#!/usr/bin/env python3
"""Serial monitor that doesn't reboot the ESP32 WHILE YOU'RE USING IT.

WHY THIS EXISTS
---------------
On the ESP32, the USB-serial chip's DTR and RTS lines are wired to the chip's EN
(reset) and GPIO0 (boot-mode) pins. That's the circuit that lets the Arduino IDE
auto-reset the board to flash it without you pressing buttons.

The Arduino IDE's Serial Monitor drives those lines, and the side effect is that the
board reboots *during use*. Measured on this board with the continuity tester running,
using insulated leads (no skin on any conductor): touching the two probes together
rebooted the chip mid-print through the IDE monitor, and did not reboot it once
through this script.

    10 probe touches -> 10 contacts, 10 breaks, 0 reboots.

Same board, same wires, same sketch, same hand. The only variable was the serial
program. This script holds DTR/RTS low and then leaves them alone.

WHAT IT DOES *NOT* DO
---------------------
It does not avoid the reset when the port is FIRST OPENED. macOS asserts DTR on open
no matter what we ask for, so the board reboots once as this script starts — verified.
That one reset is harmless, and it's actually useful: it's what prints the fresh
`Continuity Tester` banner you see straight away.

The property that matters is the one above: no resets while you're probing.

USAGE
-----
    python3 learn/monitor.py                 # auto-detect the board
    python3 learn/monitor.py /dev/cu.xxxx    # or name the port

Needs pyserial:  pip install pyserial
Ctrl-C to quit. Close it before uploading — only one program can own the port.
"""

import codecs
import glob
import sys

try:
    import serial
except ImportError:
    sys.exit("pyserial not installed.  Run:  pip install pyserial")

BAUD = 115200
USAGE = ("usage: monitor.py [PORT]\n"
         "  auto-detects /dev/cu.usbserial-* or /dev/cu.usbmodem* if omitted")


def find_port() -> str:
    argv = sys.argv[1:]
    if any(a in ("-h", "--help") for a in argv):
        print(USAGE)
        print(__doc__)
        raise SystemExit(0)
    # Anything else starting with '-' is a flag we don't support. Don't silently
    # treat it as a port path and fail with a confusing "Could not open --baud".
    unknown = [a for a in argv if a.startswith("-")]
    if unknown:
        sys.exit(f"unknown option: {unknown[0]}\n{USAGE}")
    if argv:
        return argv[0]

    ports = sorted(glob.glob("/dev/cu.usbserial-*") + glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("No board found. Plug it in, or name the port explicitly.\n" + USAGE)
    if len(ports) > 1:
        print("Multiple candidate ports found:", file=sys.stderr)
        for p in ports:
            print(f"  {p}", file=sys.stderr)
        sys.exit("Name the one you want:  python3 learn/monitor.py <PORT>")
    return ports[0]


def main() -> None:
    port = find_port()

    # Request DTR/RTS low BEFORE opening rather than after, so we never walk the pair
    # through a mismatched state. (macOS still asserts DTR at open, so the board does
    # reboot once here — see the docstring. What this prevents is resets *during use*.)
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = BAUD
    ser.timeout = 0.1
    ser.dtr = False
    ser.rts = False

    try:
        ser.open()
    except Exception as exc:  # noqa: BLE001
        sys.exit(
            f"Could not open {port}: {exc}\n"
            "If it's busy, close the Arduino IDE Serial Monitor."
        )

    print(f"--- {port} @ {BAUD}  (DTR/RTS held low — no resets while probing)")
    print("--- The board reboots once as this opens; that's what prints the banner.")
    print("--- Ctrl-C quits. Close this before uploading.\n")

    # Decode incrementally: a multi-byte character (e.g. the em dash the sketch
    # prints) can straddle two reads, and decoding each chunk separately would
    # mangle it into a replacement char — garbled output, in the anti-garble tool.
    decoder = codecs.getincrementaldecoder("utf-8")("replace")

    try:
        while True:
            data = ser.read(256)
            if data:
                sys.stdout.write(decoder.decode(data))
                sys.stdout.flush()
    except KeyboardInterrupt:
        print("\n--- closed")
    except serial.SerialException as exc:
        print(f"\n--- lost the port: {exc}")
        print("--- (board unplugged?)")
    finally:
        try:
            ser.close()
        except Exception:  # noqa: BLE001
            pass


if __name__ == "__main__":
    main()
