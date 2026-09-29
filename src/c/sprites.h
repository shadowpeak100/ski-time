#ifndef SPRITES_H
#define SPRITES_H

#include <pebble.h>
#include "skier.h"

typedef enum { TRACK_TWIN = 0, TRACK_SOLID = 1 } TrackStyle;

void sprites_track(GContext *ctx, const Skier *s, uint16_t from, uint16_t to, TrackStyle style);
void sprites_skier(GContext *ctx, int16_t x_q4, int16_t y_q4, int32_t h, uint8_t color, float k);
void sprites_spray(GContext *ctx, int16_t x_q4, int16_t y_q4, int32_t h, uint8_t spray, float k);

#endif
