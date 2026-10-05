#include "mono.h"

uint32_t mono_step(MonoClock *c, uint32_t s, uint16_t sub_ms) {
  if (c->init) {
    uint32_t d = (sub_ms + 1000u - c->last_sub) % 1000u, ds = s - c->last_s;
    if (ds >= 2 && ds < 0x80000000u) d += (ds - 1) * 1000u;   // long gap: the sub-second part alone wraps
    c->ms += d;
  }
  c->init = true;
  c->last_s = s;
  c->last_sub = sub_ms;
  return c->ms;
}
