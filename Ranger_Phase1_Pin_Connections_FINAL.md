# Ranger Phase 1 — Full Pin Connection Reference (Corrected)

Board: ESP32 DevKit v1

---

## LoRa — Ra-01 / Ra-02 (SX1278, 433MHz)

| Module Pin | ESP32 GPIO |
|---|---|
| NSS (SS) | GPIO5 |
| RST | GPIO14 |
| DIO0 | GPIO35 |
| SCK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| VCC | 3.3V |
| GND | GND |

Confirm physical module is 433MHz before powering on; firmware `LoRa.begin()` must match.
DIO0 moved from GPIO2 to GPIO35 (GPIO2 is a strapping pin). Update `LoRa.setPins()` and
any `attachInterrupt()` call to GPIO35. GPIO35 is input-only, which is fine since DIO0
is only ever read, never driven.

No BUSY, RX_EN, or TX_EN pins exist on this module.

---

## Microphone — INMP441 (I2S input)

| INMP441 Pin | ESP32 GPIO |
|---|---|
| SCK | GPIO32 |
| WS | GPIO25 |
| SD | GPIO33 |
| L/R | GND |
| VDD | 3.3V |
| GND | GND |

Firmware: `I2S_CHANNEL_FMT_ONLY_LEFT` at 8kHz, `use_apll=true`. If samples read as
all-zero (known ESP32 Arduino I2S driver bug), fall back to
`I2S_CHANNEL_FMT_RIGHT_LEFT` (stereo) and extract the left channel manually. Apply
`>> 14` shift to bring 24-bit-in-32-bit samples into range.

---

## Audio Output — WeAct PCM5100A I2S DAC

| PCM5100A Pin | ESP32 GPIO / Net |
|---|---|
| WS | GPIO26 |
| BCK | GPIO27 |
| DIN | GPIO25 |
| VCC | 3.3V |
| GND | GND |
| MC | Not connected |
| SD | Not connected |

Confirmed via WeAct's own repo usage pattern and an independent real-world build using
this exact module: MC and SD are left unwired in normal operation. GPIO25 is shared
between the mic's WS line and the DAC's DIN line — this is **not a conflict**, since mic
and DAC use separate I2S peripherals/buses on the ESP32.

Optimized for 4Ω loads; an 8Ω speaker will get reduced output power, expected behavior,
not a fault.

---

## GPIO Expander — PCF8575 (I2C)

| PCF8575 Pin | ESP32 GPIO / Net |
|---|---|
| SDA | GPIO21 |
| SCL | GPIO22 |
| A0/A1/A2 | GND (address 0x20) |
| INT | Free GPIO of choice |
| VCC | 3.3V |
| GND | GND |
| P0x / P1x | Keypad matrix rows/columns |

INT fires on both press and release — firmware must track previous pin state. Buttons
pull LOW to GND; no external pull-ups needed (PCF8575 internal weak pull-up handles it).

---

## Display — SSD1306 128×64 OLED (I2C)

| SSD1306 Pin | ESP32 GPIO / Net |
|---|---|
| SDA | GPIO21 |
| SCL | GPIO22 |
| VCC | 3.3V |
| GND | GND |

Address 0x3C, used with U8g2 library. Shares bus/pull-ups with PCF8575.

---

## GPS — NEO-M8N Breakout

| NEO-M8N Pin | ESP32 GPIO |
|---|---|
| TX | GPIO16 (ESP32 RX) |
| RX | GPIO17 (ESP32 TX) |
| VCC | 3.3V or 5V per module spec |
| GND | GND |

Confirm against actual firmware UART instance/pins in use.

---

## Full GPIO Usage Summary

| GPIO | Function |
|---|---|
| GPIO5 | LoRa NSS |
| GPIO14 | LoRa RST |
| GPIO35 | LoRa DIO0 (input-only) |
| GPIO18 | LoRa SCK |
| GPIO19 | LoRa MISO |
| GPIO23 | LoRa MOSI |
| GPIO32 | Mic SCK |
| GPIO33 | Mic SD |
| GPIO25 | Mic WS **and** DAC DIN (separate I2S buses, confirmed not a conflict) |
| GPIO26 | DAC WS |
| GPIO27 | DAC BCK |
| GPIO21 | I2C SDA (PCF8575 + OLED) |
| GPIO22 | I2C SCL (PCF8575 + OLED) |
| GPIO16 | GPS TX (typical, confirm in firmware) |
| GPIO17 | GPS RX (typical, confirm in firmware) |
| (free choice) | PCF8575 INT |

---

*Reflects confirmed Phase 1 breadboard wiring as of latest project memory.*
