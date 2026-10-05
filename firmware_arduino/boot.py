"""Condor Tracker - Boot Script

Initialization script that sets up the device and starts the main application.
Use this as your main.py on the ESP32-S3 MicroPython firmware.
"""

import time
from config import DEBUG_MODE
from main import CondorTracker, GNSSReader
from http_sender import HTTPSender


def main():
    """Main entry point for Condor Tracker."""
    print("="*50)
    print("CONDOR TRACKER - Flight Monitoring System")
    print("Arduino/ESP32-S3 + T-SIM7080G")
    print("="*50)
    print()

    # Initialize components
    gnss_reader = GNSSReader(debug=DEBUG_MODE)
    http_sender = HTTPSender(debug=DEBUG_MODE)

    # Create tracker application
    tracker = CondorTracker(
        gnss_reader=gnss_reader,
        http_sender=http_sender,
        debug=DEBUG_MODE,
    )

    # Run main loop
    try:
        tracker.run()
    except KeyboardInterrupt:
        print("\n[APP] Shutdown requested")
    except Exception as e:
        print(f"[APP] Fatal error: {e}")
        if DEBUG_MODE:
            import traceback
            traceback.print_exc()


if __name__ == "__main__":
    main()
