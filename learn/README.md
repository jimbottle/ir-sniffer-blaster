# Learning the Arduino IDE — Build a Continuity Tester

A first real programming project. You'll build a **go/no-go continuity tester**: a
tool that tells you whether a circuit is complete (a wire is unbroken, a switch is
closed, a solder joint is actually joined).

**Parts needed: none.** Just the Feather and two jumper wires. Every component this
project needs is already inside the microcontroller.

> Standalone learning exercise — unrelated to the IR blaster firmware in the rest of
> this repo. It just borrows the same board.
>
> **Nothing else should be attached.** If the 5990 IR board happens to be wired to the
> Feather, unplug it — this sketch is deliberately minimal and doesn't configure its
> pins. Just the Feather and two jumpers.

---

## 1. Safety — read before plugging anything in

**Rule 1: Only test circuits with the power OFF.**
This tool probes using the ESP32's own 3.3V logic. If you touch a probe to something
that has its *own* voltage on it — a battery, a plugged-in device — you inject that
voltage directly into the GPIO pin. ESP32 pins are **not 5V tolerant** (~3.6V max)
and you can destroy the pin or the whole chip. Unplug what you're testing. Every time.

**Rule 2: Never test continuity by driving one pin HIGH and another LOW and shorting
them together.**
That wires the chip's internal *power* driver straight to its internal *ground*
driver with nothing to limit the current — a dead short through the silicon. It can
physically damage the output drivers.

The method we use is inherently safe: the sense pin is only ever an **INPUT** (it
never pushes current out), and the other probe goes to the **real GND pin** (which is
built to sink current). If you ever use a second GPIO as a ground reference, it must
be `OUTPUT` driven `LOW` — never `HIGH`.

---

## 2. The idea (this is the part worth understanding)

Here's the problem: a pin that's connected to *nothing* doesn't read as 0 or 1. It
"floats" — picks up electrical noise and reads randomly. Useless. So how do you
detect "connected vs. not connected"?

**The trick: give the pin a default, then see if the circuit overrides it.**

Inside the ESP32 is a **pull-up resistor** you can switch on in software. It gently
ties the pin up toward 3.3V. It's weak on purpose — think of it as a light suggestion,
not a command.

- **Nothing connected** → nothing else influences the pin, so the weak resistor wins.
  Pin sits at 3.3V → reads **HIGH**.
- **Probes touching across a complete circuit** → you've now given the pin a direct,
  easy path down to GND (0V). That path is *far* easier than the weak resistor, so it
  wins. Pin gets dragged to 0V → reads **LOW**.

```
   3.3V
     |
    [R]  <- internal pull-up (weak). Switched on by INPUT_PULLUP.
     |
     +----> pin 32  ---- probe A ----+
     |                                | your circuit under test
    GND <---------------- probe B ----+

   circuit OPEN   -> resistor wins  -> pin HIGH -> "no continuity"
   circuit CLOSED -> path to GND wins -> pin LOW -> "CONTINUITY!"
```

**The counterintuitive bit — and it trips up nearly everyone:** a *completed* circuit
reads **LOW**, not HIGH. That feels backwards. It reads LOW precisely *because*
completing the circuit is what drags the voltage **down** to ground. Sit with that
until it clicks; the same inverted logic shows up everywhere (it's how nearly every
button on every Arduino project works).

**Why no external parts:** the resistor you'd normally have to add is already built
into the chip. `INPUT_PULLUP` just switches it on.

---

## 3. The code, concept by concept

Open `learn/continuity_tester/continuity_tester.ino`. Every line is commented, but
here are the five ideas:

### `setup()` vs `loop()`
Every Arduino program has exactly these two functions.
- **`setup()`** runs **once**, at power-on/reset. It's for configuration: "this pin is
  an input, that one's an output, open the serial link."
- **`loop()`** runs **over and over, forever**, as fast as it can, until power is cut.
  This is where the work happens. There's no `main()` and no exit — an Arduino program
  is a loop that never ends.

### `pinMode(pin, mode)`
Tells the chip how to use a pin. We use two modes:
- `INPUT_PULLUP` — make it an input **and** switch on that internal pull-up resistor.
- `OUTPUT` — we drive this pin's voltage (for the LED).

