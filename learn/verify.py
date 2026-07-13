#!/usr/bin/env python3
"""Live verifier for the continuity tester.

Proves, end to end, that:
  1. the sketch is running        (its banner arrives)
  2. the stream is clean          (no mojibake -> baud is right)
  3. the pin-to-pin trigger works (contact MADE and BROKEN both detected, and the
                                   contacts counter increments)

Prints each event live as you touch the probes, then a PASS/FAIL verdict.

This is optional. You can do the same thing by eye in the Arduino IDE's Serial
Monitor (at 115200 baud) - this just turns it into a pass/fail check.

USAGE
-----
    python3 learn/verify.py                 # auto-detect the board
    python3 learn/verify.py /dev/cu.xxxx    # or name the port

Then touch the two probe leads (pin 32 and GND) together and apart.
Close the Arduino IDE Serial Monitor first - only one program can own the port.
"""

import codecs
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial not installed.  Run:  pip install pyserial")

# Reuse the port-picking logic rather than re-implementing (and re-breaking) it.
from monitor import BAUD, find_port

TIMEOUT_S = 120

BANNER = "Continuity Tester"


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
    print("--- The board reboots once as this opens; that's expected, and it's")
    print("--- what prints the banner. Reboots after that would be real.")
    print("--- Now TOUCH THE PROBES TOGETHER AND APART (pin 32 <-> GND). Ctrl-C quits.\n")

    decoder = codecs.getincrementaldecoder("utf-8")("replace")
    buf = ""
    made = broke = reboots = mojibake = 0
    counter_hi = 0
    banner_seen = False
    first_event_skipped = False
    start = time.time()

    try:
        while time.time() - start < TIMEOUT_S:
            data = ser.read(256)
            if not data:
                continue
            buf += decoder.decode(data)
            *lines, buf = buf.split("\n")
            for raw in lines:
                line = raw.rstrip("\r")
                mojibake += line.count("�")

                if BANNER in line:
                    banner_seen = True
                    print(f"  [{time.time()-start:5.1f}s] banner seen - sketch is running")
                    continue

                # Only count reboots AFTER the banner. The open-time reset is expected
                # (macOS asserts DTR on open); flagging it every run would be crying wolf.
                if "rst:0x" in line and banner_seen:
                    reboots += 1
                    print(f"  [{time.time()-start:5.1f}s] *** UNEXPECTED REBOOT ***")
                    continue

                m = re.search(r"contacts=(\d+)", line)
                if m:
                    counter_hi = max(counter_hi, int(m.group(1)))

                is_made = line.startswith("CONTINUITY")
                is_broke = line.startswith("open")
                if not (is_made or is_broke):
                    continue

                # The sketch seeds lastState = -1, so it always prints ONE line at boot
                # describing the state it woke up in. That is not a probe event.
                if not first_event_skipped:
                    first_event_skipped = True
                    print(f"  [{time.time()-start:5.1f}s] (boot state: {line.split()[0]})")
                    continue

                if is_made:
                    made += 1
                    print(f"  [{time.time()-start:5.1f}s] CONTACT MADE    -> {line.strip()}")
                else:
                    broke += 1
                    print(f"  [{time.time()-start:5.1f}s] contact broken  -> {line.strip()}")

            if made >= 1 and broke >= 1 and counter_hi >= 1:
                break
    except KeyboardInterrupt:
        print("\n(stopped)")
    except serial.SerialException as exc:
        print(f"\n--- lost the port: {exc}")
    finally:
        ser.close()

    print("\n================= VERDICT =================")
    print(f"  sketch running (banner)   : {banner_seen}")
    print(f"  contact MADE events       : {made}")
    print(f"  contact BROKEN events     : {broke}")
    print(f"  contacts counter reached  : {counter_hi}")
    print(f"  unexpected reboots        : {reboots}")
    print(f"  mojibake characters       : {mojibake}")

    ok = banner_seen and made >= 1 and broke >= 1 and counter_hi >= 1 and mojibake == 0
    if ok:
        print("\n  PASS - the pin-to-pin trigger works and prints correctly.")
        if reboots:
            print(f"  (note: {reboots} unexpected reboot(s) - worth a look, but the trigger worked)")
    else:
        print("\n  FAIL")
        if not banner_seen:
            print("   - No banner. Sketch not running, or the baud is wrong. Re-upload,")
            print("     and check Serial.begin(115200) matches.")
        if mojibake:
            print("   - Mojibake: baud mismatch. The board talks at 115200.")
        if made == 0:
            print("   - No contact detected: check one jumper is in pin 32 and one in GND,")
            print("     and that you touched the metal ends together.")
        elif broke == 0:
            print("   - Contact made but never released - separate the probes too.")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
