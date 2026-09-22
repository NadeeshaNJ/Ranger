// Crypto: AES-128-CCM for Net (encrypts + detects changed/fake/replayed packets).
// One shared key for the group, made from a passphrase, saved in flash.
// Sealed payload = [epoch 4][ciphertext][tag 8]  -> 12 extra bytes per packet
#pragma once
#include <Arduino.h>
#include <mbedtls/ccm.h>
#include "Net.h"

class Crypto : public NetCipher {
public:
  bool begin(uint8_t network = 0x52);       // loads the saved key; false if there is none yet
  bool setPassphrase(const char* text);     // same text on every device = same key
  bool hasKey() { return _hasKey; }
  void fingerprint(char out[9]);            // 8 hex chars: must match on every device

  uint8_t overhead() override { return 12; }
  bool seal(const uint8_t* aad, const uint8_t* in, size_t len, uint8_t* out, size_t& outLen) override;
  bool open(const uint8_t* aad, const uint8_t* in, size_t len, uint8_t* out, size_t& outLen) override;

private:
  bool useKey(const uint8_t key[16]);
  void makeNonce(uint8_t n[13], uint32_t epoch, const uint8_t* aad);
  bool isNew(uint16_t from, uint64_t counter);

  mbedtls_ccm_context _ccm;
  bool     _hasKey = false;
  uint8_t  _key[16], _net = 0;
  uint32_t _boot = 0;                        // +1 every power-up (saved in flash)
  uint8_t  _wrap = 0;                        // +1 each time my 16-bit seq wraps
  uint16_t _lastSeq = 0;
  struct { uint16_t from; uint64_t top, mask; } _seen[8] = {};   // replay check per sender
  uint8_t  _seenNext = 0;
};
