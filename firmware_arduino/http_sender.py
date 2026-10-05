"""Condor Tracker - HTTP/MQTT Transmission

Handles sending flight data to the API endpoint.
"""

import json
import time

try:
    import urequests as requests
    import network
    MICROPYTHON = True
except ImportError:
    import requests
    MICROPYTHON = False

from config import (
    APP_ENDPOINT,
    APP_AUTH_TOKEN,
    TRANSMIT_RETRY_COUNT,
    TRANSMIT_RETRY_DELAY_SECONDS,
    DEBUG_MODE,
)


class HTTPSender:
    """Sends flight data via HTTP POST."""

    def __init__(self, debug=DEBUG_MODE):
        self.debug = debug
        self.endpoint = APP_ENDPOINT
        self.token = APP_AUTH_TOKEN

    def connect_network(self):
        """Connect to network if using MicroPython."""
        if not MICROPYTHON:
            return True

        try:
            wlan = network.WLAN(network.STA_IF)
            if not wlan.isconnected():
                if self.debug:
                    print("[NET] Connecting to WiFi...")
                return False  # GSM/LTE connection handled by modem
            return True
        except Exception as e:
            if self.debug:
                print(f"[NET] Connection check failed: {e}")
            return False

    def send(self, payload, retry_count=TRANSMIT_RETRY_COUNT):
        """Send payload to API endpoint with retries."""
        if not payload:
            if self.debug:
                print("[HTTP] Empty payload, skipping")
            return False

        for attempt in range(retry_count):
            try:
                if self.debug:
                    print(
                        f"[HTTP] Attempt {attempt + 1}/{retry_count} to send to {self.endpoint}"
                    )

                headers = {
                    "Content-Type": "application/json",
                    "Authorization": f"Bearer {self.token}",
                }

                response = requests.post(
                    self.endpoint,
                    data=payload,
                    headers=headers,
                    timeout=30,
                )

                if response.status_code in (200, 201, 204):
                    if self.debug:
                        print(
                            f"[HTTP] Success (status {response.status_code}): {response.text}"
                        )
                    response.close()
                    return True
                else:
                    if self.debug:
                        print(
                            f"[HTTP] Failed (status {response.status_code}): {response.text}"
                        )
                    response.close()

            except Exception as e:
                if self.debug:
                    print(f"[HTTP] Exception: {e}")

            if attempt < retry_count - 1:
                if self.debug:
                    print(
                        f"[HTTP] Retrying in {TRANSMIT_RETRY_DELAY_SECONDS} seconds..."
                    )
                time.sleep(TRANSMIT_RETRY_DELAY_SECONDS)

        if self.debug:
            print("[HTTP] All transmission attempts failed")
        return False


class MockSender:
    """Mock sender for testing without network."""

    def __init__(self, debug=True):
        self.debug = debug

    def send(self, payload):
        if self.debug:
            print(f"[MOCK] Would send: {payload}")
        return True
