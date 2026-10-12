#include <Arduino.h>
#include <TinyGsmClient.h>
#include <HTTPClient.h>
#include <TinyGsmClient.h>
#include <esp_sleep.h>

// ==========================
//  Condor Tracker - Arduino (Improved)
//  ESP32-S3 + T-SIM7080G
//  - Uses TinyGSM for GPRS HTTP
//  - RTC memory preserves telemetry across deep-sleep cycles
//  - Retries, modem checks, debug (serial-only) mode
//  - Configurable wake / transmit intervals
// ==========================

// ----- Config -----
const char* DEVICE_NAME = "condor-tracker";
const bool DEBUG_MODE = false; // true => print only, no GPRS, no deep sleep

// Pins / UARTs (adjust to your wiring)
#define GNSS_RX_PIN 18
#define GNSS_TX_PIN 17
#define MODEM_RX_PIN 9
#define MODEM_TX_PIN 8
#define MODEM_PWR_KEY_PIN 10
#define MODEM_RST_PIN 11

// Timing (adjust for testing or production)
const uint32_t WAKE_INTERVAL_MS = 30UL * 1000UL;      // device wakes every 30s
const uint32_t TRANSMIT_INTERVAL_MS = 30UL * 1000UL;  // transmit after 30s (for testing)
                                                         // for production set to 5UL*60UL*60UL*1000UL
const uint32_t GPS_FIX_TIMEOUT_MS = 60UL * 1000UL;

// Network / API
const char* APN = "internet";
const char* GPRS_USER = "";
const char* GPRS_PASS = "";
const char* APP_ENDPOINT = "https://your-api.example.com/condor-data";
const char* APP_TOKEN = "replace-with-your-token";
const uint8_t TRANSMIT_RETRY_COUNT = 3;
const uint32_t TRANSMIT_RETRY_DELAY_MS = 10UL * 1000UL;

// ----- TinyGSM modem selection -----
#define TINY_GSM_MODEM_SIM7080

// ----- Hardware serials -----
HardwareSerial GNSSSerial(2); // GNSS (NMEA)
HardwareSerial ModemSerial(1); // Modem AT
TinyGsm modem(ModemSerial);
TinyGsmClient tgClient(modem);

// ----- RTC (retain across deep sleep) -----
RTC_DATA_ATTR uint32_t rtc_cycle_count = 0; // number of wake cycles accumulated
RTC_DATA_ATTR float rtc_totalDistanceKm = 0.0f;
RTC_DATA_ATTR float rtc_maxSpeedKmh = 0.0f;
RTC_DATA_ATTR float rtc_maxAltitudeM = -99999.0f;
RTC_DATA_ATTR float rtc_minAltitudeM = 99999.0f;
RTC_DATA_ATTR float rtc_lastLat = 0.0f;
RTC_DATA_ATTR float rtc_lastLon = 0.0f;
RTC_DATA_ATTR uint32_t rtc_sampleCount = 0;
RTC_DATA_ATTR uint32_t rtc_lastSampleMs = 0;

// ----- Runtime structures -----
struct LocationFix {
  float latitude = 0.0f;
  float longitude = 0.0f;
  float altitude = 0.0f;
  float speedKmh = 0.0f;
  unsigned long timestampMs = 0;
  bool valid = false;
};

// Utility: radians & haversine
float toRadians(float deg) { return deg * PI / 180.0f; }
float haversineKm(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371.0f;
  float dLat = toRadians(lat2 - lat1);
  float dLon = toRadians(lon2 - lon1);
  float a = sin(dLat/2.0f)*sin(dLat/2.0f) + cos(toRadians(lat1))*cos(toRadians(lat2))*sin(dLon/2.0f)*sin(dLon/2.0f);
  float c = 2.0f * atan2(sqrt(a), sqrt(1.0f - a));
  return R * c;
}

// ----- NMEA parsing (simple GGA/RMC) -----
float parseLatitude(String value, String hemi) {
  if (value.length() < 4) return 0.0f;
  float deg = value.substring(0,2).toFloat();
  float minutes = value.substring(2).toFloat() / 60.0f;
  float res = deg + minutes;
  if (hemi == "S") res *= -1.0f;
  return res;
}
float parseLongitude(String value, String hemi) {
  if (value.length() < 5) return 0.0f;
  float deg = value.substring(0,3).toFloat();
  float minutes = value.substring(3).toFloat() / 60.0f;
  float res = deg + minutes;
  if (hemi == "W") res *= -1.0f;
  return res;
}

bool parseGga(const String &line, LocationFix &fix) {
  if (!line.startsWith("$GPGGA") && !line.startsWith("$GNGGA")) return false;
  char buf[256];
  line.toCharArray(buf, sizeof(buf));
  char *tok = strtok(buf, ",");
  String f[16]; int i=0;
  while (tok && i < 16) { f[i++] = String(tok); tok = strtok(NULL, ","); }
  if (i < 10) return false;
  if (f[2].length() && f[3].length() && f[4].length() && f[5].length()) {
    fix.latitude = parseLatitude(f[2], f[3]);
    fix.longitude = parseLongitude(f[4], f[5]);
    fix.altitude = f[9].toFloat();
    fix.timestampMs = millis();
    fix.valid = (fix.latitude!=0.0f || fix.longitude!=0.0f);
    return true;
  }
  return false;
}

