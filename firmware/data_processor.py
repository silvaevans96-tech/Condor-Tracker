"""GPS acquisition logic for the Condor Tracker project.

This module is designed to work with a T-SIM7080G GNSS/GPS stream on ESP32-S3,
while still allowing a desktop simulation environment for testing.
"""

from __future__ import annotations

import math
import time

try:
    import machine  # type: ignore
except ImportError:  # pragma: no cover - used in desktop tests
    machine = None  # type: ignore


class GPSReader:
    """Reads and parses NMEA sentences from the GNSS module."""

    def __init__(self, uart=None, use_simulator=True, debug=False):
        self.uart = uart
        self.use_simulator = use_simulator
        self.debug = debug
        self.last_fix = None
        self.last_update = None

    def start(self):
        """Initialize the GPS receiver."""
        if self.debug:
            print("[GPS] Initializing GNSS module")

        if self.use_simulator:
            self.last_fix = {
                "latitude": 40.416775,
                "longitude": -3.703790,
                "altitude": 1300.0,
                "speed_kmh": 0.0,
                "timestamp": time.time(),
            }
            self.last_update = time.time()
            return True

        if machine is not None and self.uart is not None:
            self.uart.init(baudrate=115200, bits=8, parity=None, stop=1)
            return True

        return False

    def read_fix(self):
        """Return a valid GPS fix dictionary or None if unavailable."""
        if self.use_simulator:
            return self._simulate_fix()

        if self.uart is None:
            return None

        sentence = self._read_nmea_sentence()
        if not sentence:
            return None

        parsed = self._parse_nmea_sentence(sentence)
        if parsed is None:
            return None

        self.last_fix = parsed
        self.last_update = time.time()
        return parsed

    def _simulate_fix(self):
        """Generate a deterministic GPS sample for testing without hardware."""
        base_lat = 40.416775
        base_lon = -3.703790
        base_alt = 1300.0

        now = time.time()
        seconds = now - self.last_update if self.last_update is not None else 0.0

        # A small drift simulating bird flight.
        lat_offset = math.sin(now / 120.0) * 0.00025
        lon_offset = math.cos(now / 150.0) * 0.00030
        alt_offset = math.sin(now / 90.0) * 12.0

        sample = {
            "latitude": base_lat + lat_offset,
            "longitude": base_lon + lon_offset,
            "altitude": base_alt + alt_offset,
            "speed_kmh": 58.0 + (math.sin(now / 60.0) * 15.0),
            "timestamp": now,
        }

        self.last_fix = sample
        self.last_update = now
        return sample

    def _read_nmea_sentence(self):
        """Reads a single NMEA line from UART."""
        if self.uart is None:
            return None

        try:
            data = self.uart.readline()
        except Exception:  # pragma: no cover - hardware dependent
            return None

        if data is None:
            return None

        if isinstance(data, bytes):
            try:
                return data.decode("ascii", "ignore").strip()
            except Exception:
                return None

        return str(data).strip()

    def _parse_nmea_sentence(self, sentence):
        """Parses GGA/RMC style NMEA sentences.

        Supported fields:
        - $GPGGA: time, latitude, longitude, altitude
        - $GPRMC: speed over ground in knots
        """
        if not sentence or not sentence.startswith("$"):
            return None

        parts = sentence.split(",")
        if len(parts) < 2:
            return None

        if parts[0] in ("$GPGGA", "$GNGGA"):
            if len(parts) < 9:
                return None

            lat = self._parse_latitude(parts[2], parts[3])
            lon = self._parse_longitude(parts[4], parts[5])
            alt = self._parse_float(parts[9])
            if lat is None or lon is None:
                return None

            return {
                "latitude": lat,
                "longitude": lon,
                "altitude": alt,
                "speed_kmh": 0.0,
                "timestamp": time.time(),
            }

        if parts[0] in ("$GPRMC", "$GNRMC"):
            if len(parts) < 8:
                return None

            lat = self._parse_latitude(parts[3], parts[4])
            lon = self._parse_longitude(parts[5], parts[6])
            speed_knots = self._parse_float(parts[7])
            if lat is None or lon is None:
                return None

            return {
                "latitude": lat,
                "longitude": lon,
                "altitude": 0.0,
                "speed_kmh": speed_knots * 1.852,
                "timestamp": time.time(),
            }

        return None

    @staticmethod
    def _parse_latitude(value, hemisphere):
        if not value or not hemisphere:
            return None
        try:
            degrees = float(value[:2])
            minutes = float(value[2:]) / 60.0
            decimal = degrees + minutes
            if hemisphere.upper() == "S":
                decimal *= -1.0
            return decimal
        except (TypeError, ValueError):
            return None

    @staticmethod
    def _parse_longitude(value, hemisphere):
        if not value or not hemisphere:
            return None
        try:
            degrees = float(value[:3])
            minutes = float(value[3:]) / 60.0
            decimal = degrees + minutes
            if hemisphere.upper() == "W":
                decimal *= -1.0
            return decimal
        except (TypeError, ValueError):
            return None

    @staticmethod
    def _parse_float(value):
        try:
            return float(value)
        except (TypeError, ValueError):
            return 0.0