### `digitalRead(pin)`
Looks at a pin's voltage and returns one of two values: `HIGH` (near 3.3V) or `LOW`
(near 0V). That's it — it's a yes/no question about voltage.

### `digitalWrite(pin, value)`
The opposite: *sets* an output pin's voltage. `HIGH` lights our LED, `LOW` turns it off.

### `Serial`
Your window into the program. `Serial.begin(115200)` opens a text channel over the USB
cable; `Serial.println(...)` sends a line to your computer. This is how you see what
the program is *thinking* — and it's the single most useful debugging tool you have.
When something misbehaves, print the values and look.

The heart of the whole sketch is three lines:

```cpp
int state = digitalRead(SENSE_PIN);   // HIGH or LOW?
bool continuity = (state == LOW);     // LOW means the circuit completed the path to GND
digitalWrite(LED_PIN, continuity ? HIGH : LOW);   // show it
```

### Two details that aren't obvious

- **`if (state != lastState)`** — we only print when the reading *changes*. Without
  this, `loop()` would print hundreds of identical lines a second and you couldn't read
  a thing. Printing on change is a habit worth forming.
- **`delay(20)`** — pauses 20ms, so we check 50x/second. Instant to a human. It also
  smooths over "contact bounce": as two metal probes touch, they actually make and
  break contact several times in a few milliseconds.

---

## 4. Doing it in the Arduino IDE

### One-time setup

1. **Arduino IDE → Settings** (`⌘,`) → **Additional boards manager URLs**, paste:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
2. **Tools → Board → Boards Manager…** → search `esp32` → install **esp32 by Espressif
   Systems**. (Large download, one time only.)

### Every time

3. **Tools → Board → esp32 → "Adafruit Feather ESP32 V2"**
4. **Tools → Port → `/dev/cu.usbserial-…`** (the *usbserial* one — not Bluetooth)
5. **Tools → Upload Speed → `115200`** ← **REQUIRED, do not skip**

> **Why 115200 matters.** Your board's USB-serial chip is a CH34x, which is unreliable
> at the IDE's default 921600. The upload connects, identifies the chip, then dies with
> *"A fatal error occurred: The chip stopped responding."* It looks like broken hardware.
> It isn't — it's just the baud rate. We hit this exact failure and confirmed 115200
> fixes it.
>
> **Set this AFTER choosing the board.** Picking the board resets this menu back to its
> 921600 default, so if you set the speed first it gets silently clobbered.

