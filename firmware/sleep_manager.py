"""Flight data computation logic."""

from __future__ import annotations

import math
import time


class FlightDataProcessor:
    """Tracks altitude, distance, and maximum speed for one transmission window."""

    def __init__(self):
        self.start_time = None
        self.last_timestamp = None
        self.last_position = None
        self.total_distance_km = 0.0
        self.max_speed_kmh = 0.0
        self.max_altitude_m = -999999.0
        self.min_altitude_m = 999999.0
        self.samples = 0

    def reset(self):
        self.start_time = time.time()
        self.last_timestamp = None
        self.last_position = None
        self.total_distance_km = 0.0
        self.max_speed_kmh = 0.0
        self.max_altitude_m = -999999.0
        self.min_altitude_m = 999999.0
        self.samples = 0

    def update(self, latitude, longitude, altitude, timestamp=None):
        if timestamp is None:
            timestamp = time.time()

        if self.start_time is None:
            self.start_time = timestamp

        if self.last_position is not None and self.last_timestamp is not None:
            elapsed_seconds = max(1.0, timestamp - self.last_timestamp)
            distance_km = self._haversine_km(
                self.last_position[0],
                self.last_position[1],
                latitude,
                longitude,
            )
            self.total_distance_km += distance_km

            if distance_km > 0.0:
                speed_kmh = (distance_km / elapsed_seconds) * 3600.0
                if speed_kmh > self.max_speed_kmh:
                    self.max_speed_kmh = speed_kmh

        self.last_position = (latitude, longitude)
        self.last_timestamp = timestamp
        self.samples += 1

        if altitude > self.max_altitude_m:
            self.max_altitude_m = altitude
        if altitude < self.min_altitude_m:
            self.min_altitude_m = altitude

        return {
            "distance_km": self.total_distance_km,
            "max_speed_kmh": self.max_speed_kmh,
            "max_altitude_m": self.max_altitude_m,
            "min_altitude_m": self.min_altitude_m,
            "samples": self.samples,
        }

    def summary_payload(self, latitude, longitude, altitude, timestamp=None):
        if timestamp is None:
            timestamp = time.time()

        return {
            "timestamp": timestamp,
            "latitude": latitude,
            "longitude": longitude,
            "altitude": altitude,
            "distance_km": round(self.total_distance_km, 3),
            "max_speed_kmh": round(self.max_speed_kmh, 3),
            "max_altitude_m": round(self.max_altitude_m, 3),
            "min_altitude_m": round(self.min_altitude_m, 3),
            "samples": self.samples,
        }

    @staticmethod
    def _haversine_km(lat1, lon1, lat2, lon2):
        radius_km = 6371.0
        lat1_rad = math.radians(lat1)
        lon1_rad = math.radians(lon1)
        lat2_rad = math.radians(lat2)
        lon2_rad = math.radians(lon2)

        delta_lat = lat2_rad - lat1_rad
        delta_lon = lon2_rad - lon1_rad

        a = (
            math.sin(delta_lat / 2.0) ** 2
            + math.cos(lat1_rad) * math.cos(lat2_rad) * math.sin(delta_lon / 2.0) ** 2
        )
        c = 2.0 * math.atan2(math.sqrt(a), math.sqrt(1.0 - a))
        return radius_km * c
