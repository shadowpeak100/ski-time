#ifndef COMPS_H
#define COMPS_H

#include <pebble.h>
#include "layout.h"

void comps_set_temp(int temp_f, bool valid);
void comps_set_saver(bool saver);
void comps_draw(GContext *ctx, const Layout *L);
bool comps_needs_seconds(void);

#endif