6. **File → Open** → `learn/continuity_tester/continuity_tester.ino`
7. Click **Verify** (✓) — compiles without uploading. Always a good first check.
8. Click **Upload** (**→**) — the right-arrow, **not** the 🐞 bug icon. (The bug is
   *Debug*, which needs JTAG hardware this board doesn't have — see
   [Don't press the Debug button](#dont-press-the-debug-button).) Watch the progress
   bar; it ends with the board resetting.
9. **Tools → Serial Monitor**, and set the dropdown to **115200 baud**.

You should immediately see:

```
=========================================
 Continuity Tester
 Probe A = pin 32     Probe B = GND
 Last reset: POWERON (power applied, or EN/reset pulled low)
 Only test circuits with the power OFF.
=========================================
open        circuit BROKEN     pin32=HIGH   LED=off   contacts=0
```

Then **silence**. The sketch prints only when the state *changes*, so quiet means
"nothing has happened" — not "it's broken".

**Which LED am I watching?** The small **red** LED on the board (GPIO13) — *not* the
colorful NeoPixel. Right now it should be **off**.

### Wire it up (two jumpers)

| Jumper | Into | Is |
|---|---|---|
| Probe A | pin **`32`** | the sense pin |
| Probe B | any pin marked **`GND`** | the ground reference |

### Prove it works

**Touch the two free jumper ends together.**

| | Serial Monitor | Red LED |
|---|---|---|
| Probes apart (at rest) | `open        circuit BROKEN     pin32=HIGH   LED=off   contacts=0` | off |
| **Probes touching** | `CONTINUITY  circuit COMPLETE   pin32=LOW    LED=ON    contacts=1` | **on** |
| Probes apart again | `open        circuit BROKEN     pin32=HIGH   LED=off   contacts=1` | off |

`contacts` only increments when contact is **made**, so it stays at 0 until your first
touch, then holds its value while the probes are apart.

Separate them and it flips back. That's your proof: you're reading the physical world
and reacting to it. Now touch the probes to the two ends of a wire, a paperclip, a
fuse, a switch (press it), or two ends of a solder joint — anything that should
conduct. Complete → LED on. Broken → LED off.

---

## 5. When it doesn't work

> ### 🔴 Check the baud dropdown FIRST
>
> **Garbled output (`?? ? ??`) or nothing at all is, nine times out of ten, the Serial
> Monitor's baud dropdown sitting at 9600.** The sketch transmits at **115200**. They
> must match, or every byte is sliced at the wrong bit boundary and decodes to garbage.
>
> The Arduino IDE **defaults it to 9600 and silently resets it back to 9600** on
> reconnect — so it can revert after an upload, and it looks like your board broke.
>
> Measured on this board: at 9600 the output is pure garbage; at 115200 *the very same
> bytes* decode perfectly. It is one dropdown. Check it before you suspect anything else.

| Symptom | Cause |
|---|---|
| **Garbled characters** (`?? ? ??`) | **Baud dropdown is at 9600. Set it to 115200.** See above. |
| **Serial Monitor is blank / "nothing on contact"** | Same: **wrong baud**. (Or: the sketch prints only on *change*, so silence with the probes untouched is normal.) |
| `Waiting for gdb server to start...` / `unable to open ftdi device` | You clicked **Debug** (🐞), not **Upload** (**→**). Debug needs a JTAG probe this board doesn't have — see below. Click the **→** arrow. |
| Board reboots when you touch the probes; boot noise (`SPI_FAST_FLASH_BOOT`, `configsip:`) shredding your output | Seen on this board with the IDE monitor. If it bothers you, `python3 learn/monitor.py` doesn't do it — see [below](#️-if-the-ide-monitor-reboots-the-board-when-you-touch-the-probes). |
| Upload: *"chip stopped responding"* | **Upload Speed isn't 115200.** See above. |
| Upload: *"port is busy"* | The **Serial Monitor is holding the port.** Close it, then upload. Only one program can own the port. |
| No port listed at all | Board unplugged, or a charge-only USB cable. It must be a **data** cable. |
| Always says CONTINUITY, even with probes apart | Probe A is in a **GND** pin, or the two jumpers are touching. |
| Always says open, even with probes touching | Probe B isn't actually in GND; or a jumper isn't seated. |
| Reading flickers randomly | You picked an **input-only** pin: **GPIO 34, 35, 36, 37, 38, 39** have **NO internal pull-up** at all. `INPUT_PULLUP` silently does nothing there and the pin floats. **Use pin 32.** |

That input-only one is the classic ESP32 trap, and no generic Arduino tutorial warns you
about it — on a real Arduino Uno, every pin has a pull-up. Note it's the **GPIO number**
that matters, not the `Ax` silkscreen label: those labels map to different GPIOs on
different Feather revisions, so always check the number.

### ⚠️ If the IDE monitor reboots the board when you touch the probes

**First make sure the baud dropdown is 115200** (see above) — that accounts for most of
what looks like this. But if, at the *correct* baud, you still see your output shredded
by boot messages every time you complete the circuit:

```
ff]
        circuit BROKEN    pin32=HIGH  LED=oPI_FAST_FLASH_BOOT)
configsip: 271414342, SPIWP:0xee
clk_drv:0
```

**your wiring and your code are fine — the chip is rebooting.**

Each reset re-runs `setup()`, and the `lastState = -1` sentinel forces one print. So you
get: reboot → banner → one status line → reboot → ... forever. It looks completely broken.

**This was measured, not guessed.** Same board, same wires, same sketch, same hand,
insulated leads (no skin on any conductor) — the *only* variable changed was the program
reading the serial port:

| Serial program | 10 probe touches |
|---|---|
| Arduino IDE Serial Monitor | constant reboots, garbled output |
| `learn/monitor.py` (holds DTR/RTS low) | **10 clean contacts, 10 breaks, 0 reboots** |

**Why:** on the ESP32, the USB-serial chip's **DTR** and **RTS** lines are wired to the
chip's **EN (reset)** and **GPIO0 (boot)** pins — that's the circuit that lets the IDE
auto-reset the board to flash it without you pressing buttons. The Arduino Serial Monitor
drives those lines; `monitor.py` deliberately holds them low and leaves them alone. That
difference is what the measurement above isolates. (The exact electrical path by which
making the probe contact then tips EN isn't fully pinned down — but it is *not* your
hands, and it is *not* your wiring.)

**A fallback if it bothers you** — a terminal monitor that doesn't do this:

```bash
pip install pyserial          # one time
python3 learn/monitor.py
```

Ctrl-C to quit. **Close it before uploading** — only one program can own the port.

This is entirely optional, and it's *not* the main path: the Arduino IDE Serial Monitor
is the tool this project is meant to teach, and at **115200** it works. Use `monitor.py`
only if the reboots make the IDE monitor impractical for you, or when you want to script
against the board.

One honest caveat: `monitor.py` **does** reboot the board once, as it opens the port —
macOS asserts DTR on open no matter what we ask for. That single reset is harmless, and
it's what prints the fresh `Continuity Tester` banner immediately. What it doesn't do is
reset the chip *while you're probing*.

### Optional: prove it with one command

```bash
python3 learn/verify.py
```

Touch the probes together and apart. It watches the stream and exits **PASS** once it has
seen the banner, a contact made, a contact broken, the counter increment, and zero
mojibake — or **FAIL** with the specific reason. Same thing you can confirm by eye in the
Serial Monitor; this just makes it a pass/fail check.

### Is it a *real* reset loop?

Don't let the above train you to ignore every reboot. Tell them apart:

- **Reboots only when you touch the probes / connect the monitor** → the Serial Monitor
  issue above. Use `monitor.py`.
- **Banner re-printing every second or two while you sit still and touch nothing** → that
  is a **genuine** reset loop. Read the `rst:0x…` reason in the boot dump:
  `BROWNOUT` (weak USB port/cable), `SW_CPU_RESET` (a crash in your code), or a probe
  shorting a strapping pin. That one is a real fault — chase it.

### Don't press the Debug button

The three toolbar buttons, top-left:

| Button | What it does |
|---|---|
| **✓** checkmark | **Verify** — compile only, don't touch the board |
| **→** right arrow | **Upload** — compile *and* flash. **This is the one you want.** |
| 🐞 bug | **Debug** — hardware JTAG debugging |

**Debug won't work on this board as-is.** It launches OpenOCD, which talks to a physical
**JTAG probe** over a debug interface — and this board doesn't expose one.

To be precise, because the distinction matters: the ESP32 *chip* does have JTAG (on
GPIO12–15). What the **Feather ESP32 V2** lacks is any built-in **debug interface** to
reach it — the board carries only a CH34x USB-serial bridge, which can flash and print
but cannot do JTAG. (The classic ESP32 also lacks the built-in USB-JTAG that newer S3/C3
chips have.) The `unable to open ftdi device` error is OpenOCD following the ESP32 core's
default config, which expects an FTDI-based adapter like the one on an ESP-WROVER-KIT.

So debugging *is* possible — but only by buying an external JTAG probe and wiring it up.
For this project, don't.

Which means on this board, **`Serial.println()` IS your debugger.** That's not a
consolation prize — printing values and watching them is how most embedded debugging
genuinely gets done, and it's exactly why this sketch reports its state over serial.

---

## 6. Make it yours

Small changes, each teaching one thing:

1. **Invert the LED** so it's on when the circuit is *open*. (One character. Do you
   understand *why* it's one character?)
2. **Change the sense pin** to `33`. Move the jumper. It should just work — then try
   `34` and watch it break, and reread the input-only trap above.
   ⚠️ **Don't use pin `27`.** On this Feather that pin drives the 5990's IR emitter. You
   should have the IR board unplugged (see the note at the top) — but if it *is* still
   attached, `INPUT_PULLUP` on 27 would hold the emitter's gate high and run the IR LEDs
   continuously.
3. **Blink instead of solid**: on continuity, blink the LED rather than holding it on.
   Now you need a variable that remembers time — meet `millis()`.
4. **Add a beep counter**: print how many times continuity was made. This teaches
   *state* — remembering something between passes through `loop()`.
5. **Measure how long** contact was held, in milliseconds, and print it on release.

Each one forces you to touch a new concept: boolean logic, pin config, timing, state.
That's the whole game.
