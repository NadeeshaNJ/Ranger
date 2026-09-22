# Ranger (Phase 2 firmware)

LoRa handheld: push-to-talk voice, text messages, positions, SOS. ESP32-WROOM-32E.

## Folders

```
Ranger/
├── platformio.ini      settings + one "env" per test
├── src/main.cpp        the final program: starts the modules, passes data between them
├── src/tests/          one small test per module
└── lib/                the modules, each = one .h + one .cpp
```

## How to use

1. VS Code: **File > Open Folder > Ranger**. PlatformIO downloads the libraries by itself.
2. Click the environment name in the blue status bar and pick one, e.g. `env:test_mic`.
3. **Upload**, then **Monitor**. Each test says at the top what it does.

## Pins

| Part | Pins |
|---|---|
| Mic ICS-43434 (I2S 0) | SCK IO32, WS IO25, SD IO33 |
| Speaker MAX98357A (I2S 1) | BCLK IO27, LRC IO26, DIN IO17, SD_MODE = expander P15 |
| I2C (expander + OLED) | SDA IO21, SCL IO22 |
| Expander PCF8575 (0x20) | keys 0-9, VOICE (P17), knob push (P14), amp (P15) |
| Knob EC12D | A IO39, B IO36 |
| OLED SSD1306 (0x3C) | on I2C |
| LoRa E28 (SX1280) | SCK IO18, MISO IO19, MOSI IO23, NSS IO5, BUSY IO34, DIO1 IO35, RST IO15, RX_EN IO2, TX_EN IO12 |
| GPS BZ-251 (UART2, 5 V) | GPS TX -> IO13, GPS RX <- IO4 |
| Battery | IO14 through 100k/100k divider |

## Modules

| Module | What it does | Main functions |
|---|---|---|
| **Mic** | Reads 8 kHz sound | `begin()`, `read(buf, n)`, `sleep()`, `wake()` |
| **Speaker** | Plays sound and tones | `begin()`, `play(buf, n)`, `tone(hz, ms)`, `playTone(1..4)`, `setVolume()` |
| **Controls** | Keys, VOICE, knob, amp on/off | `begin()`, `poll(event)`, `isDown(key)`, `setAmp(on)` |
| **Display** | OLED screens (Lopaka designs) | `clear()`, `statusBar()`, `menu()`, `people()`, `track()`, `chat()`, ... `show()` |
| **UI** | Menus, typing, push-to-talk | `begin()`, `update()`, `onAction(fn)`, `setPeople()`, `setStatus()` |
| **Radio** | LoRa send / receive | `begin()`, `send()`, `receive()`, `busy()`, `setChannel()` |
| **Net** | Messages between devices, relaying | `begin()`, `update()`, `sendText()`, `sendVoice()`, `track()`, `ping()`, `sendSos()` |
| **Crypto** | AES-128 encryption for Net | `begin()`, `setPassphrase()`, `fingerprint()` |
| **Codec** | Codec2 voice compression | `begin(mode)`, `encode()`, `decode()`, `conceal()` |
| **Voice** | Push-to-talk audio (own task) | `begin()`, `update()`, `talk(on)`, `onNetEvent()` |
| **Gps** | Position, satellites, time | `begin()`, `update()`, `hasFix()`, `lat()`, `lon()`, `timeText()`, `bars()` |
| **Battery** | Voltage and % | `begin()`, `volts()`, `percent()` |

## How they connect

```
keys/knob -> Controls -> UI -> Display
                          |
                    actions (talk, send, track...)
                          v
Mic -> Codec -> Voice -> Net (+ Crypto) -> Radio  ~~~ air ~~~  other devices
Speaker <- Codec <- Voice <-'
Gps, Battery -> status bar and positions
```

## Before flashing the final program (`env:ranger`)

In `src/main.cpp` set `MY_NAME` (different on each device) and `PASSPHRASE` (the same on every device).

## Good to know

* **Keys** (from the silkscreen): 2 up, 8 down, 4 back, 6 action, 5 select. Knob turn = move, press = select.
  Typing uses T9 (press 2 twice = b); knob left = delete, knob press = send.
* **Codec2 needs a 32 KB stack.** Voice runs it in its own task. In your own code, never call Codec2 from a normal `loop()`.
* **Every device must use the same channel** (Settings) and, with encryption, the same passphrase. Compare `fingerprint()`.
* **Net**: text is confirmed (up to 4 tries), voice is not (a late sound is useless). Every device relays for others; voice is not relayed.
  Positions are only sent while someone tracks you, every 20 s.
* **GPS** finds its own baud rate (115200 or 38400). Time is shown in Sri Lanka time (UTC+5:30).
* **Battery** 0% = 3.6 V. If the device switches off at another voltage, change the last row in `Battery.cpp`.
* **Knob turns the wrong way?** Swap `ENC_A` and `ENC_B` in `Controls.cpp`.
* **Key 9** silkscreen says "wxys"; the firmware uses wxyz.
