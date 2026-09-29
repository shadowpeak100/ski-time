// Floating-point stand-in for the firmware trig tables. Host only.
#include <math.h>
#include "trig_shim.h"

#define TWO_PI_D 6.28318530717958647692

int32_t sin_lookup(int32_t a) { return (int32_t)lround(sin((double)a * TWO_PI_D / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO); }
int32_t cos_lookup(int32_t a) { return (int32_t)lround(cos((double)a * TWO_PI_D / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO); }

// Matches PebbleOS: standard atan2(y, x) in [0, 2*pi), angle 0 on the +x axis.
int32_t atan2_lookup(int16_t y, int16_t x) {
  double a = atan2((double)y, (double)x);
  if (a < 0) a += TWO_PI_D;
  return (int32_t)(a * TRIG_MAX_ANGLE / TWO_PI_D) & (TRIG_MAX_ANGLE - 1);
}
