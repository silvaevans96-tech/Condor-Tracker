"""Configuration values for the Condor Tracker device."""

from __future__ import annotations

# Flight tracking settings
GPS_SAMPLE_INTERVAL_SECONDS = 10
TRANSMISSION_INTERVAL_SECONDS = 5 * 60 * 60  # 18,000 seconds = 5 hours
GPS_FIX_TIMEOUT_SECONDS = 60
MAX_TRACKING_SECONDS = TRANSMISSION_INTERVAL_SECONDS

# Device configuration
DEVICE_NAME = "condor-tracker"
APP_ENDPOINT = "https://your-api.example.com/condor-data"
APP_AUTH_TOKEN = "replace-with-token"

# GPS UART configuration for ESP32-S3 + T-SIM7080G
GPS_UART_ID = 1
GPS_UART_BAUD = 115200
GPS_RX_PIN = 18
GPS_TX_PIN = 17

# Energy optimization
GO_TO_DEEP_SLEEP_AFTER_TRANSMIT = True
DEEP_SLEEP_WAKEUP_REASON = "timer"

# Data validity
MIN_VALID_ALTITUDE_METERS = -100.0
MAX_VALID_ALTITUDE_METERS = 10000.0

# Simulation/testing mode
USE_SIMULATED_GPS = True
SIMULATION_START_LATITUDE = 40.416775
SIMULATION_START_LONGITUDE = -3.703790
SIMULATION_START_ALTITUDE = 1300.0
SIMULATION_SPEED_KMH = 65.0

# Logging
LOG_TO_SERIAL = True
LOG_TO_FILE = False
