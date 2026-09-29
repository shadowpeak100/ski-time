#ifndef TRIG_SHIM_H
#define TRIG_SHIM_H
#include <stdint.h>
#define TRIG_MAX_ANGLE 0x10000
#define TRIG_MAX_RATIO 0xffff
int32_t sin_lookup(int32_t angle);
int32_t cos_lookup(int32_t angle);
int32_t atan2_lookup(int16_t y, int16_t x);
#endif
