#ifndef MONO_H
#define MONO_H

#include <stdint.h>
#include <stdbool.h>

// time_ms()'s seconds and milliseconds can roll over at different moments (the emulator's are
// ~half a second apart), so seconds*1000+ms jumps a second forward and back. This clock only
// adds how far the millisecond counter moved; whole seconds count only across gaps of 2 s or more.
typedef struct { uint32_t ms, last_s; uint16_t last_sub; bool init; } MonoClock;

uint32_t mono_step(MonoClock *c, uint32_t s, uint16_t sub_ms);

#endif
