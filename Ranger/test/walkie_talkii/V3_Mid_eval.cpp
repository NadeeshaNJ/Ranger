/*
  Walkie-Talkie over LoRa -- V2, same behaviour as V1_noisy.cpp, rewritten
  against the lib/ libraries.

  Functionally identical to V1: half-duplex PTT, Codec2 1300, 4 frames per
  packet, capture in its own task with a drop-oldest queue, receive squelch.
  What changed is that every piece of hardware handling now lives behind a
  library, so this file is only the state machine:

    Mic      lib/Mic       I2S capture and 32->16 bit conversion
    Sound    lib/Sound     I2S playback, start/stop, tone generator
    Radio    lib/Radio     LoRa modem setup and packet I/O
    Codec    lib/Codec     Codec2 handle, one per direction
    FrameQueue lib/Codec   drop-oldest queue between the two tasks
    Button   lib/Buttons   debounced PTT

  V1 is kept for reference because it is the version whose behaviour was
  actually observed on hardware; this one should behave the same.
*/

#include <Arduino.h>

#include <Wire.h>

#include <Buttons.h>
#include <Codec.h>
#include <FrameQueue.h>
#include <Keypad.h>
#include <Mic.h>
#include <Radio.h>
#include <Screen.h>
#include <Sound.h>
#include <UI.h>

// ---- Pins ----
#define MIC_SCK_PIN   32
#define MIC_WS_PIN    25
#define MIC_SD_PIN    33

#define SPK_BCK_PIN   27
#define SPK_WS_PIN    26
// DIN must NOT be 25 -- that is the mic's WS line. One GPIO cannot be driven
// by two I2S peripherals; the second gets no data path and stays silent.
#define SPK_DIN_PIN   15

#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  4

// GPIO34-39 are input-only with no internal pull-up, so a button on one of
// them floats and reads as permanently pressed. GPIO12 has a real pull-up.
#define PTT_PIN      12
#define DEBOUNCE_MS  50

// I2C bus, shared by the SSD1306 OLED and the PCF8575 button expander.
#define I2C_SDA_PIN  21
#define I2C_SCL_PIN  22

// PCF8575 interrupt. The tested button sketch used GPIO4, but that is LoRa
// DIO0 on this board -- wire INT to GPIO13 instead or the radio and the
// keypad fight over the same pin.
#define PCF_INT_PIN  13
#define PCF_ADDRESS  0x20

// ---- Timing ----
#define RX_REPORT_MS   1000
// A sender releasing PTT just goes quiet -- there is no end-of-transmission
// marker -- so the receiver has to notice the silence itself. One packet is
// ~160 ms of audio, so 400 ms rides out a dropped packet without cutting a
// real transmission short.
#define RX_SQUELCH_MS  400

// ---- Audio ----
#define SAMPLE_RATE       8000
#define CODEC2_MODE       CODEC2_MODE_1300
#define FRAME_SAMPLES     320   // 40 ms
#define FRAME_BYTES       7

// Batching 4 frames gives ~6.25 packets/sec at 28 bytes, which the link
// carries at roughly 23% duty cycle. Cost is ~160 ms of latency each way.
#define FRAMES_PER_PACKET 4
#define PACKET_BYTES      (FRAME_BYTES * FRAMES_PER_PACKET)
#define FRAME_QUEUE_LEN   (FRAMES_PER_PACKET * 2)

// The PCM5100A is line level with no amplifier, so this only maximises the
// signal handed to whatever amp follows -- it cannot make a bare speaker loud.
#define PLAYBACK_GAIN  6

Mic mic;
Sound speaker;
Radio radio;
Button ptt;
Screen screen;
Keypad keypad;
UI ui;

// One Codec per direction, never shared between tasks. A CODEC2 handle carries
// mutable per-frame state that both encode and decode write through; sharing
// one across cores corrupts it. See lib/Codec/src/Codec.h.
Codec encoder;   // micTask / core 0
Codec decoder;   // loop()  / core 1

FrameQueue<FRAME_BYTES, FRAME_QUEUE_LEN> frames;
TaskHandle_t micTaskHandle = NULL;

volatile bool captureEnabled = false;
volatile uint32_t framesCaptured = 0;
volatile uint32_t shortReads = 0;

