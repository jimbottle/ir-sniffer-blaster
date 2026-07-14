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
That is expected. ROM boot dumps are ignored - their junk bytes are not counted as
mojibake, and the open-time reset is not counted as a reboot.

The line parser is a pure state machine (`feed`), so it can be tested without a
board - see test_verify.py. It has been broken by output-format changes before.

USAGE
-----
    python3 learn/verify.py                 # auto-detect the board
    python3 learn/verify.py /dev/cu.xxxx    # or name the port

Then touch the two probe leads (pin 32 and GND) together and apart.
Close the Arduino IDE Serial Monitor first - only one program can own the port.
"""

from __future__ import annotations

import codecs
import re
import sys
import time
from dataclasses import dataclass

TIMEOUT_S = 120
BANNER = "Continuity Tester"
USAGE = ("usage: verify.py [PORT]\n"
         "  auto-detects /dev/cu.usbserial-* or /dev/cu.usbmodem* if omitted")

# Lines that mean "the ESP32 ROM is talking, not the sketch". A boot dump is full of
# binary junk; counting that as mojibake would accuse a perfectly good board of a
# baud mismatch. Detecting the START is enough - we stay in "resyncing" until real
# sketch output resumes.
BOOT_MARKERS = ("rst:0x", "ets ", "SPI_FAST_FLASH_BOOT", "configsip:", "clk_drv:")

MOJIBAKE = "�"


@dataclass
class State:
    alive: bool = False           # have we seen ANY line we recognise?
    banner_seen: bool = False
    resyncing: bool = False       # currently inside a ROM boot dump
    skip_boot_line: bool = False  # setup() prints one status line; not a probe event
    made: int = 0
    broke: int = 0
    reboots: int = 0
    mojibake: int = 0
    counter_hi: int = 0

    @property
    def ok(self) -> bool:
        return (self.alive and self.made >= 1 and self.broke >= 1
                and self.counter_hi >= 1 and self.mojibake == 0)


def feed(st: State, line: str) -> str | None:
    """Consume one line of serial output. Returns an event name, or None.

    Pure: all effects land in `st`. This is what test_verify.py exercises.
    """
    if any(m in line for m in BOOT_MARKERS):
        # A reboot only counts as UNEXPECTED if the board was already streaming.
        # At port-open time nothing recognisable has arrived yet (alive == False),
        # so the expected open-time reset is not counted. Gating on `alive` rather
        # than `banner_seen` also means the FIRST genuine mid-run reboot is counted
        # on a host where the board doesn't reset when the port opens.
        st.resyncing = True
        if "rst:0x" in line and st.alive:
            st.reboots += 1
            return "REBOOT"
        return None

    if BANNER in line:
        first = not st.banner_seen
        st.banner_seen = True
        st.alive = True
        st.resyncing = False
        # Re-arm on EVERY banner: a mid-run reboot re-runs setup(), which prints
        # another status line. Without this it would be counted as a probe event -
        # the one fault this tool exists to catch would corrupt its own counts.
        st.skip_boot_line = True
        return "BANNER" if first else None

    is_made = line.startswith("CONTINUITY")
    is_broke = line.startswith("open")
    recognised = is_made or is_broke

    if recognised:
        st.resyncing = False       # real sketch output resumed
        st.alive = True

    # Count mojibake only on real sketch output: alive, and not mid-boot-dump.
    # Both gates matter. Without `alive`, the pre-banner dump is counted; without
    # `resyncing`, a POST-reboot dump is counted - and either one FAILs a board
    # whose baud is provably fine, which is the exact opposite of the diagnosis.
    if st.alive and not st.resyncing:
        st.mojibake += line.count(MOJIBAKE)

    m = re.search(r"contacts=(\d+)", line)
    if m:
        st.counter_hi = max(st.counter_hi, int(m.group(1)))

    if not recognised:
        return None

    if st.skip_boot_line:
        st.skip_boot_line = False
        return "BOOT_STATE"

    if is_made:
        st.made += 1
        return "MADE"
    st.broke += 1
    return "BROKE"


def main() -> None:
    try:
        import serial
    except ImportError:
        sys.exit("pyserial not installed.  Run:  pip install pyserial")

    # Reuse the port picker; pass our own usage/doc so messages name THIS tool.
    from monitor import BAUD, find_port

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
    print("--- boot noise is ignored until real sketch output resumes.")
    print("--- Now TOUCH THE PROBES TOGETHER AND APART (pin 32 <-> GND). Ctrl-C quits.\n")

    st = State()
    decoder = codecs.getincrementaldecoder("utf-8")("replace")
    buf = ""
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
                event = feed(st, line)
                t = time.time() - start
                if event == "BANNER":
                    print(f"  [{t:5.1f}s] banner seen - sketch is running")
                elif event == "REBOOT":
                    print(f"  [{t:5.1f}s] *** UNEXPECTED REBOOT ***")
                elif event == "BOOT_STATE":
                    print(f"  [{t:5.1f}s] (boot state: {line.split()[0]})")
                elif event == "MADE":
                    print(f"  [{t:5.1f}s] CONTACT MADE    -> {line.strip()}")
                elif event == "BROKE":
                    print(f"  [{t:5.1f}s] contact broken  -> {line.strip()}")
            if st.made >= 1 and st.broke >= 1 and st.counter_hi >= 1:
                break
    except KeyboardInterrupt:
        print("\n(stopped)")
    except Exception as exc:  # noqa: BLE001  (serial.SerialException and friends)
        print(f"\n--- lost the port: {exc}")
    finally:
        ser.close()

    print("\n================= VERDICT =================")
    print(f"  sketch running            : {st.alive}")
    print(f"  contact MADE events       : {st.made}")
    print(f"  contact BROKEN events     : {st.broke}")
    print(f"  contacts counter reached  : {st.counter_hi}")
    print(f"  unexpected reboots        : {st.reboots}")
    print(f"  mojibake characters       : {st.mojibake}")

    if st.ok:
        print("\n  PASS - the pin-to-pin trigger works and prints correctly.")
        if st.reboots:
            print(f"  (note: {st.reboots} unexpected reboot(s) - worth a look, but the "
                  f"trigger worked)")
    else:
        print("\n  FAIL")
        if not st.alive:
            print("   - Nothing recognisable arrived. Either the sketch isn't running, or")
            print("     the baud is wrong (the board talks at 115200). Re-upload and check")
            print("     Serial.begin(115200).")
        if st.mojibake:
            print("   - Corrupted characters in real sketch output: baud mismatch.")
        if st.made == 0 and st.alive:
            print("   - No contact detected: check one jumper is in pin 32 and one in GND,")
            print("     and that you touched the metal ends together.")
        elif st.broke == 0 and st.made:
            print("   - Contact made but never released - separate the probes too.")
    sys.exit(0 if st.ok else 1)


if __name__ == "__main__":
    main()
