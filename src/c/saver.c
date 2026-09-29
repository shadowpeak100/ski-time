#include "saver.h"

bool saver_active(bool enabled, uint16_t start, uint16_t end, int hour, int minute) {
  if (!enabled || start == end) return false;
  int now = hour * 60 + minute;
  return start < end ? (now >= start && now < end) : (now >= start || now < end);
}
