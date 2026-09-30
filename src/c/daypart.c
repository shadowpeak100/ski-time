#include "daypart.h"

static bool near_event(int m, int ev) {
  int d = ((m - ev) % 1440 + 1440) % 1440;
  return d < 60 || d >= 1380;
}

Daypart daypart_at(int m, uint16_t sr, uint16_t ss) {
  if (near_event(m, sr)) return DAY_SUNRISE;
  if (near_event(m, ss)) return DAY_SUNSET;
  int since_rise_window = ((m - (sr + 60)) % 1440 + 1440) % 1440, span = ((ss - sr - 120) % 1440 + 1440) % 1440;
  return since_rise_window < span ? DAY_MIDDAY : DAY_NIGHT;   // wrap-aware: sunset may precede sunrise
}

Daypart daypart_for(uint8_t mode, int m, uint16_t pr, uint16_t ps, uint16_t fr, uint16_t fs) {
  if (mode == DPM_ALWAYS_DAY) return DAY_MIDDAY;
  return mode == DPM_FIXED ? daypart_at(m, fr, fs) : daypart_at(m, pr, ps);
}

static long jdn(int y, int mo, int d) {
  int a = (14 - mo) / 12, yy = y + 4800 - a, mm = mo + 12 * a - 3;
  return d + (153L * mm + 2) / 5 + 365L * yy + yy / 4 - yy / 100 + yy / 400 - 32045L;
}

float moon_phase(int y, int mo, int d, int h, int mi) {
  // reference new moon 2000-01-06 18:14 UTC; synodic month 29.530588853 days
  float days = (float)(jdn(y, mo, d) - jdn(2000, 1, 6)) + ((h * 60 + mi) - 1094) / 1440.0f;
  float p = days / 29.530588853f;
  p -= (float)(long)p;
  if (p < 0) p += 1;
  return p;
}