bool parseRmc(const String &line, LocationFix &fix) {
  if (!line.startsWith("$GPRMC") && !line.startsWith("$GNRMC")) return false;
  char buf[256];
  line.toCharArray(buf, sizeof(buf));
  char *tok = strtok(buf, ",");
  String f[16]; int i=0;
  while (tok && i < 16) { f[i++] = String(tok); tok = strtok(NULL, ","); }
  if (i < 8) return false;
  if (f[3].length() && f[4].length() && f[5].length() && f[6].length()) {
    fix.latitude = parseLatitude(f[3], f[4]);
    fix.longitude = parseLongitude(f[5], f[6]);
    fix.speedKmh = f[7].toFloat() * 1.852f;
    fix.timestampMs = millis();
    fix.valid = (fix.latitude!=0.0f || fix.longitude!=0.0f);
    return true;
  }
  return false;
}

bool readGnssFix(LocationFix &fix) {
  uint32_t start = millis();
  while ((millis() - start) < GPS_FIX_TIMEOUT_MS) {
    while (GNSSSerial.available()) {
      String line = GNSSSerial.readStringUntil('\n');
      line.trim(); if (line.length()==0) continue;
      LocationFix cand;
      if (parseGga(line, cand) || parseRmc(line, cand)) {
        if (cand.valid) { fix = cand; return true; }
      }
    }
    delay(20);
  }
  return false;
}

// ----- Telemetry helpers using RTC variables -----
void rtc_init_if_needed() {
  // rtc_cycle_count preserved; if first boot (unknown), leave as is (defaults zero)
}

void rtc_update_with_fix(const LocationFix &fix) {
  if (!fix.valid) return;
  if (rtc_sampleCount == 0) {
    rtc_lastLat = fix.latitude;
    rtc_lastLon = fix.longitude;
    rtc_lastSampleMs = fix.timestampMs;
  } else {
    float d = haversineKm(rtc_lastLat, rtc_lastLon, fix.latitude, fix.longitude);
    rtc_totalDistanceKm += d;
    if (d > 0.0f) {
      float elapsed = max(1.0f, (fix.timestampMs - rtc_lastSampleMs) / 1000.0f);
      float speed = (d / elapsed) * 3600.0f;
      if (speed > rtc_maxSpeedKmh) rtc_maxSpeedKmh = speed;
    }
    rtc_lastLat = fix.latitude;
    rtc_lastLon = fix.longitude;
    rtc_lastSampleMs = fix.timestampMs;
  }
  if (fix.altitude > rtc_maxAltitudeM) rtc_maxAltitudeM = fix.altitude;
  if (fix.altitude < rtc_minAltitudeM) rtc_minAltitudeM = fix.altitude;
  rtc_sampleCount++;
}

void rtc_reset() {
  rtc_cycle_count = 0;
  rtc_totalDistanceKm = 0.0f;
  rtc_maxSpeedKmh = 0.0f;
  rtc_maxAltitudeM = -99999.0f;
  rtc_minAltitudeM = 99999.0f;
  rtc_lastLat = 0.0f;
  rtc_lastLon = 0.0f;
  rtc_sampleCount = 0;
  rtc_lastSampleMs = 0;
}

String buildJsonPayloadFromRTC() {
  String payload = "{";
  payload += "\"device\":\"" + String(DEVICE_NAME) + "\",";
  payload += "\"timestamp\":\"" + String(millis()) + "\",";
  payload += "\"latitude\":" + String(rtc_lastLat, 6) + ",";
  payload += "\"longitude\":" + String(rtc_lastLon, 6) + ",";
  payload += "\"altitude\":" + String(rtc_maxAltitudeM, 2) + ",";
  payload += "\"distance_km\":" + String(rtc_totalDistanceKm, 3) + ",";
  payload += "\"max_speed_kmh\":" + String(rtc_maxSpeedKmh, 3) + ",";
  payload += "\"max_altitude_m\":" + String(rtc_maxAltitudeM, 2) + ",";
  payload += "\"min_altitude_m\":" + String(rtc_minAltitudeM, 2) + ",";
  payload += "\"samples\":" + String(rtc_sampleCount) + "}";
  return payload;
}

// ----- Modem / network routines (TinyGSM) -----
bool powerModemOn() {
  pinMode(MODEM_PWR_KEY_PIN, OUTPUT);
  pinMode(MODEM_RST_PIN, OUTPUT);
  // Basic sequence: ensure pins HIGH, pulse PWRKEY if needed
  digitalWrite(MODEM_PWR_KEY_PIN, LOW);
  delay(100);
  digitalWrite(MODEM_PWR_KEY_PIN, HIGH);
  delay(100);
  // leave reset HIGH
  digitalWrite(MODEM_RST_PIN, HIGH);
  delay(200);
  return true;
}

