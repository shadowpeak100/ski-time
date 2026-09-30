#include "saver.h"

bool saver_active(bool enabled, uint16_t start, uint16_t end, int hour, int minute) {
  if (!enabled || start == end) return false;
  int now = hour * 60 + minute;
  return start < end ? (now >= start && now < end) : (now >= start || now < end);
}

uint8_t effective_anim_mode(uint8_t mode, bool schedule_on, uint16_t start, uint16_t end, int hour, int minute) {
  if (mode > ANIM_SAVER) mode = ANIM_BALANCED;
  return (mode == ANIM_SAVER || saver_active(schedule_on, start, end, hour, minute)) ? ANIM_SAVER : mode;
}
