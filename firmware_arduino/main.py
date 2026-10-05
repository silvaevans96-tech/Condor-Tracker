"""Condor Tracker - Main Flight Tracking Logic

This module handles the core flight tracking functionality including:
- GPS acquisition and parsing
- Flight statistics computation (distance, speed, altitude)
- Payload preparation
- Deep sleep management
"""

import time
import math
import json
from datetime import datetime

try:
    import machine
    import esp_sleep
    MICROPYTHON = True
except ImportError:
    MICROPYTHON = False

from config import (
    DEVICE_NAME,
    GNSS_SAMPLE_INTERVAL_SECONDS,
    TRANSMIT_INTERVAL_SECONDS,
    DEBUG_MODE,
    ENABLE_DEEP_SLEEP,
)


class LocationFix:
    """Represents a single GPS location fix."""

    def __init__(self):
        self.latitude = 0.0
        self.longitude = 0.0
        self.altitude = 0.0
        self.speed_kmh = 0.0
        self.timestamp = None
        self.valid = False


class FlightTelemetry:
    """Tracks flight statistics over a transmission window."""

    def __init__(self):
        self.total_distance_km = 0.0
        self.max_speed_kmh = 0.0
        self.max_altitude_m = -99999.0
        self.min_altitude_m = 99999.0
        self.sample_count = 0
        self.last_sample_time = None
        self.last_fix = None
        self.start_time = None

    def reset(self):
        """Reset telemetry for a new tracking session."""
        self.total_distance_km = 0.0
        self.max_speed_kmh = 0.0
        self.max_altitude_m = -99999.0
        self.min_altitude_m = 99999.0
        self.sample_count = 0
        self.last_sample_time = None
        self.last_fix = None
        self.start_time = time.time()

    def update(self, fix):
        """Update telemetry with a new GPS fix."""
        if not fix.valid:
            return

        if self.sample_count == 0:
            self.last_fix = fix
            self.last_sample_time = time.time()
            self.start_time = self.last_sample_time
        else:
            now = time.time()
            elapsed_seconds = max(1.0, now - self.last_sample_time)
            distance_km = self._haversine_distance(
                self.last_fix.latitude,
                self.last_fix.longitude,
                fix.latitude,
                fix.longitude,
            )

            self.total_distance_km += distance_km

            if distance_km > 0.0:
                speed_kmh = (distance_km / elapsed_seconds) * 3600.0
                self.max_speed_kmh = max(self.max_speed_kmh, speed_kmh)

            self.last_fix = fix
            self.last_sample_time = now

        self.max_altitude_m = max(self.max_altitude_m, fix.altitude)
        self.min_altitude_m = min(self.min_altitude_m, fix.altitude)
        self.sample_count += 1

    def get_payload(self, current_fix):
        """Generate JSON payload for transmission."""
        payload = {
            "device": DEVICE_NAME,
            "timestamp": datetime.utcnow().isoformat() + "Z",
            "latitude": round(current_fix.latitude, 6),
            "longitude": round(current_fix.longitude, 6),
            "altitude": round(current_fix.altitude, 2),
            "distance_km": round(self.total_distance_km, 3),
            "max_speed_kmh": round(self.max_speed_kmh, 3),
            "max_altitude_m": round(self.max_altitude_m, 2),
            "min_altitude_m": round(self.min_altitude_m, 2),
            "samples": self.sample_count,
        }
        return json.dumps(payload)

    @staticmethod
    def _haversine_distance(lat1, lon1, lat2, lon2):
        """Calculate distance between two coordinates using haversine formula."""
        earth_radius_km = 6371.0
        lat1_rad = math.radians(lat1)
        lon1_rad = math.radians(lon1)
        lat2_rad = math.radians(lat2)
        lon2_rad = math.radians(lon2)

        delta_lat = lat2_rad - lat1_rad
        delta_lon = lon2_rad - lon1_rad

        a = (
            math.sin(delta_lat / 2.0) ** 2
            + math.cos(lat1_rad)
            * math.cos(lat2_rad)
            * math.sin(delta_lon / 2.0) ** 2
        )
        c = 2.0 * math.atan2(math.sqrt(a), math.sqrt(1.0 - a))
        return earth_radius_km * c


