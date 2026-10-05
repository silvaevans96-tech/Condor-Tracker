#pragma once

#include <Arduino.h>

// ---------- Device ----------
constexpr uint32_t GPS_SAMPLE_INTERVAL_MS = 30000UL;
constexpr uint32_t RADIO_WAKEUP_MS = 30000UL;
constexpr uint32_t TRANSMIT_INTERVAL_MS = 5UL * 60UL * 60UL * 1000UL; // 5 hours
constexpr uint32_t GPS_FIX_TIMEOUT_MS = 60000UL;

// ---------- UART ----------
constexpr int GNSS_RX_PIN = 18;
constexpr int GNSS_TX_PIN = 17;
constexpr int MODEM_RX_PIN = 9;
constexpr int MODEM_TX_PIN = 8;
constexpr int MODEM_PWR_KEY_PIN = 10;
constexpr int MODEM_RST_PIN = 11;

// ---------- Modem network ----------
constexpr char APN[] = "internet";
constexpr char GPRS_USER[] = "";
constexpr char GPRS_PASS[] = "";
constexpr char APP_ENDPOINT[] = "https://your-api.example.com/condor-data";
constexpr char APP_TOKEN[] = "replace-with-token";

// ---------- Flight ----------
constexpr float MIN_ALTITUDE_M = -100.0f;
constexpr float MAX_ALTITUDE_M = 12000.0f;

// ---------- Power ----------
constexpr bool ENABLE_DEEP_SLEEP = true;
