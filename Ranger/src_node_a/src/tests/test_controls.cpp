// Controls test: press keys, turn and press the knob; every event is printed.
// Serial key: a = speaker amp on/off
#include <Arduino.h>
#include "Controls.h"

Controls ctl;
const char* NAMES[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "VOICE", "KNOB"};

void setup() {
  Serial.begin(115200);
  Serial.println(ctl.begin() ? "Expander found at 0x20" : "Expander NOT found (check I2C wiring)");
}

void loop() {
  InputEvent e;
  while (ctl.poll(e)) {
    if (e.turn) Serial.printf("knob %+d\n", e.turn);
    else        Serial.printf("key %s %s\n", NAMES[e.key], e.down ? "down" : "up");
  }
  if (Serial.available() && Serial.read() == 'a') {
    static bool amp = true;
    amp = !amp;
    ctl.setAmp(amp);
    Serial.printf("amp %s\n", amp ? "on" : "off");
  }
}
