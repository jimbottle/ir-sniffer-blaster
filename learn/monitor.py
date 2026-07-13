#!/usr/bin/env python3
"""Serial monitor that does NOT reset the ESP32.

WHY THIS EXISTS
---------------
On the ESP32, the USB-serial chip's DTR and RTS control lines are wired to the
chip's EN (reset) and GPIO0 (boot-mode) pins. That's the circuit that lets the
Arduino IDE auto-reset the board to flash it without you pressing buttons.

The side effect: the Arduino IDE's Serial Monitor ASSERTS those lines. That leaves
EN in a marginal state where ordinary electrical noise — like your hand bridging
two probe wires — can tip it and REBOOT the chip mid-print. You get boot spam
(`SPI_FAST_FLASH_BOOT`, `configsip:`, `clk_drv:`) shredding your output, and it
looks like your circuit is broken when it is fine.

This script deasserts DTR and RTS and just reads. Verified: 10 probe touches,
10 clean contact/break events, zero reboots — where the IDE monitor rebooted
constantly on the same hardware.

USAGE
-----
    python3 learn/monitor.py                 # auto-detect the board
    python3 learn/monitor.py /dev/cu.xxxx    # or name the port

Ctrl-C to quit. Close it before uploading — only one program can own the port.
"""

import glob
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial not installed.  pip install pyserial")

BAUD = 115200


def find_port() -> str:
    if len(sys.argv) > 1:
        return sys.argv[1]
    ports = glob.glob("/dev/cu.usbserial-*") + glob.glob("/dev/cu.usbmodem*")
    if not ports:
        sys.exit("No board found. Plug it in, or pass the port explicitly.")
    return ports[0]


def main() -> None:
    port = find_port()
    try:
        ser = serial.Serial(port, BAUD, timeout=0.1)
    except Exception as exc:  # noqa: BLE001
        sys.exit(
            f"Could not open {port}: {exc}\n"
            "If it's busy, close the Arduino IDE Serial Monitor."
        )

    # The whole point: leave the board's reset/boot lines alone.
    ser.setDTR(False)
    ser.setRTS(False)
    time.sleep(0.2)
    ser.reset_input_buffer()

    print(f"--- {port} @ {BAUD} (DTR/RTS released — will not reset the board)")
    print("--- Ctrl-C to quit\n")
    try:
        while True:
            data = ser.read(256)
            if data:
                sys.stdout.write(data.decode("utf-8", "replace"))
                sys.stdout.flush()
    except KeyboardInterrupt:
        print("\n--- closed")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
