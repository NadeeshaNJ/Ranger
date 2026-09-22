// Net test (flash on 2 or 3 boards). Serial keys:
//   t = text to everyone    s = text to the first peer (with ACK)    p = ping the first peer
//   l = list peers
// With 3 boards: put A and C out of range of each other, B in the middle: B relays.
#include <Arduino.h>
#include "Radio.h"
#include "Net.h"

Radio radio;
Net net;

void onNet(const NetEvent& e) {
  Peer* p = net.findPeer(e.from);
  const char* who = p ? p->name : "?";
  switch (e.type) {
    case EV_PEER:   Serial.printf("peer %04X is %s\n", e.from, who); break;
    case EV_TEXT:   Serial.printf("text from %s: %.*s   (RSSI %.0f)\n", who, e.len, (const char*)e.data, e.rssi); break;
    case EV_SENT:   Serial.printf("message %u delivered\n", e.msgId); break;
    case EV_FAILED: Serial.printf("message %u FAILED\n", e.msgId); break;
    case EV_PONG:   Serial.printf("pong from %s in %lu ms\n", who, (unsigned long)e.rttMs); break;
    case EV_SOS:    Serial.printf("SOS from %s\n", who); break;
  }
}

void setup() {
  Serial.begin(115200);
  radio.begin();
  char name[13];
  snprintf(name, sizeof(name), "Unit-%04X", (uint16_t)ESP.getEfuseMac());
  net.onEvent(onNet);
  net.begin(radio, name);
  Serial.printf("I am %04X\n", net.id());
}

void loop() {
  net.update();
  if (!Serial.available()) return;
  char c = Serial.read();
  Peer* first = net.peerCount() ? net.peer(0) : nullptr;
  if (c == 't') net.sendText(NET_ALL, "hello everyone");
  if (c == 's' && first) Serial.printf("message %d sent\n", net.sendText(first->id, "hello you"));
  if (c == 'p' && first) net.ping(first->id);
  if (c == 'l')
    for (int i = 0; i < net.peerCount(); i++) {
      Peer* p = net.peer(i);
      Serial.printf("%04X %-12s %s  RSSI %.0f\n", p->id, p->name, p->direct ? "direct " : "relayed", p->rssi);
    }
}
