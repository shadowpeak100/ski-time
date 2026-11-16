#ifndef ANIM_LEN_H
#define ANIM_LEN_H

#include <stdint.h>

#define ANIM_LEN_NATURAL 255   // Settings.anim_len: 0 instant, 1..200 tenths of a second, 255 natural

uint32_t anim_len_total_ms(uint16_t natural_ms, uint8_t len);
// Timeline ms for real elapsed ms: scaled so the change fills the chosen length, never past natural_ms.
uint16_t anim_len_t(uint32_t elapsed_ms, uint16_t natural_ms, uint8_t len);

#endif