class GNSSReader:
    """Reads and parses NMEA sentences from GNSS module."""

    def __init__(self, uart=None, debug=False):
        self.uart = uart
        self.debug = debug
        self.last_fix = None

    def read_fix(self):
        """Read a single GPS fix from UART."""
        fix = LocationFix()

        if self.uart is None:
            if self.debug:
                print("[GPS] No UART configured")
            return fix

        start_time = time.time()
        timeout = 60  # seconds

        while (time.time() - start_time) < timeout:
            try:
                line = self.uart.readline()
            except Exception as e:
                if self.debug:
                    print(f"[GPS] UART read error: {e}")
                time.sleep(0.1)
                continue

            if not line:
                time.sleep(0.1)
                continue

            if isinstance(line, bytes):
                try:
                    line = line.decode("utf-8", "ignore")
                except Exception:
                    continue

            line = line.strip()
            if not line:
                continue

            if self._parse_gga(line, fix) or self._parse_rmc(line, fix):
                if fix.valid:
                    if self.debug:
                        print(
                            f"[GPS] Fix: lat={fix.latitude:.6f}, lon={fix.longitude:.6f}, "
                            f"alt={fix.altitude:.2f}m, speed={fix.speed_kmh:.2f}km/h"
                        )
                    self.last_fix = fix
                    return fix

        if self.debug:
            print(f"[GPS] No valid fix received within {timeout}s")
        return fix

    @staticmethod
    def _parse_gga(line, fix):
        """Parse GGA NMEA sentence."""
        if not line.startswith(("$GPGGA", "$GNGGA")):
            return False

        try:
            parts = line.split(",")
            if len(parts) < 10:
                return False

            lat_str = parts[2]
            lat_hem = parts[3]
            lon_str = parts[4]
            lon_hem = parts[5]
            alt_str = parts[9]

            if not (lat_str and lat_hem and lon_str and lon_hem):
                return False

            fix.latitude = GNSSReader._parse_latitude(lat_str, lat_hem)
            fix.longitude = GNSSReader._parse_longitude(lon_str, lon_hem)
            fix.altitude = float(alt_str) if alt_str else 0.0
            fix.timestamp = time.time()
            fix.valid = fix.latitude != 0.0 or fix.longitude != 0.0
            return True
        except (ValueError, IndexError):
            return False

    @staticmethod
    def _parse_rmc(line, fix):
        """Parse RMC NMEA sentence."""
        if not line.startswith(("$GPRMC", "$GNRMC")):
            return False

        try:
            parts = line.split(",")
            if len(parts) < 8:
                return False

            lat_str = parts[3]
            lat_hem = parts[4]
            lon_str = parts[5]
            lon_hem = parts[6]
            speed_str = parts[7]

            if not (lat_str and lat_hem and lon_str and lon_hem):
                return False

            fix.latitude = GNSSReader._parse_latitude(lat_str, lat_hem)
            fix.longitude = GNSSReader._parse_longitude(lon_str, lon_hem)
            fix.speed_kmh = float(speed_str) * 1.852 if speed_str else 0.0
            fix.timestamp = time.time()
            fix.valid = fix.latitude != 0.0 or fix.longitude != 0.0
            return True
        except (ValueError, IndexError):
            return False

    @staticmethod
    def _parse_latitude(value, hemisphere):
        if not value or not hemisphere:
            return 0.0
        try:
            degrees = float(value[:2])
            minutes = float(value[2:]) / 60.0
            decimal = degrees + minutes
            if hemisphere.upper() == "S":
                decimal *= -1.0
            return decimal
        except (ValueError, IndexError):
            return 0.0

    @staticmethod
    def _parse_longitude(value, hemisphere):
        if not value or not hemisphere:
            return 0.0
        try:
            degrees = float(value[:3])
            minutes = float(value[3:]) / 60.0
            decimal = degrees + minutes
            if hemisphere.upper() == "W":
                decimal *= -1.0
            return decimal
        except (ValueError, IndexError):
            return 0.0


class CondorTracker:
    """Main Condor Tracker application."""

    def __init__(self, gnss_reader=None, http_sender=None, debug=DEBUG_MODE):
        self.gnss_reader = gnss_reader or GNSSReader(debug=debug)
        self.http_sender = http_sender
        self.telemetry = FlightTelemetry()
        self.debug = debug
        self.current_fix = LocationFix()

    def start_tracking(self):
        """Start flight tracking session."""
        if self.debug:
            print(f"[APP] Starting Condor Tracker - {DEVICE_NAME}")

        self.telemetry.reset()

        start_time = time.time()
        while (time.time() - start_time) < TRANSMIT_INTERVAL_SECONDS:
            fix = self.gnss_reader.read_fix()
            if fix.valid:
                self.telemetry.update(fix)
                self.current_fix = fix

            time.sleep(GNSS_SAMPLE_INTERVAL_SECONDS)

        if self.debug:
            print("[APP] Transmission window reached")

        self.transmit_data()

    def transmit_data(self):
        """Transmit collected flight data to API."""
        if self.telemetry.sample_count == 0:
            if self.debug:
                print("[APP] No samples collected, skipping transmission")
            return False

        payload = self.telemetry.get_payload(self.current_fix)
        if self.debug:
            print(f"[APP] Payload: {payload}")

        if self.http_sender:
            return self.http_sender.send(payload)

        return False

    def enter_sleep(self):
        """Enter deep sleep mode."""
        if not ENABLE_DEEP_SLEEP:
            if self.debug:
                print("[POWER] Deep sleep disabled, waiting...")
            time.sleep(TRANSMIT_INTERVAL_SECONDS)
            return

        if self.debug:
            print(
                f"[POWER] Entering deep sleep for {TRANSMIT_INTERVAL_SECONDS} seconds"
            )

        if MICROPYTHON:
            try:
                sleep_ms = int(TRANSMIT_INTERVAL_SECONDS * 1000)
                machine.deepsleep(sleep_ms)
            except Exception as e:
                if self.debug:
                    print(f"[POWER] Deep sleep failed: {e}")
                time.sleep(TRANSMIT_INTERVAL_SECONDS)
        else:
            time.sleep(TRANSMIT_INTERVAL_SECONDS)

    def run(self):
        """Main application loop."""
        while True:
            self.start_tracking()
            self.enter_sleep()
