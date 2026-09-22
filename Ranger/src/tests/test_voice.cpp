// Voice test (flash on 2 boards): hold VOICE on one board and talk; the other plays it.
#include <Arduino.h>
#include "Controls.h"
#include "Mic.h"
#include "Speaker.h"
#include "Radio.h"
#include "Net.h"
#include "Voice.h"

Controls ctl;
Mic mic;
Speaker spk;
Radio radio;
Net net;
Voice voice;

void onNet(const NetEvent& e) { voice.onNetEvent(e); }

void setup() {
  Serial.begin(115200);
  ctl.begin();
  ctl.setAmp(true);
  mic.begin();
  spk.begin();
  radio.begin();
  net.onEvent(onNet);
  net.begin(radio, "Voice-test");
  Serial.println(voice.begin(mic, spk, net) ? "Voice ready: hold VOICE and talk" : "Voice FAILED");
}

void loop() {
  InputEvent e;
  while (ctl.poll(e)) {
    if (e.key != KEY_VOICE) continue;
    if (e.down) { spk.playTone(1); voice.talk(true); Serial.println("talking..."); }
    else        { voice.talk(false); spk.playTone(2); Serial.println("stopped"); }
  }
  net.update();
  voice.update();

  static bool wasListening = false;
  if (voice.listening() != wasListening) {
    wasListening = voice.listening();
    Serial.printf(wasListening ? "hearing %04X\n" : "quiet\n", voice.talker());
  }
}
