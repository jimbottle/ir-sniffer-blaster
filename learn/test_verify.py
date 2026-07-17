#!/usr/bin/env python3
"""Tests for verify.py's line parser, using captured serial streams.

Run:  python3 learn/test_verify.py       (no pytest, no board, no dependencies)

WHY THIS EXISTS
---------------
verify.py's parser was silently broken FOUR times in a row by changes to the
sketch's output format - each time it kept "working" (exiting, printing a verdict)
while being completely unable to detect anything. A parser that greps for strings
the firmware no longer prints fails silently, which is the worst way to fail.

These fixtures are real output shapes from the board. If you change what
continuity_tester.ino prints, these break loudly. That is the point.
"""

from __future__ import annotations

import sys

from verify import State, feed

BOOT_DUMP = [
    "ets Jul 29 2019 12:21:46",
    "",
    "rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)",
    "configsip: 271414342, SPIWP:0xee",
    "clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00",
    "�� � ��  � 0",     # binary junk in the ROM dump
    "entry 0x4008059c",
]

BANNER_BLOCK = [
    "=========================================",
    " Continuity Tester",
    " Probe A = pin 32     Probe B = GND",
    " Last reset: POWERON (power applied, or EN/reset pulled low)",
    " Only test circuits with the power OFF.",
    "=========================================",
]

# A reboot where the FIRST ROM line is itself garbled and does not start with a
# clean marker - the reset happens while the UART is mid-frame. The old "classify
# the boot dump" gate missed this; the current "only trust recognised lines" rule
# ignores it by construction.
GARBLED_BOOT = [
    "�ts Jul 29 2019 12:21:46",   # 'ets...' with a mangled first byte
    "�� rst:0x1 boot noise",
    "configsip: 271414342",
    "entry 0x4008059c",
]

BOOT_STATE = "open        circuit BROKEN     pin32=HIGH   LED=off   contacts=0"
MADE_1 = "CONTINUITY  circuit COMPLETE   pin32=LOW    LED=ON    contacts=1"
BROKE_1 = "open        circuit BROKEN     pin32=HIGH   LED=off   contacts=1"


def run(lines: list[str]) -> State:
    st = State()
    for line in lines:
        feed(st, line)
    return st


def check(name: str, cond: bool, detail: str = "") -> bool:
    print(f"  {'PASS' if cond else 'FAIL'}  {name}{'  ' + detail if detail else ''}")
    return cond


def main() -> None:
    results = []

    # 1. The normal run: open-time reset, banner, boot line, then a real make+break.
    print("normal run (expected open-time reset):")
    st = run(BOOT_DUMP + BANNER_BLOCK + [BOOT_STATE, MADE_1, BROKE_1])
    results += [
        check("PASSes", st.ok),
        check("pre-banner ROM junk not counted as mojibake", st.mojibake == 0,
              f"(got {st.mojibake})"),
        check("open-time reset not counted as a reboot", st.reboots == 0,
              f"(got {st.reboots})"),
        check("boot status line not counted as a probe event",
              st.made == 1 and st.broke == 1, f"(made={st.made} broke={st.broke})"),
    ]

    # 2. A mid-run reboot. This is the fault the tool exists to catch: it must be
    #    REPORTED, must not corrupt the event counts, and must not be misdiagnosed
    #    as a baud problem via the post-reboot ROM dump's junk bytes.
    print("\nmid-run reboot (the fault this tool exists to catch):")
    st = run(BOOT_DUMP + BANNER_BLOCK + [BOOT_STATE, MADE_1, BROKE_1]
             + BOOT_DUMP + BANNER_BLOCK + [BOOT_STATE])
    results += [
        check("the reboot IS counted", st.reboots == 1, f"(got {st.reboots})"),
        check("post-reboot ROM junk NOT counted as mojibake", st.mojibake == 0,
              f"(got {st.mojibake})"),
        check("post-reboot boot line not counted as an event",
              st.made == 1 and st.broke == 1, f"(made={st.made} broke={st.broke})"),
        check("still PASSes (trigger worked; reboot is a note, not a failure)", st.ok),
    ]

    # 3. Wrong baud: nothing decodes, so nothing is ever recognised.
    print("\nwrong baud (9600 vs 115200):")
    st = run(["�� � ��0 �", "� ���"])
    results += [
        check("FAILs", not st.ok),
        check("reports not-alive (not a bogus contact count)", not st.alive),
    ]

    # 4. A host where the port open does NOT reset the board: no banner ever, the
    #    sketch is just mid-stream. It must still be recognised as alive, its first
    #    event must not be swallowed as a boot line, and a genuine reboot must count.
    print("\nboard already running, no reset on open (non-mac host):")
    st = run([MADE_1, BROKE_1])
    results += [
        check("recognised as alive without a banner", st.alive),
        check("first event NOT swallowed as a boot line",
              st.made == 1 and st.broke == 1, f"(made={st.made} broke={st.broke})"),
        check("PASSes", st.ok),
    ]
    st = run([MADE_1, BROKE_1] + BOOT_DUMP + BANNER_BLOCK + [BOOT_STATE])
    results += [
        check("first genuine reboot counted even with no banner beforehand",
              st.reboots == 1, f"(got {st.reboots})"),
    ]

    # 5. Real corruption AFTER sync is genuine bit-corruption and must be caught.
    print("\ngenuine corruption in recognised sketch output:")
    st = run(BANNER_BLOCK + [BOOT_STATE, MADE_1, "CONTINUITY �� garbled", BROKE_1])
    results += [
        check("mojibake in a recognised line IS counted", st.mojibake > 0),
        check("FAILs", not st.ok),
    ]

    # 6. A reboot whose ROM dump is garbled from the first byte. The junk must NOT
    #    be counted as mojibake - this is the exact ordering the previous gate missed.
    print("\nmid-run reboot with a garbled boot dump:")
    st = run(BANNER_BLOCK + [BOOT_STATE, MADE_1, BROKE_1]
             + GARBLED_BOOT + BANNER_BLOCK + [BOOT_STATE])
    results += [
        check("garbled boot junk NOT counted as mojibake", st.mojibake == 0,
              f"(got {st.mojibake})"),
        check("the reboot is still counted", st.reboots == 1, f"(got {st.reboots})"),
        check("still PASSes", st.ok),
    ]

    # 7. A power-on boot loop: resets forever, never reaches the sketch. The tool
    #    must surface it (raw_resets) rather than blame the baud or the wiring.
    print("\npower-on boot loop (never reaches the sketch):")
    st = run(["rst:0x1 (POWERON_RESET)", "boot noise",
              "rst:0xc (SW_CPU_RESET)", "more noise",
              "rst:0x1 (POWERON_RESET)"])
    results += [
        check("FAILs", not st.ok),
        check("not reported as alive", not st.alive),
        check("raw resets counted (>=2) so the verdict can name a boot loop",
              st.raw_resets >= 2, f"(got {st.raw_resets})"),
        check("not miscounted as unexpected mid-stream reboots", st.reboots == 0,
              f"(got {st.reboots})"),
    ]

    print(f"\n{sum(results)}/{len(results)} checks passed")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
