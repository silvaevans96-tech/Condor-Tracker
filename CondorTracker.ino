#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_sleep.h>

// ==========================
//  Condor Tracker - Arduino
//  ESP32-S3 + T-SIM7080G
//  Medición: altitud, distancia, velocidad máxima
//  Envío cada 5 horas y deep sleep
// ==========================

const char* DEVICE_NAME = "condor-tracker";

// Configuración de UART para GNSS y modem
#define GNSS_RX_PIN 18
#define GNSS_TX_PIN 17
#define MODEM_RX_PIN 9
#define MODEM_TX_PIN 8
#define MODEM_PWR_KEY_PIN 10
#define MODEM_RST_PIN 11

const uint32_t GPS_SAMPLE_INTERVAL_MS = 30000;
const uint32_t GPS_FIX_TIMEOUT_MS = 60000;
const uint32_t TRANSMIT_INTERVAL_MS = 5UL * 60UL * 60UL * 1000UL; // 5 horas

const char* APN = "internet";
const char* GPRS_USER = "";
const char* GPRS_PASS = "";
const char* APP_ENDPOINT = "https://your-api.example.com/condor-data";
const char* APP_TOKEN = "replace-with-your-token";

// Variables globales
HardwareSerial GNSSSerial(2);
HardwareSerial MODEMSerial(1);

struct LocationFix {
  float latitude = 0.0f;
  float longitude = 0.0f;
  float altitude = 0.0f;
  float speedKmh = 0.0f;
  unsigned long timestampMs = 0;
  bool valid = false;
};

struct FlightStats {
  float totalDistanceKm = 0.0f;
  float maxSpeedKmh = 0.0f;
  float maxAltitudeM = -99999.0f;
  float minAltitudeM = 99999.0f;
  uint32_t sampleCount = 0;
  uint32_t lastSampleMs = 0;
  LocationFix lastFix;
};

FlightStats telemetry;
LocationFix currentFix;

float toRadians(float degrees) {
  return degrees * PI / 180.0f;
}

float haversineKm(float lat1, float lon1, float lat2, float lon2) {
  const float earthRadiusKm = 6371.0f;
  float dLat = toRadians(lat2 - lat1);
  float dLon = toRadians(lon2 - lon1);

  float a = sin(dLat / 2.0f) * sin(dLat / 2.0f) +
            cos(toRadians(lat1)) * cos(toRadians(lat2)) *
            sin(dLon / 2.0f) * sin(dLon / 2.0f);

  float c = 2.0f * atan2(sqrt(a), sqrt(1.0f - a));
  return earthRadiusKm * c;
}

float parseLatitude(String value, String hemi) {
  if (value.length() < 4) return 0.0f;
  float deg = value.substring(0, 2).toFloat();
  float minutes = value.substring(2).toFloat() / 60.0f;
  float result = deg + minutes;
  if (hemi == "S") result *= -1.0f;
  return result;
}

float parseLongitude(String value, String hemi) {
  if (value.length() < 5) return 0.0f;
  float deg = value.substring(0, 3).toFloat();
  float minutes = value.substring(3).toFloat() / 60.0f;
  float result = deg + minutes;
  if (hemi == "W") result *= -1.0f;
  return result;
}

bool parseGga(const String& line, LocationFix& fix) {
  if (!line.startsWith("$GPGGA") && !line.startsWith("$GNGGA")) return false;

  char buffer[256];
  line.toCharArray(buffer, sizeof(buffer));
  char* token = strtok(buffer, ",");
  if (!token) return false;

  String values[16];
  int index = 0;
  while (token && index < 16) {
    values[index++] = String(token);
    token = strtok(nullptr, ",");
  }

  if (index < 10) return false;

  if (values[2].length() > 0 && values[3].length() > 0 && values[4].length() > 0 && values[5].length() > 0) {
    fix.latitude = parseLatitude(values[2], values[3]);
    fix.longitude = parseLongitude(values[4], values[5]);
    fix.altitude = values[9].toFloat();
    fix.timestampMs = millis();
    fix.valid = (fix.latitude != 0.0f || fix.longitude != 0.0f);
    return true;
  }

  return false;
}

bool parseRmc(const String& line, LocationFix& fix) {
  if (!line.startsWith("$GPRMC") && !line.startsWith("$GNRMC")) return false;

  char buffer[256];
  line.toCharArray(buffer, sizeof(buffer));
  char* token = strtok(buffer, ",");
  if (!token) return false;

  String values[16];
  int index = 0;
  while (token && index < 16) {
    values[index++] = String(token);
    token = strtok(nullptr, ",");
  }

  if (index < 8) return false;

  if (values[3].length() > 0 && values[4].length() > 0 && values[5].length() > 0 && values[6].length() > 0 && values[7].length() > 0) {
    fix.latitude = parseLatitude(values[3], values[4]);
    fix.longitude = parseLongitude(values[5], values[6]);
    fix.speedKmh = values[7].toFloat() * 1.852f;
    fix.timestampMs = millis();
    fix.valid = (fix.latitude != 0.0f || fix.longitude != 0.0f);
    return true;
  }

  return false;
}

bool readGnssFix(LocationFix& fix) {
  uint32_t startMs = millis();

  while ((millis() - startMs) < GPS_FIX_TIMEOUT_MS) {
    while (GNSSSerial.available()) {
      String line = GNSSSerial.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;

      LocationFix candidate;
      if (parseGga(line, candidate) || parseRmc(line, candidate)) {
        if (candidate.valid) {
          fix = candidate;
          return true;
        }
      }
    }
    delay(20);
  }

  return false;
}

