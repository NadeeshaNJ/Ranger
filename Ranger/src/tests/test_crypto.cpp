// Crypto test: sets the key from a passphrase, encrypts a message, checks it
// decrypts, and checks that a changed packet and a replayed packet are rejected.
#include <Arduino.h>
#include "Crypto.h"

#define PASSPHRASE "rescue-team-2026"          // use the same text on every device

Crypto crypto;

void setup() {
  Serial.begin(115200);
  crypto.begin();
  crypto.setPassphrase(PASSPHRASE);
  char fp[9];
  crypto.fingerprint(fp);
  Serial.printf("key fingerprint %s (must be the same on every device)\n", fp);

  uint8_t header[10] = {2, 1, 0x52, 0x00, 0xA0, 0x02, 0xA0, 0x39, 0x30, 2};   // a Net header
  const char* msg = "Meet at the school";
  uint8_t sealed[64], plain[64];
  size_t n, m;
  crypto.seal(header, (const uint8_t*)msg, strlen(msg), sealed, n);
  bool ok = crypto.open(header, sealed, n, plain, m) && m == strlen(msg) && !memcmp(plain, msg, m);
  Serial.printf("encrypt + decrypt:      %s\n", ok ? "OK" : "FAIL");
  Serial.printf("same packet again:      %s\n", crypto.open(header, sealed, n, plain, m) ? "FAIL (accepted)" : "OK (rejected as replay)");
  header[8]++;                                                // next packet number
  crypto.seal(header, (const uint8_t*)msg, strlen(msg), sealed, n);
  sealed[6] ^= 1;                                             // change one bit
  Serial.printf("changed packet:         %s\n", crypto.open(header, sealed, n, plain, m) ? "FAIL (accepted)" : "OK (rejected)");
}

void loop() {}
