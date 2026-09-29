#ifndef FB_H
#define FB_H

#include <pebble.h>

void fb_fill_poly(GContext *ctx, const GPoint *pts, uint16_t n, GColor c);
void fb_fill_tri(GContext *ctx, float x0, float y0, float x1, float y1, float x2, float y2, GColor c);
void fb_fill_ellipse(GContext *ctx, float cx, float cy, float rx, float ry, GColor c);
// Bayer-dithered disc at density p256/256, limited per column to rows [top[x], bot[x]).
void fb_dither_circle(GBitmap *fb, int cx, int cy, int r, GColor c, int p256, const int16_t *top, const int16_t *bot);

static inline uint8_t *fb_row(GBitmap *fb, int y) { return gbitmap_get_data_row_info(fb, (uint16_t)y).data; }

// Canvas-style float coordinate (pixel edges on integers) to the Pebble pixel it starts in.
static inline int16_t fb_floor(float v) { int i = (int)v; return (int16_t)(v < i ? i - 1 : i); }
static inline int16_t fb_round(float v) { return fb_floor(v + 0.5f); }

#endif