void updateFlightStats(const LocationFix& fix) {
  if (!fix.valid) return;

  if (telemetry.sampleCount == 0) {
    telemetry.lastFix = fix;
    telemetry.lastSampleMs = fix.timestampMs;
  } else {
    float distanceKm = haversineKm(
      telemetry.lastFix.latitude,
      telemetry.lastFix.longitude,
      fix.latitude,
      fix.longitude
    );

    telemetry.totalDistanceKm += distanceKm;

    if (distanceKm > 0.0f) {
      float elapsedSeconds = max(1.0f, (float)(fix.timestampMs - telemetry.lastSampleMs) / 1000.0f);
      float speedKmh = (distanceKm / elapsedSeconds) * 3600.0f;
      telemetry.maxSpeedKmh = max(telemetry.maxSpeedKmh, speedKmh);
    }

    telemetry.lastFix = fix;
    telemetry.lastSampleMs = fix.timestampMs;
  }

  telemetry.maxAltitudeM = max(telemetry.maxAltitudeM, fix.altitude);
  telemetry.minAltitudeM = min(telemetry.minAltitudeM, fix.altitude);
  telemetry.sampleCount++;
  currentFix = fix;
}

String buildJsonPayload() {
  String payload = "{";
  payload += "\"device\":\"" + String(DEVICE_NAME) + "\",";
  payload += "\"timestamp\":\"" + String(millis()) + "\",";
  payload += "\"latitude\":" + String(currentFix.latitude, 6) + ",";
  payload += "\"longitude\":" + String(currentFix.longitude, 6) + ",";
  payload += "\"altitude\":" + String(currentFix.altitude, 2) + ",";
  payload += "\"distance_km\":" + String(telemetry.totalDistanceKm, 3) + ",";
  payload += "\"max_speed_kmh\":" + String(telemetry.maxSpeedKmh, 3) + ",";
  payload += "\"max_altitude_m\":" + String(telemetry.maxAltitudeM, 2) + ",";
  payload += "\"min_altitude_m\":" + String(telemetry.minAltitudeM, 2) + ",";
  payload += "\"samples\":" + String(telemetry.sampleCount) + "}";
  return payload;
}

bool initModem() {
  pinMode(MODEM_PWR_KEY_PIN, OUTPUT);
  pinMode(MODEM_RST_PIN, OUTPUT);

  digitalWrite(MODEM_PWR_KEY_PIN, HIGH);
  digitalWrite(MODEM_RST_PIN, HIGH);
  delay(2000);

  MODEMSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(2000);

  MODEMSerial.println("AT");
  delay(500);

  String response = MODEMSerial.readString();
  if (response.indexOf("OK") == -1) {
    Serial.println("[MODEM] No response from T-SIM7080G");
    return false;
  }

  Serial.println("[MODEM] T-SIM7080G ready");
  return true;
}

bool connectGprs() {
  MODEMSerial.println("AT+CIMI");
  delay(500);
  Serial.println(MODEMSerial.readString());

  MODEMSerial.println("AT+CGATT=1");
  delay(1500);
  Serial.println(MODEMSerial.readString());

  MODEMSerial.println("AT+CGDCONT=1,\"IP\",\"" + String(APN) + "\"");
  delay(1500);
  Serial.println(MODEMSerial.readString());

  MODEMSerial.println("AT+CGACT=1,1");
  delay(3000);
  String resp = MODEMSerial.readString();
  Serial.println(resp);

  if (resp.indexOf("OK") == -1) {
    Serial.println("[GSM] PDP context not activated");
    return false;
  }

  Serial.println("[GSM] GPRS connected");
  return true;
}

bool transmitPayload() {
  if (!connectGprs()) {
    return false;
  }

  String payload = buildJsonPayload();
  Serial.println("[HTTP] Sending payload");
  Serial.println(payload);

  WiFiClient client;
  HTTPClient http;

  if (!http.begin(client, APP_ENDPOINT)) {
    Serial.println("[HTTP] Unable to begin connection");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(APP_TOKEN));

  int httpCode = http.POST(payload);
  String response = http.getString();
  Serial.printf("[HTTP] HTTP code: %d\n", httpCode);
  Serial.println(response);

  http.end();
  return (httpCode >= 200 && httpCode < 300);
}

void deepSleepForFiveHours() {
  Serial.println("[SLEEP] Entering deep sleep for 5 hours");
  esp_sleep_enable_timer_wakeup((uint64_t)TRANSMIT_INTERVAL_MS * 1000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== CONDOR TRACKER ===");

  GNSSSerial.begin(115200, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);
  delay(1000);

  LocationFix gpsFix;
  if (readGnssFix(gpsFix)) {
    Serial.printf("[GPS] Lat: %.6f, Lon: %.6f, Alt: %.2f m, Speed: %.2f km/h\n",
                  gpsFix.latitude,
                  gpsFix.longitude,
                  gpsFix.altitude,
                  gpsFix.speedKmh);
    updateFlightStats(gpsFix);
  } else {
    Serial.println("[GPS] No valid fix read");
  }

  if (telemetry.sampleCount > 0) {
    if (initModem()) {
      bool ok = transmitPayload();
      Serial.println(ok ? "[APP] Payload transmitted" : "[APP] Transmission failed");
    }
  }

  deepSleepForFiveHours();
}

void loop() {
  // The device goes to sleep in setup(), so loop is not used.
  delay(1000);
}
