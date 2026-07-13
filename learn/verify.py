#!/usr/bin/env python3
"""Live verifier for the continuity tester.

Proves, end to end, that:
  1. the sketch is running        (its banner, or any recognised status line, arrives)
  2. the stream is clean          (no mojibake -> the baud rate is right)
  3. the pin-to-pin trigger works (contact MADE and BROKEN both detected, and the
                                   contacts counter increments)

Prints each event live as you touch the probes, then a PASS/FAIL verdict.

This is optional. You can confirm the same thing by eye in the Arduino IDE's Serial
Monitor (at 115200 baud) - this just turns it into a pass/fail check.

Note: opening the port usually reboots the board once (macOS asserts DTR on open).
That is expected. The ROM boot dump that follows is ignored - garbage bytes in it are
not counted as mojibake, and it is not counted as a reboot.

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
# Pass our own usage/doc so its messages name THIS tool, not monitor.py.
from monitor import BAUD, find_port

TIMEOUT_S = 120
BANNER = "Continuity Tester"
USAGE = ("usage: verify.py [PORT]\n"
         "  auto-detects /dev/cu.usbserial-* or /dev/cu.usbmodem* if omitted")


def main() -> None:
    port = find_port(usage=USAGE, doc=__doc__)
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = port, BAUD, 0.1
    ser.dtr = False
    ser.rts = False
    try:
        ser.open()
    except Exception as exc:  # noqa: BLE001
        sys.exit(f"Could not open {port}: {exc}\nClose the Arduino Serial Monitor.")

    print(f"--- {port} @ {BAUD}")
    print("--- Opening the port usually reboots the board once. That's expected;")
    print("--- the boot noise before the banner is ignored.")
    print("--- Now TOUCH THE PROBES TOGETHER AND APART (pin 32 <-> GND). Ctrl-C quits.\n")

    decoder = codecs.getincrementaldecoder("utf-8")("replace")
    buf = ""
    made = broke = reboots = mojibake = 0
    counter_hi = 0
    alive = False          # have we seen ANY line we recognise? (liveness)
    banner_seen = False    # have we seen the banner specifically?
    skip_boot_line = False # the sketch prints one status line at boot; not a probe event
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

                if BANNER in line:
                    if not banner_seen:
                        print(f"  [{time.time()-start:5.1f}s] banner seen - sketch is running")
                    banner_seen = True
                    alive = True
                    # setup() always prints one status line right after the banner
                    # (lastState = -1). Re-arm on EVERY banner, so the line following a
                    # mid-run reboot is discarded too, not counted as a probe event.
                    skip_boot_line = True
                    continue

                is_made = line.startswith("CONTINUITY")
                is_broke = line.startswith("open")
                recognised = is_made or is_broke

                # Liveness without a banner: if the board never reboots on open (a
                # non-mac host, or an adapter whose DTR isn't wired to EN), a healthy
                # board just streams status lines. Those prove it's alive too.
                if recognised and not alive:
                    alive = True

                # Only count mojibake once we're synced to real output. The pre-banner
                # ROM boot dump legitimately contains junk bytes, and counting those
                # would FAIL a board whose baud is provably correct - crying wolf.
                if alive:
                    mojibake += line.count("�")

                # Likewise, only count reboots after the banner: the open-time reset is
                # expected and would otherwise be flagged on every single run.
                if "rst:0x" in line and banner_seen:
                    reboots += 1
                    print(f"  [{time.time()-start:5.1f}s] *** UNEXPECTED REBOOT ***")
                    continue

                m = re.search(r"contacts=(\d+)", line)
                if m:
                    counter_hi = max(counter_hi, int(m.group(1)))

                if not recognised:
                    continue

                if skip_boot_line:
                    skip_boot_line = False
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
    print(f"  sketch running            : {alive}")
    print(f"  contact MADE events       : {made}")
    print(f"  contact BROKEN events     : {broke}")
    print(f"  contacts counter reached  : {counter_hi}")
    print(f"  unexpected reboots        : {reboots}")
    print(f"  mojibake characters       : {mojibake}")

    ok = alive and made >= 1 and broke >= 1 and counter_hi >= 1 and mojibake == 0
    if ok:
        print("\n  PASS - the pin-to-pin trigger works and prints correctly.")
        if reboots:
            print(f"  (note: {reboots} unexpected reboot(s) - worth a look, but the trigger worked)")
    else:
        print("\n  FAIL")
        if not alive:
            print("   - Nothing recognisable arrived. Sketch not running, or the baud is")
            print("     wrong. Check Serial.begin(115200), and re-upload.")
        if mojibake:
            print("   - Mojibake AFTER the banner: baud mismatch. The board talks at 115200.")
        if made == 0:
            print("   - No contact detected: check one jumper is in pin 32 and one in GND,")
            print("     and that you touched the metal ends together.")
        elif broke == 0:
            print("   - Contact made but never released - separate the probes too.")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
