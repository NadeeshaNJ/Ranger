# Ranger Phase 2 PCB

## Overview

Ranger Phase 2 PCB with ESP32-WROOM-32E (4MB) microcontroller, featuring LoRa communication, OLED display, microphone input, speaker output, and extended I/O via PCF8575 expanders.

## Pin Connection Reference

### ESP32-WROOM-32E Full Pin Map

| Module Pin | ESP32 Signal | Net / Connected To | Notes |
|---|---|---|---|
| 1 | GND | GND | |
| 2 | 3V3 | 3.3V | C6 22uF + C2 100nF bulk decoupling |
| 3 | EN | ESP32_EN | R1 10k pull-up to 3.3V, C1 1uF EN-to-GND |
| 4 | IO36 (SENSOR_VP) | EncoderA | Input-only, PCNT quadrature decode, needs external 10k pull-up to 3.3V |
| 5 | IO39 (SENSOR_VN) | EncoderB | Input-only, PCNT quadrature decode, needs external 10k pull-up to 3.3V |
| 6 | IO34 | LoRa_BUSY | Input-only |
| 7 | IO35 | LoRa_DIO1 | Input-only |
| 8 | IO32 | MicSCK | |
| 9 | IO33 | MicSD | |
| 10 | IO25 | MicWS | |
| 11 | IO26 | Sound_LRC | |
| 12 | IO27 | Sound_BCLK | |
| 13 | IO14 | LoRa_RST | |
| 14 | IO12 | DAC_DIN | |
| 15 | GND | GND | |
| 16 | IO13 | LoRa_TX_EN | |
| 17–22 | NC | — | Not connected (internal SPI flash on D0WD-V3) |
| 23 | IO15 | — | FREE (strapping pin) |
| 24 | IO2 | — | FREE (strapping pin) |
| 25 | IO0 | ESP32_IO0 | Boot button net, R2 10k pull-up. Reserved, do not reuse |
| 26 | IO4 | LoRa_RX_EN | |
| 27 | IO16 | IO_INT | Shared interrupt line — both PCF8575 expanders wire-ORed here (open-drain), one shared 10k pull-up |
| 28 | IO17 | Sound_DIN | |
| 29 | IO5 | LoRa_SPI_SS | Strapping pin, handled via auto-reset circuit pull-ups |
| 30 | IO18 | SPI_SCK | |
| 31 | IO19 | SCK_MOSI | Net name kept as-is on schematic (known label mismatch, not yet renamed) |
| 32 | NC | — | |
| 33 | IO21 | SDA | I2C data, R6 4.7k pull-up to 3.3V |
| 34 | RXD0 | CH343P_TX | Reserved for UART0 / flashing |
| 35 | TXD0 | CH343P_RX | Reserved for UART0 / flashing |
| 36 | IO22 | SCL | I2C clock, R5 4.7k pull-up to 3.3V |
| 37 | IO23 | SCK_MISO | |
| 38 | GND | GND | |

**Free GPIOs remaining after all assignments: IO15, IO2** (both strapping pins, usable with boot-state care).

---

### LoRa Module — EBYTE E28-2G4M27SX (SX1281, 2.4GHz)

| E28 Pin | Function | ESP32 Net |
|---|---|---|
| 1 | VCC | 3.3V (C8 1000uF bulk cap, + to 3.3V; add 100nF+1uF ceramic decoupling — open item) |
| 2, 7, 10, 16 | GND | GND (all 4 tied) |
| 3 | MISO_TX | IO19 (SCK_MOSI net — mislabeled, rename to SPI_MISO) |
| 4 | MOSI_RX | IO23 (SCK_MISO net — mislabeled, rename to SPI_MOSI) |
| 5 | SCK_RTSN | IO18 (SPI_SCK) |
| 6 | NSS_CTS | IO5 (LoRa_SPI_SS) — optional 10k pull-up open item |
| 8 | RX_EN | IO4 (LoRa_RX_EN) — needs 10k pull-down so PA/LNA default off at boot (open item) |
| 9 | TX_EN | IO13 (LoRa_TX_EN) — needs 10k pull-down (open item) |
| 11 | NRESET | IO14 (LoRa_RST) — internal 50k pull-up on module |
| 12 | BUSY | IO34 (LoRa_BUSY) — input-only, correct choice |
| 13 | DIO1 | IO35 (LoRa_DIO1) — input-only, correct choice |
| 14 | DIO2 | Not yet assigned |
| 15 | DIO3 | Not yet assigned |

Bulk cap: Panasonic 16SVPF1000M polymer (LCSC C236289, 1000uF 16V, 12mOhm ESR, 5.4A ripple).
Firmware: requires RadioLib SX1280 class (not Arduino-LoRa, which targeted the Phase 1 SX1278/433MHz module).

---

### Microphone — TDK InvenSense ICS-43434 (U4)

| ICS-43434 Pin | ESP32 Net |
|---|---|
| SCK | IO32 (MicSCK) |
| WS | IO25 (MicWS) |
| SD | IO33 (MicSD) |
| VDD | 3.3V |
| GND | GND |
| L/R | GND (left-channel select) |

Pinout fully verified against datasheet: WS/LR/GND/SCK/VDD/SD all correct.

---

### Speaker Amp — MAX98357A (U5, TQFN)

