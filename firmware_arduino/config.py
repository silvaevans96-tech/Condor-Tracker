"""Condor Tracker - Configuration

Main configuration file for the Condor Tracker device.
Adjust these values to match your hardware setup and API endpoint.
"""

# ========== DEVICE CONFIGURATION ==========
DEVICE_NAME = "condor-tracker-01"
DEVICE_MODEL = "ESP32-S3 + T-SIM7080G"

# ========== GPS/GNSS CONFIGURATION ==========
GNSS_UART_PORT = 1  # UART port for GNSS module
GNSS_BAUD_RATE = 115200
GNSS_RX_PIN = 18
GNSS_TX_PIN = 17
GNSS_SAMPLE_INTERVAL_SECONDS = 30  # Take GPS sample every 30 seconds
GNSS_FIX_TIMEOUT_SECONDS = 60  # Maximum time to wait for GPS fix

# ========== GSM/MODEM CONFIGURATION ==========
MODEM_UART_PORT = 2  # UART port for modem
MODEM_BAUD_RATE = 115200
MODEM_RX_PIN = 9
MODEM_TX_PIN = 8
MODEM_PWR_KEY_PIN = 10
MODEM_RST_PIN = 11

# Network settings
APN = "internet"  # Replace with your carrier APN
GPRS_USER = ""  # Leave empty if not required
GPRS_PASS = ""  # Leave empty if not required

# ========== API ENDPOINT ==========
APP_ENDPOINT = "https://your-api.example.com/condor-data"
APP_AUTH_TOKEN = "replace-with-your-token"

# ========== TRANSMISSION SETTINGS ==========
TRANSMIT_INTERVAL_SECONDS = 5 * 60 * 60  # 5 hours = 18,000 seconds
TRANSMIT_RETRY_COUNT = 3  # Number of retries if transmission fails
TRANSMIT_RETRY_DELAY_SECONDS = 10  # Delay between retries

# ========== FLIGHT PARAMETERS ==========
MIN_VALID_ALTITUDE_M = -100.0
MAX_VALID_ALTITUDE_M = 12000.0
MIN_VALID_SPEED_KMH = 0.0
MAX_VALID_SPEED_KMH = 200.0

# ========== POWER MANAGEMENT ==========
ENABLE_DEEP_SLEEP = True
DEEP_SLEEP_AFTER_TRANSMIT = True

# ========== LOGGING & DEBUG ==========
DEBUG_MODE = True
LOG_TO_SERIAL = True
LOG_TO_FILE = False
LOG_FILE_PATH = "/logs/condor-tracker.log"

# ========== BATTERY MONITORING ==========
BATTERY_ADC_PIN = 35
BATTERY_VOLTAGE_MAX = 4.2  # Maximum voltage for LiPo
BATTERY_VOLTAGE_MIN = 2.8  # Minimum voltage for LiPo
