"""Capture native ESP USB logs across disconnects; stop with Ctrl+C."""
import argparse
from datetime import datetime
from pathlib import Path
import sys
import time

import serial
from serial.tools import list_ports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Fixed port; otherwise detect one Espressif USB Serial/JTAG device")
    parser.add_argument("--log", type=Path, default=Path("usb-monitor.log"))
    args = parser.parse_args()
    with args.log.open("ab", buffering=0) as log:
        def emit(data):
            log.write(data)
            sys.stdout.write(data.decode("utf-8", errors="replace"))
            sys.stdout.flush()

        def status(message):
            emit(f"\n[{datetime.now().astimezone().isoformat(timespec='seconds')}] {message}\n".encode())

        status(f"Logging to {args.log.resolve()}; Ctrl+C to stop")
        previous_error = None
        try:
            while True:
                connection = None
                try:
                    port = args.port
                    if not port:
                        ports = [p.device for p in list_ports.comports()
                                 if (p.vid, p.pid) == (0x303A, 0x1001)]
                        if len(ports) != 1:
                            raise serial.SerialException(
                                f"Waiting for one Espressif USB device; found {ports}. "
                                "Use --port COMxx if multiple devices are attached.")
                        port = ports[0]
                    connection = serial.Serial(baudrate=115200, timeout=0.2)
                    # Apply initial control-line states before opening. Drivers may
                    # still produce a transient; do not intentionally reset the chip.
                    connection.dtr = False
                    connection.rts = False
                    connection.port = port
                    connection.open()
                    status(f"Connected to {port}")
                    previous_error = None
                    while True:
                        data = connection.read(connection.in_waiting or 1)
                        if data:
                            emit(data)
                except (serial.SerialException, OSError) as error:
                    message = str(error)
                    if message != previous_error:
                        status(f"USB unavailable: {message}; retrying")
                        previous_error = message
                finally:
                    if connection is not None:
                        connection.close()
                time.sleep(0.25)
        except KeyboardInterrupt:
            status("Stopped")


if __name__ == "__main__":
    main()
