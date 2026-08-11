/*
  FrameQueue -- fixed-size encoded-audio queue between a capture task and a
  transmit task, with drop-oldest overflow.

  Why drop-oldest rather than blocking the producer: blocking would just move
  the overflow back into the I2S DMA ring, where it is invisible and unbounded.
  Discarding the stale frame instead keeps latency capped at the queue depth
  and guarantees that what goes out is the most recent speech -- which is what
  a walkie-talkie should do. Drops are counted so overload is visible.
*/

#ifndef RANGER_FRAMEQUEUE_H
#define RANGER_FRAMEQUEUE_H

#include <Arduino.h>
#include <string.h>

template <int FRAME_BYTES, int DEPTH>
class FrameQueue {
public:
  struct Frame {
    uint8_t bytes[FRAME_BYTES];
  };

  bool begin() {
    q = xQueueCreate(DEPTH, sizeof(Frame));
    return q != NULL;
  }

  // Producer side. Never blocks. On a full queue the oldest frame is dropped
  // to make room, so the newest audio always wins.
  void push(const uint8_t *data) {
    if (q == NULL) return;

    Frame f;
    memcpy(f.bytes, data, FRAME_BYTES);

    if (xQueueSend(q, &f, 0) == pdTRUE) return;

    Frame discard;
    if (xQueueReceive(q, &discard, 0) == pdTRUE) dropped++;
    // If this still fails the consumer drained concurrently and the next
    // frame will find room, so let this one go rather than spinning.
    if (xQueueSend(q, &f, 0) != pdTRUE) dropped++;
  }

  // Consumer side. Returns false on timeout.
  bool pop(uint8_t *dst, uint32_t timeoutMs) {
    if (q == NULL) return false;
    Frame f;
    if (xQueueReceive(q, &f, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) return false;
    memcpy(dst, f.bytes, FRAME_BYTES);
    return true;
  }

  // Discard everything queued. Call on transmission boundaries so a new
  // transmission never opens with audio captured before it started.
  void flush() {
    if (q == NULL) return;
    Frame discard;
    while (xQueueReceive(q, &discard, 0) == pdTRUE) { }
  }

  uint32_t dropCount() const { return dropped; }
  void resetCounters() { dropped = 0; }

private:
  QueueHandle_t q = NULL;
  volatile uint32_t dropped = 0;
};

#endif  // RANGER_FRAMEQUEUE_H
