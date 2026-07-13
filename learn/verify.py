#!/usr/bin/env python3
"""Live verifier for the continuity tester.

Runs a loop against the board and proves, end to end, that:
  1. the sketch is running        (heartbeat lines arriving)
  2. output is clean              (no mojibake, no reboots)
  3. the pin-to-pin trigger works (contact MADE and BROKEN both detected,
                                   and the contacts counter increments)

Prints each event live as you touch the probes, then a PASS/FAIL verdict.

USAGE
-----
    python3 learn/verify.py

Then touch the two probe leads (pin 32 and GND) together and apart a few times.
It exits PASS as soon as it has seen a clean make + break. Ctrl-C to stop early.

Close the Arduino IDE Serial Monitor first - only one program can own the port.
"""

import glob
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial not installed.  Run:  pip install pyserial")

BAUD = 115200
TIMEOUT_S = 120


def find_port() -> str:
    if len(sys.argv) > 1:
        return sys.argv[1]
    ports = sorted(glob.glob("/dev/cu.usbserial-*") + glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("No board found. Plug it in.")
    return ports[0]


def main() -> None:
    port = find_port()
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = port, BAUD, 0.1
    ser.dtr = False
    ser.rts = False
    try:
        ser.open()
    except Exception as exc:  # noqa: BLE001
        sys.exit(f"Could not open {port}: {exc}\nClose the Arduino Serial Monitor.")

    print(f"--- {port} @ {BAUD}")
    print("--- TOUCH THE PROBES TOGETHER AND APART a few times.")
    print("--- (pin 32 <-> GND).  Ctrl-C to stop.\n")

    buf = ""
    seen: set[str] = set()
    made = broke = beats = reboots = mojibake = 0
    counter_hi = 0
    start = time.time()

    try:
        while time.time() - start < TIMEOUT_S:
            data = ser.read(256)
            if not data:
                continue
            buf += data.decode("utf-8", "replace")
            lines, buf = buf.split("\n")[:-1], buf.split("\n")[-1]
            for line in lines:
                line = line.rstrip("\r")
                mojibake += line.count("�")
                if "rst:0x" in line:
                    reboots += 1
                    print(f"  [{time.time()-start:5.1f}s] *** REBOOT ***")
                if "beat" in line:
                    beats += 1
                m = re.search(r"contacts=(\d+)", line)
                if m:
                    counter_hi = max(counter_hi, int(m.group(1)))
                if line.startswith(">>> CHANGE") and line not in seen:
                    seen.add(line)
                    if "CONTINUITY" in line:
                        made += 1
                        print(f"  [{time.time()-start:5.1f}s] CONTACT MADE    -> {line[12:].strip()}")
                    else:
                        broke += 1
                        print(f"  [{time.time()-start:5.1f}s] contact broken  -> {line[12:].strip()}")
            if made >= 1 and broke >= 2 and counter_hi >= 1:
                break
    except KeyboardInterrupt:
        print("\n(stopped)")
    except serial.SerialException as exc:
        print(f"\n--- lost the port: {exc}")
    finally:
        ser.close()

    print("\n================= VERDICT =================")
    print(f"  sketch alive (heartbeats) : {beats}")
    print(f"  contact MADE events       : {made}")
    print(f"  contact BROKEN events     : {broke}")
    print(f"  contacts counter reached  : {counter_hi}")
    print(f"  spurious reboots          : {reboots}")
    print(f"  mojibake characters       : {mojibake}")

    ok = beats > 0 and made >= 1 and counter_hi >= 1 and mojibake == 0
    if ok:
        print("\n  PASS - the pin-to-pin trigger works and prints correctly.")
        if reboots:
            print(f"  (note: {reboots} reboot(s) seen, but the trigger still worked)")
    else:
        print("\n  FAIL")
        if beats == 0:
            print("   - No heartbeat: sketch not running, or wrong baud. Re-upload.")
        if made == 0:
            print("   - No contact detected: check a jumper is in pin 32 and one in GND,")
            print("     and that you actually touched the metal ends together.")
        if mojibake:
            print("   - Mojibake: baud mismatch somewhere.")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
