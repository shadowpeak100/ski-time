#include "flood.h"
#include "geom.h"

#define LEN 215.0f
#define POLE 16.0f
#define COSH 0.9135455f   // cos 24 degrees

static const float FLOODS[FLOOD_COUNT][4] = {   // base x, base y, aim x, aim y (fractions of W, H)
  { 0.95f, 0.30f, 0.40f, 0.36f }, { 0.97f, 0.50f, 0.38f, 0.53f }, { 0.965f, 0.64f, 0.40f, 0.70f },
  { 0.07f, 0.45f, 0.62f, 0.36f }, { 0.05f, 0.76f, 0.62f, 0.62f },
};

void flood_heads(int W, int H, float k, FloodHead out[FLOOD_COUNT]) {
  for (int i = 0; i < FLOOD_COUNT; i++) {
    FloodHead *f = &out[i];
    f->bx = FLOODS[i][0] * W; f->by = FLOODS[i][1] * H;
    f->hx = f->bx; f->hy = f->by - POLE * k;
    Vec2 d = geom_norm(FLOODS[i][2] * W - f->hx, FLOODS[i][3] * H - f->hy);
    f->dx = d.x; f->dy = d.y;
  }
}

static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static float smooth(float t) { return t * t * (3 - 2 * t); }

float flood_at(const FloodHead *h, int n, float k, float x, float y) {
  float best = 0, len = LEN * k;
  for (int i = 0; i < n; i++) {
    float vx = x - h[i].hx, vy = y - h[i].hy, d = geom_sqrt(vx * vx + vy * vy);
    if (d > len || d < 1) continue;
    float c = (vx * h[i].dx + vy * h[i].dy) / d;
    if (c <= COSH) continue;
    float edge = smooth(clamp01((c - COSH) / (1 - COSH) * 2.2f));
    float f = 1 - d / len, f75 = geom_sqrt(f) * geom_sqrt(geom_sqrt(f));
    float fall = 0.5f * (f + f75);                       // f^0.85 without libm
    float start = smooth(clamp01((d - 4 * k) / (12 * k)));
    float v = edge * fall * start;
    if (v > best) best = v;
  }
  return best;
}
