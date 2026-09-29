#ifndef AVAL_DRAW_H
#define AVAL_DRAW_H

#include <pebble.h>
#include "layout.h"

void aval_draw_init(void);                                       // builds the streak table
void aval_draw(GContext *ctx, const Layout *L, uint16_t t_ms);   // crack, pixel pass, billows, debris, spray, then restore outside slope

#endif
