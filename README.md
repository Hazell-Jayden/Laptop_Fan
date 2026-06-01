# Laptop Cooling Pad

A custom laptop cooling pad built from scratch — custom PCB, ESP32 microcontroller, and two CORSAIR 140mm fans. Designed in Fusion 360, hand soldered, and programmed in C++.

---

## How It Works

The PCB boosts the USB 5V input up to 12V to drive the fans, while also powering the ESP32. A toggle switch turns the whole thing on, and a potentiometer lets you dial in the fan speed.

The ESP32 uses 25kHz PWM to control fan speed. Both fans are daisy-chained, so one PWM signal drives both. A N-MOS low-side switching circuit handles cutting power to the fans entirely when the pot is dialled down far enough.

Fan speed ramps smoothly rather than jumping instantly — the ramp rate adjusts based on current speed so it feels natural at both low and high ends.

---

## Hardware

| Component | Detail |
|---|---|
| Microcontroller | ESP32 (via PlatformIO / Arduino framework) |
| Fans | 2x CORSAIR 140mm PWM |
| PCB | Custom, ordered from PCBWay |
| Enclosure | PLA with threaded inserts and bolts |
| Input power | USB 5V → boosted to 12V on-board |
| Protections | Overvoltage, overcurrent, reverse polarity |

---

## Firmware Overview

Written in C++ using the Arduino framework on PlatformIO.

- **PWM** — 25kHz, 8-bit resolution on the ESP32's LEDC peripheral
- **Speed control** — pot mapped to 17–95% duty cycle with smooth ramping
- **Auto off** — pot below threshold cuts the fans; turning it back up restores them
- **Button toggle** — debounced on/off toggle with a short startup ramp when powering on
- **Pot averaging** — 8-sample average to filter ADC noise

---

## Lessons Learned

- The toggle switch is SPDT, meaning the ESP stays powered even when the fans are off — a DPDT switch would properly cut everything
- Be careful probing around 12V with a multimeter — bridged 12V onto an ESP digital pin, not ideal. Clamping diodes would help here
- Indicator LEDs visible from outside the enclosure would be a nice quality-of-life addition

---

## What's Next

- DPDT switch to fully cut power
- User-facing status LEDs
- Full enclosure redesign
- Bluetooth connectivity to read laptop temperatures and auto-adjust fan speed

---

## Tools & Suppliers

- **Design** — Fusion 360 (PCB + enclosure)
- **IDE** — VS Code + PlatformIO
- **Parts** — DigiKey
- **PCB** — PCBWay
