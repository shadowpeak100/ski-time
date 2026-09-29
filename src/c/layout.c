#include "layout.h"

const Vec2 FALL = { -0.6f, 0.8f };
const Vec2 ACROSS = { 0.8f, 0.6f };

// Math.round for the positive values used here.
static int16_t rnd(float v) { return (int16_t)(v >= 0 ? v + 0.5f : v - 0.5f); }

float layout_ridge_y(const Layout *L, float x) {
  float f = x / L->W;
  float fc = f < 0 ? 0 : f > 1 ? 1 : f;
  return L->y_l + (L->y_r - L->y_l) * f - geom_sin(3.14159265f * fc) * L->bulge;
}

float layout_berm_y(const Layout *L, float x) {
  return L->comp_top - 1 + geom_sin(x * 0.07f) * 1.2f * L->k;
}

Vec2 layout_glyph_point(const Layout *L, uint8_t slot, float u, float v) {
  Pt16 b = L->boxes[slot];
  return (Vec2){ b.x + (u + (1 - v) * GLYPH_SLANT - GLYPH_SLANT / 2) * L->gw, b.y + v * L->gh };
}

void layout_init(Layout *L, int W, int H) {
  L->W = W; L->H = H;
  float k = L->k = W / 200.0f;
  L->comp_h = rnd(H * 0.14f);
  L->comp_top = H - L->comp_h;
  L->y_l = rnd(H * 0.37f); L->y_r = rnd(H * 0.035f); L->bulge = H * 0.035f;
  L->gw = rnd(W * 0.21f); L->gh = rnd(L->gw * 1.2f);
  int16_t gx = rnd(W * 0.035f), gy = rnd(H * 0.028f);
  int16_t row2 = rnd(L->comp_top - 6 * k - L->gh), row1 = row2 - gy - L->gh;
  int16_t stag = rnd(W * 0.07f), row_w = L->gw * 2 + gx;
  int16_t cw = rnd(W * 0.24f), chh = rnd(cw * 0.95f);
  L->chalet = (Rect16){ (int16_t)(W - cw - rnd(3 * k)), (int16_t)(L->comp_top - chh - 1), cw, chh };
  int16_t mx = L->chalet.x - rnd(8 * k) - row_w, hx = mx + stag;
  L->boxes[0] = (Pt16){ hx, row1 };
  L->boxes[1] = (Pt16){ (int16_t)(hx + L->gw + gx), row1 };
  L->boxes[2] = (Pt16){ mx, row2 };
  L->boxes[3] = (Pt16){ (int16_t)(mx + L->gw + gx), row2 };
  float cx = W - 26 * k, cy = layout_ridge_y(L, cx) + 9 * k;
  L->u0 = cx * FALL.x + cy * FALL.y;
  L->s0 = cx * ACROSS.x + cy * ACROSS.y;
  L->u1 = -6 * FALL.x + L->comp_top * FALL.y + 16 * k;
  const float xs[4] = { 0, (float)W, 0, (float)W }, ys[4] = { 0, 0, (float)H, (float)H };
  L->s_min = 1e9f; L->s_max = -1e9f;
  for (int i = 0; i < 4; i++) {
    float s = xs[i] * ACROSS.x + ys[i] * ACROSS.y;
    if (s < L->s_min) L->s_min = s;
    if (s > L->s_max) L->s_max = s;
  }
}
