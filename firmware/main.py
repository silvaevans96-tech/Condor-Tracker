"""Power management utilities for the Condor Tracker firmware."""

from __future__ import annotations

import time

try:
    import machine  # type: ignore
except ImportError:  # pragma: no cover - desktop testing
    machine = None  # type: ignore


class SleepManager:
    """Puts the device to sleep between transmission windows."""

    def __init__(self, deep_sleep_enabled=True):
        self.deep_sleep_enabled = deep_sleep_enabled

    def go_to_sleep(self, seconds):
        """Sleep for the given amount of seconds."""
        if self.deep_sleep_enabled and machine is not None and hasattr(machine, "deepsleep"):
            # MicroPython uses milliseconds for deep sleep.
            machine.deepsleep(int(seconds * 1000))
            return

        time.sleep(seconds)

    def sleep_until_next_transmission(self, seconds):
        self.go_to_sleep(seconds)
