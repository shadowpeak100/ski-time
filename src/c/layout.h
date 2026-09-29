#ifndef LAYOUT_H
#define LAYOUT_H

#include <stdint.h>
#include "geom.h"

typedef struct { int16_t x, y; } Pt16;
typedef struct { int16_t x, y, w, h; } Rect16;
typedef struct {
  int16_t W, H; float k;
  int16_t comp_h, comp_top;
  float exit_ceiling;   // lowest y a ski-out path may reach
  float y_l, y_r, bulge;
  int16_t gw, gh;
  Pt16 boxes[4];
  Rect16 chalet;
  float u0, s0, u1, s_min, s_max;
} Layout;

extern const Vec2 FALL;    // (-0.6, 0.8) fall line
extern const Vec2 ACROSS;  // (0.8, 0.6) across the slope

void  layout_init(Layout *L, int W, int H);
float layout_ridge_y(const Layout *L, float x);
float layout_berm_y(const Layout *L, float x);

#define GLYPH_SLANT 0.16f

Vec2  layout_glyph_point(const Layout *L, uint8_t slot, float u, float v);

#endif
