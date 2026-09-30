#ifndef SAVER_H
#define SAVER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { ANIM_FULL = 0, ANIM_BALANCED = 1, ANIM_SAVER = 2 } AnimMode;

bool saver_active(bool enabled, uint16_t start, uint16_t end, int hour, int minute);
uint8_t effective_anim_mode(uint8_t mode, bool schedule_on, uint16_t start, uint16_t end, int hour, int minute);

#endif
