#include "anim_len.h"

uint32_t anim_len_total_ms(uint16_t natural_ms, uint8_t len) {
  if (len == ANIM_LEN_NATURAL) return natural_ms;
  return (uint32_t)(len > 200 ? 200 : len) * 100;
}

uint16_t anim_len_t(uint32_t elapsed_ms, uint16_t natural_ms, uint8_t len) {
  if (len == ANIM_LEN_NATURAL) return elapsed_ms < 65534 ? (uint16_t)elapsed_ms : 65534;
  uint32_t total = anim_len_total_ms(natural_ms, len);
  if (total == 0 || elapsed_ms >= total) return natural_ms;
  return (uint16_t)(elapsed_ms * natural_ms / total);
}