int16_t pcmFrame[FRAME_SAMPLES];      // micTask only
int16_t decodedFrame[FRAME_SAMPLES];  // loop() only
uint8_t packetBuf[PACKET_BYTES];      // loop() only

// Every init step is fatal if it fails -- there is no degraded mode for a
// radio with no mic. Halting loudly beats continuing into undefined behaviour.
static void require(bool ok, const char *what) {
  if (ok) return;
  Serial.printf("%s failed, halting.\n", what);
  while (true) delay(1000);
}

// Capture side, pinned opposite loop() so a blocking transmit on one core
// cannot delay the mic on the other. readFrame() blocks until a frame exists,
// which paces this at 25 Hz and yields for free -- the mic is never unread.
void micTask(void *param) {
  (void)param;

  for (;;) {
    if (!captureEnabled) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    if (!mic.readFrame(pcmFrame)) {
      shortReads++;
      continue;   // mic stalled; retry rather than emit a partial frame
    }

    uint8_t bits[FRAME_BYTES];
    encoder.encode(bits, pcmFrame);
    frames.push(bits);   // drop-oldest on overflow, counted internally
    framesCaptured++;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Walkie-talkie V2 starting...");

  ptt.begin(PTT_PIN, DEBOUNCE_MS, true);

  // Config structs default to the Codec2 voice case, so only pins are set.
  MicConfig micCfg(MIC_SCK_PIN, MIC_WS_PIN, MIC_SD_PIN);
  SoundConfig spkCfg(SPK_BCK_PIN, SPK_WS_PIN, SPK_DIN_PIN);
  RadioConfig radioCfg(LORA_SS, LORA_RST, LORA_DIO0);

  require(encoder.begin(CODEC2_MODE) && decoder.begin(CODEC2_MODE), "Codec.begin");
  require(encoder.geometryMatches(FRAME_SAMPLES, FRAME_BYTES), "Codec2 geometry");
  require(mic.begin(micCfg), "Mic.begin");
  require(speaker.begin(spkCfg), "Sound.begin");
  require(radio.begin(radioCfg), "Radio.begin");
  require(frames.begin(), "FrameQueue.begin");

  // I2C first: both the OLED and the button expander sit on this bus, so it
  // is brought up once here rather than by each device.
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  KeypadConfig keyCfg;
  keyCfg.i2cAddress = PCF_ADDRESS;
  keyCfg.intPin = PCF_INT_PIN;
  keyCfg.debounceMs = DEBOUNCE_MS;
  // Buttons mounted P0..P4 = up, down, left, right, enter.
  keyCfg.upPin = 0;
  keyCfg.downPin = 1;
  keyCfg.leftPin = 2;
  keyCfg.rightPin = 3;
  keyCfg.enterPin = 4;

  // The UI is not load-bearing for the radio: if the panel or expander is
  // missing, the walkie-talkie still has to work. These are warnings, not
  // require() calls.
  if (!screen.begin()) Serial.println("Screen.begin failed -- running headless.");
  if (!keypad.begin(keyCfg)) Serial.println("Keypad.begin failed -- no menu input.");
  ui.begin(&screen, &keypad);

  Serial.printf("Codec2 ready: %d samples -> %d bytes per frame\n",
                encoder.samplesPerFrame(), encoder.bytesPerFrame());

  // Core 0, above loop()'s priority so a ready frame encodes promptly. Stack
  // size from Codec.h -- codec2 keeps large FFT buffers on the caller's stack,
  // not in its handle, and undersizing this trips the canary mid-encode.
  require(xTaskCreatePinnedToCore(micTask, "micTask", CODEC_ENCODE_STACK,
                                  NULL, 2, &micTaskHandle, 0) == pdPASS,
          "micTask create");

  Serial.println("Ready. Hold the PTT button to talk.");
}

void loop() {
  static bool wasTalking = false;
  static unsigned long lastReport = 0;
  static unsigned long lastPacketMs = 0;
  static int packetsSinceReport = 0;
  static long bytesSinceReport = 0;

  bool talking = ptt.isPressed();

  if (talking && !wasTalking) {
    speaker.stop();      // idempotent
    frames.flush();      // discard pre-press audio
    mic.flush();
    frames.resetCounters();
    framesCaptured = 0;
    shortReads = 0;
    captureEnabled = true;
    ui.setAudioActive(true, true);   // freeze the menu, show the TX banner
    ui.render();                     // one redraw now, none while transmitting
    Serial.println("PTT pressed, transmitting...");
  }

  if (!talking && wasTalking) {
    captureEnabled = false;
    Serial.printf("PTT released: %u captured, %u dropped, %u short reads\n",
                  (unsigned)framesCaptured, (unsigned)frames.dropCount(),
                  (unsigned)shortReads);
    // High-water marks are the smallest free stack ever seen. If either nears
    // zero the canary is about to trip -- see it coming instead of crashing.
    Serial.printf("  stack free: micTask %u B, loop %u B\n",
                  (unsigned)uxTaskGetStackHighWaterMark(micTaskHandle),
                  (unsigned)uxTaskGetStackHighWaterMark(NULL));
    frames.flush();
    ui.setAudioActive(false, false);   // back to the menu
    // Speaker deliberately left stopped: the receive path starts it on the
    // first arriving packet and the squelch stops it when they stop.
    Serial.println("Listening...");
  }
  wasTalking = talking;

  if (talking) {
    // ---- TRANSMIT: drain encoded frames -> LoRa ----
    // No mic access here; micTask keeps filling the queue throughout the
    // blocking send below.
    int ready = 0;
    while (ready < FRAMES_PER_PACKET) {
      if (!frames.pop(&packetBuf[ready * FRAME_BYTES], 200)) break;
      ready++;
      if (!ptt.isPressed()) break;   // released mid-batch, stop keying
    }

    if (ready > 0) radio.sendPacket(packetBuf, ready * FRAME_BYTES);

    // sendPacket blocks; yield so IDLE1 runs and the Timer Group watchdog
    // stays quiet (TG1WDT_SYS_RESET without this).
    vTaskDelay(1);
    return;
  }

  // ---- RECEIVE: LoRa -> decode -> DAC ----
  int packetSize = radio.receivePacket(packetBuf, PACKET_BYTES);
  if (packetSize > 0) {
    lastPacketMs = millis();
    bool firstOfTransmission = !speaker.isRunning();
    speaker.start();   // idempotent; zeroes the ring on the real transition

    if (firstOfTransmission) {
      // Redraw only on the transition into RX, not per packet -- a full panel
      // flush costs several ms and packets arrive every ~160 ms.
      ui.setLinkStats(radio.lastRssi(), radio.lastSnr());
      ui.setAudioActive(true, false);
      ui.render();
    }

    int framesIn = packetSize / FRAME_BYTES;
    if (framesIn > FRAMES_PER_PACKET) framesIn = FRAMES_PER_PACKET;

    packetsSinceReport++;
    bytesSinceReport += packetSize;

    // No delay between frames: they are contiguous audio and a tick of silence
    // between each is audible. playMono blocks on the DMA ring, which yields
    // and paces this at real playback speed.
    for (int f = 0; f < framesIn; f++) {
      decoder.decode(decodedFrame, &packetBuf[f * FRAME_BYTES]);
      speaker.playMono(decodedFrame, FRAME_SAMPLES, PLAYBACK_GAIN);
    }
  }

  unsigned long now = millis();

  // Squelch: a gap in packets is the only signal the sender stopped talking.
  if (speaker.isRunning() && (now - lastPacketMs >= RX_SQUELCH_MS)) {
    speaker.stop();
    ui.setAudioActive(false, false);   // back to the menu
    Serial.println("Squelch: transmission ended, speaker muted.");
  }

  // Menu input and redraw only run here, on the idle path. Both are no-ops
  // while audio is active, so neither can steal time from a decode.
  ui.update();
  ui.render();

  if (now - lastReport >= RX_REPORT_MS) {
    lastReport = now;
    if (packetsSinceReport > 0) {
      Serial.printf("RX %d pkt/s, %ld B, RSSI %d dBm, SNR %.1f\n",
                    packetsSinceReport, bytesSinceReport,
                    radio.lastRssi(), (double)radio.lastSnr());
    } else if (frames.dropCount() > 0 || shortReads > 0) {
      Serial.printf("RX idle (last TX: %u dropped, %u short reads)\n",
                    (unsigned)frames.dropCount(), (unsigned)shortReads);
    } else {
      Serial.println("RX idle (no packets)");
    }
    packetsSinceReport = 0;
    bytesSinceReport = 0;
  }
}
