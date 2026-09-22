#include "Crypto.h"
#include <Preferences.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pkcs5.h>

bool Crypto::begin(uint8_t network) {
  _net = network;
  Preferences prefs;
  prefs.begin("ranger-key", false);
  _boot = prefs.getUInt("boot", 0) + 1;              // a new number every boot keeps nonces unique
  prefs.putUInt("boot", _boot);
  uint8_t key[16];
  bool saved = prefs.getBytes("key", key, 16) == 16;
  prefs.end();
  mbedtls_ccm_init(&_ccm);
  return saved && useKey(key);
}

bool Crypto::setPassphrase(const char* text) {
  // Key = PBKDF2-SHA256(passphrase, "Ranger" + network, 4096 rounds)
  uint8_t salt[7] = {'R', 'a', 'n', 'g', 'e', 'r', _net}, key[16];
  mbedtls_md_context_t md;
  mbedtls_md_init(&md);
  mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  int err = mbedtls_pkcs5_pbkdf2_hmac(&md, (const uint8_t*)text, strlen(text), salt, 7, 4096, 16, key);
  mbedtls_md_free(&md);
  if (err || !useKey(key)) return false;
  Preferences prefs;
  prefs.begin("ranger-key", false);
  prefs.putBytes("key", key, 16);
  prefs.end();
  return true;
}

bool Crypto::useKey(const uint8_t key[16]) {
  memcpy(_key, key, 16);
  _hasKey = mbedtls_ccm_setkey(&_ccm, MBEDTLS_CIPHER_ID_AES, key, 128) == 0;
  return _hasKey;
}

void Crypto::fingerprint(char out[9]) {
  uint8_t h[32];
  mbedtls_sha256_ret(_key, 16, h, 0);
  snprintf(out, 9, "%02X%02X%02X%02X", h[0], h[1], h[2], h[3]);
}

// Nonce (13 bytes) = epoch 4 | from 2 | seq 2 | network 1 | "RNGR". Never repeats for one key.
void Crypto::makeNonce(uint8_t n[13], uint32_t epoch, const uint8_t* aad) {
  memcpy(n, &epoch, 4);
  memcpy(n + 4, aad + 3, 2);                         // from
  memcpy(n + 6, aad + 7, 2);                         // seq
  n[8] = _net;
  memcpy(n + 9, "RNGR", 4);
}

bool Crypto::seal(const uint8_t* aad, const uint8_t* in, size_t len, uint8_t* out, size_t& outLen) {
  if (!_hasKey || len + 12 > 245) return false;
  uint16_t seq = aad[7] | (aad[8] << 8);
  if (seq < _lastSeq && _lastSeq - seq > 30000) _wrap++;   // my seq wrapped: new epoch
  _lastSeq = seq;
  uint32_t epoch = (_boot << 8) | _wrap;
  uint8_t nonce[13];
  makeNonce(nonce, epoch, aad);
  memcpy(out, &epoch, 4);
  if (mbedtls_ccm_encrypt_and_tag(&_ccm, len, nonce, 13, aad, 9, in, out + 4, out + 4 + len, 8)) return false;
  outLen = len + 12;
  return true;
}

bool Crypto::open(const uint8_t* aad, const uint8_t* in, size_t len, uint8_t* out, size_t& outLen) {
  if (!_hasKey || len < 12) return false;
  uint32_t epoch;
  memcpy(&epoch, in, 4);
  uint8_t nonce[13];
  makeNonce(nonce, epoch, aad);
  size_t n = len - 12;
  if (mbedtls_ccm_auth_decrypt(&_ccm, n, nonce, 13, aad, 9, in + 4, out, in + 4 + n, 8)) return false;
  uint16_t from = aad[3] | (aad[4] << 8), seq = aad[7] | (aad[8] << 8);
  if (!isNew(from, ((uint64_t)epoch << 16) | seq)) return false;   // replayed
  outLen = n;
  return true;
}

// Replay check: remembers the newest counter per sender plus the 64 before it
bool Crypto::isNew(uint16_t from, uint64_t counter) {
  for (auto& s : _seen) {
    if (s.from != from) continue;
    if (counter > s.top) {
      uint64_t shift = counter - s.top;
      s.mask = shift >= 64 ? 1 : (s.mask << shift) | 1;
      s.top = counter;
      return true;
    }
    uint64_t age = s.top - counter;
    if (age >= 64 || (s.mask & (1ULL << age))) return false;   // too old, or seen before
    s.mask |= 1ULL << age;
    return true;
  }
  _seen[_seenNext] = {from, counter, 1};                        // first packet from this sender
  _seenNext = (_seenNext + 1) % 8;
  return true;
}