bool initModem() {
  if (DEBUG_MODE) return true;
  powerModemOn();
  ModemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(2000);
  modem.restart();
  // check SIM
  String sim = modem.getSimStatus();
  // getSimStatus may return numeric; just log
  Serial.print("[MODEM] SIM status: "); Serial.println(sim);
  return true;
}

bool connectGprsTinyGsm() {
  if (DEBUG_MODE) return true;
  // wait for network
  if (!modem.waitForNetwork(60000L)) {
    Serial.println("[MODEM] Network not available");
    return false;
  }
  Serial.println("[MODEM] Network OK");
  if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
    Serial.println("[GPRS] Connect failed");
    return false;
  }
  Serial.println("[GPRS] Connected");
  return true;
}

bool sendPayloadViaGprsWithRetries(const String &payload) {
  if (DEBUG_MODE) {
    Serial.println("[DEBUG] Payload:\n" + payload);
    return true;
  }

  for (uint8_t attempt=0; attempt<TRANSMIT_RETRY_COUNT; ++attempt) {
    Serial.printf("[HTTP] Attempt %u/%u\n", attempt+1, TRANSMIT_RETRY_COUNT);
    if (!connectGprsTinyGsm()) {
      Serial.println("[HTTP] GPRS connection failed");
    } else {
      // Use TinyGsmClient
      TinyGsmClient client(modem);
      HTTPClient http;
      if (!http.begin(client, APP_ENDPOINT)) {
        Serial.println("[HTTP] begin failed");
      } else {
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Authorization", String("Bearer ") + String(APP_TOKEN));
        int code = http.POST((uint8_t*)payload.c_str(), payload.length());
        String resp = http.getString();
        Serial.printf("[HTTP] code=%d\n", code);
        if (code >= 200 && code < 300) {
          Serial.println("[HTTP] Success: " + resp);
          http.end();
          modem.gprsDisconnect();
          return true;
        } else {
          Serial.println("[HTTP] Failed: " + resp);
        }
        http.end();
      }
      modem.gprsDisconnect();
    }

    if (attempt < TRANSMIT_RETRY_COUNT-1) {
      Serial.printf("[HTTP] Retry in %u ms\n", TRANSMIT_RETRY_DELAY_MS);
      delay(TRANSMIT_RETRY_DELAY_MS);
    }
  }
  Serial.println("[HTTP] All attempts failed");
  return false;
}

// ----- Main flow -----
void doWakeCycle() {
  // Initialize GNSS serial and read one fix
  GNSSSerial.begin(115200, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);
  delay(500);
  LocationFix fix;
  bool got = readGnssFix(fix);
  if (got) {
    Serial.printf("[GPS] lat=%.6f lon=%.6f alt=%.2fm speed=%.2fkm/h\n", fix.latitude, fix.longitude, fix.altitude, fix.speedKmh);
    rtc_update_with_fix(fix);
  } else {
    Serial.println("[GPS] No fix this wakeup");
  }

  rtc_cycle_count++;
  // compute how many cycles needed to reach transmit window
  uint32_t cycles_needed = max(1UL, TRANSMIT_INTERVAL_MS / WAKE_INTERVAL_MS);

  Serial.printf("[RTC] cycle=%u / needed=%u samples=%u\n", rtc_cycle_count, cycles_needed, rtc_sampleCount);

  if (rtc_cycle_count >= cycles_needed && rtc_sampleCount > 0) {
    // Build payload from RTC and transmit
    String payload = buildJsonPayloadFromRTC();
    bool ok = sendPayloadViaGprsWithRetries(payload);
    if (ok) {
      Serial.println("[APP] Transmit OK, resetting RTC telemetry");
      rtc_reset();
    } else {
      Serial.println("[APP] Transmit failed, keeping RTC state for next attempt");
      // Do not reset rtc_cycle_count so next wake will try again (or you may keep count)
    }
  }
}

void prepareDeepSleepOrWait() {
  if (DEBUG_MODE) {
    Serial.println("[POWER] DEBUG_MODE - not entering deep sleep");
    delay(WAKE_INTERVAL_MS);
    return;
  }
  Serial.printf("[SLEEP] Sleeping for %lu ms\n", WAKE_INTERVAL_MS);
  esp_sleep_enable_timer_wakeup((uint64_t)WAKE_INTERVAL_MS * 1000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== CONDOR TRACKER (improved) ===");

  // Optional: init modem serial early for logs
  ModemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(200);

  rtc_init_if_needed();

  if (!DEBUG_MODE) initModem();

  doWakeCycle();

  prepareDeepSleepOrWait();
}

void loop() {
  // not used; device sleeps in setup
  delay(1000);
}
