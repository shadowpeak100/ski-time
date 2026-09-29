#include "fb.h"
#include "geom.h"
#include "avalanche.h"

void fb_fill_poly(GContext *ctx, const GPoint *pts, uint16_t n, GColor c) {
  GPath p = { .num_points = n, .points = (GPoint *)pts, .rotation = 0, .offset = GPointZero };
  graphics_context_set_fill_color(ctx, c);
  gpath_draw_filled(ctx, &p);
}

void fb_fill_tri(GContext *ctx, float x0, float y0, float x1, float y1, float x2, float y2, GColor c) {
  GPoint pts[3] = {
    { fb_floor(x0), fb_floor(y0) }, { fb_floor(x1), fb_floor(y1) }, { fb_floor(x2), fb_floor(y2) } };
  fb_fill_poly(ctx, pts, 3, c);
}

void fb_fill_ellipse(GContext *ctx, float cx, float cy, float rx, float ry, GColor c) {
  graphics_context_set_fill_color(ctx, c);
  for (int y = fb_floor(cy - ry); y <= fb_floor(cy + ry); y++) {
    float dy = (y + 0.5f - cy) / ry;
    if (dy * dy > 1) continue;
    float hw = rx * geom_sqrt(1 - dy * dy);
    int16_t x0 = fb_round(cx - hw), x1 = fb_round(cx + hw);
    if (x1 > x0) graphics_fill_rect(ctx, GRect(x0, y, x1 - x0, 1), 0, GCornerNone);
  }
}

void fb_dither_circle(GBitmap *fb, int cx, int cy, int r, GColor c, int p256, const int16_t *top, const int16_t *bot) {
  GRect b = gbitmap_get_bounds(fb);
  int y0 = cy - r < 0 ? 0 : cy - r, y1 = cy + r >= b.size.h ? b.size.h - 1 : cy + r;
  int x0 = cx - r < 0 ? 0 : cx - r, x1 = cx + r >= b.size.w ? b.size.w - 1 : cx + r;
  for (int y = y0; y <= y1; y++) {
    uint8_t *row = fb_row(fb, y);
    int dy = y - cy;
    for (int x = x0; x <= x1; x++) {
      int dx = x - cx;
      if (dx * dx + dy * dy > r * r) continue;
      if (y < top[x] || y >= bot[x]) continue;
      if (aval_bayer(x, y) * 16 + 8 < p256) row[x] = c.argb;
    }
  }
}
