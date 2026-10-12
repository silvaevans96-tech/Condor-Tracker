#include <Arduino.h>

// =========================================================
// Diagnóstico básico de led / alimentación para ESP32-S3
// - Prueba LED onboard y GPIO 13/2
// - Muestra tensión de batería (si hay divisor en ADC) o estado del sistema
// - Se usa para detectar brownout o fallo de firmware
// =========================================================

const int ledBuiltin = LED_BUILTIN;
const int led13 = 13;
const int led2 = 2;

const int batteryAdcPin = 34; // Ajusta si tu placa usa otro ADC
const float adcReference = 3.3f;
const float resistorDividerRatio = 2.0f; // si usas divisor 1:1, cambia si aplicas otro divisor

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("===========================");
  Serial.println("ESP32-S3 DIAGNOSTIC TEST");
  Serial.println("===========================");

  pinMode(ledBuiltin, OUTPUT);
  pinMode(led13, OUTPUT);
  pinMode(led2, OUTPUT);

  analogReadResolution(12);
  pinMode(batteryAdcPin, INPUT);

  Serial.println("GPIO setup OK.");
  Serial.println("LEDs will blink in cycles.");
}

void blinkPin(int pin, int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(delayMs);
    digitalWrite(pin, LOW);
    delay(delayMs);
  }
}

float readBatteryVoltage() {
  int raw = analogRead(batteryAdcPin);
  float voltage = (raw / 4095.0f) * adcReference * resistorDividerRatio;
  return voltage;
}

void loop() {
  Serial.println("--- TEST CYCLE ---");
  Serial.printf("BAT ADC raw = %d\n", analogRead(batteryAdcPin));
  Serial.printf("Battery voltage approx = %.2f V\n", readBatteryVoltage());

  Serial.println("Blinking LED_BUILTIN");
  blinkPin(ledBuiltin, 3, 200);
  delay(300);

  Serial.println("Blinking GPIO 13");
  blinkPin(led13, 3, 200);
  delay(300);

  Serial.println("Blinking GPIO 2");
  blinkPin(led2, 3, 200);
  delay(300);

  Serial.println("Waiting 2s before next cycle...\n");
  delay(2000);
}