| MAX98357A Pin | Connected To | Notes |
|---|---|---|
| DIN | IO17 (Sound_DIN) | |
| LRCLK | IO26 (Sound_LRC) | |
| BCLK | IO27 (Sound_BCLK) | |
| GAIN_SLOT | GND | 12dB fixed gain |
| SD_MODE | 3.3V | Enable + left channel select |
| VDD (pins 7/8) | VBAT (raw battery, not regulated 3.3V) | For louder output; 22uF+100nF ceramic decoupling at VDD |
| OUTP / OUTN | Speaker+ / Speaker− | |
| EP (pin 17) | GND | Thermal pad |

Note: SD_MODE cannot exceed VDD as battery sags, *only if* the 3.3V regulator is a plain buck/LDO. Revisit if a buck-boost regulator is chosen for the power tree.

---

### PCF8575 #1 — Keypad Expander (U6, I2C address 0x20)

| PCF8575 Pin | Function | Connected To |
|---|---|---|
| 1 | INT# | IO_INT net (shared with PCF8575 #2) — needs 10k pull-up to 3.3V (was flagged missing, confirm added) |
| 2 | A1 | GND |
| 3 | A2 | GND |
| 4–11 | P00–P07 | Button_0–Button_7 (direct-wired, 1:1, no matrix, no diodes needed) |
| 12 | GND | GND |
| 13–20 | P10–P17 | Button_8, Button_9, Button_Left, Button_Middle, Button_Right, Button_Top, Button_Voice, Button_Bottom |
| 21 | A0 | GND |
| 22 | SCL | Shared I2C bus (R5 4.7k pull-up) |
| 23 | SDA | Shared I2C bus (R6 4.7k pull-up) |
| 24 | VCC | 3.3V, 100nF decoupling (C10) |

All 16 I/O pins consumed by 16 direct-wired buttons — no room for status LEDs on this expander.

---

### PCF8575 #2 — Aux Expander (U7, I2C address 0x21)

| PCF8575 Pin | Function | Connected To |
|---|---|---|
| 1 | INT# | IO_INT net (shared with PCF8575 #1), R9 10k pull-up to 3.3V |
| 2 | A1 | GND |
| 3 | A2 | GND |
| 4 | P00 | Knob_PushDown (encoder push switch, terminal D) |
| 5–11 | P01–P07 | Free for future use |
| 12 | GND | GND |
| 13–20 | P10–P17 | Free for future use |
| 21 | A0 | **⚠️ OPEN ISSUE: currently wired to SCL net, must be rerouted to 3.3V to achieve address 0x21** |
| 22 | SCL | Shared I2C bus |
| 23 | SDA | Shared I2C bus |
| 24 | VCC | 3.3V, 100nF decoupling (C14) |

---

### SSD1306 OLED Display

- Shares I2C bus with both PCF8575 expanders (same SDA/SCL, same pull-up pair — do not duplicate pull-ups).
- Confirm address strap is 0x3C.
- If bare SSD1306 driver IC (not premade breakout): needs own charge pump cap (~1uF) plus separate VCC/VBAT per reference design.
- 100nF decoupling at VCC either way.

---

### Volume Knob — ALPS EC12D1524403 (SW3)

| Encoder Pin | Function | Connected To |
|---|---|---|
| A | Quadrature | ESP32 IO39 — external 10k pull-up to 3.3V, PCNT hardware quadrature decode |
| B | Quadrature | ESP32 IO36 — external 10k pull-up to 3.3V, PCNT hardware quadrature decode |
| C | Encoder common | GND |
| D | Push switch | PCF8575 #2, P00 (Knob_PushDown) — relies on PCF8575 internal pull-up |
| E | Push switch (other terminal) | GND |
| 6, 7 | Mounting/shield tabs | GND (mechanical strength + RF shielding, not floating) |

Rationale for A/B on direct GPIOs (not the expander): quadrature signals need real-time edge capture; I2C polling (~200–500us per transaction) can miss or misread fast spins. ESP32's PCNT peripheral does hardware quadrature decode with zero CPU/ISR overhead — works on any GPIO via the GPIO matrix, not a fixed subset. Push switch (D) has no timing pressure, so it fits the existing INT + state-diff firmware pattern on the expander.

---

## Open Items Still Needing Fixes

1. **PCF8575 #2, A0 (pin 21)**: currently wired to SCL net — reroute to 3.3V rail to achieve address 0x21.
2. **PCF8575 #1, INT# pull-up**: confirm 10k pull-up to 3.3V is present (was flagged missing in early review; R9 on PCF8575 #2 now serves as the *shared* pull-up for the IO_INT net, so a single shared resistor may suffice — verify only one exists, not zero).
3. **LoRa DIO2 (pin 14) / DIO3 (pin 15)**: not yet assigned to any ESP32 GPIO.
4. **LoRa RX_EN / TX_EN**: need 10k pull-downs so PA/LNA default off while ESP32 GPIOs are high-Z at boot.
5. **LoRa E28 pin 1 (VCC)**: add 100nF + 1uF ceramic decoupling alongside the existing C8 bulk cap.
6. **Net naming**: SCK_MOSI / SCK_MISO labels are swapped/mislabeled on the LoRa SPI section — rename to SPI_MISO / SPI_MOSI for clarity.
7. **CH343P section**: ground exposed pad (pin 17/EP), tie USB-C shield (EH1–EH4) to GND, remove dangling IO16 wire stub, add 5V→3.3V regulator, add ESD protection (USBLC6-2SC6) on USB D+/D-.
8. **Status LEDs**: no free pins left on PCF8575 #1 (keypad); need to use PCF8575 #2's free pins (P01–P07, P10–P17) or spare ESP32 GPIOs (IO15, IO2) instead.

---

## Development Notes

- Generated from project memory — reflects schematic state as reviewed across sessions.
- Cross-check against the live KiCad project before fabrication.
