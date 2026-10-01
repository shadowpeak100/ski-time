#include "wx_draw.h"

#ifdef PBL_PLATFORM_EMERY
#include "bolt.h"
#include "fb.h"
#include "avalanche.h"

#define P256(f) ((int)((f) * 256 + 0.5f))

typedef struct { GBitmap *fb; int16_t W, H; } Canvas;

static void put(const Canvas *c, int x, int y, Col8 col) {
  if (x < 0 || y < 0 || x >= c->W || y >= c->H) return;
  fb_row(c->fb, y)[x] = col;
}

static bool dith(int x, int y, int p256) { return (int)aval_bayer(x, y) * 16 + 8 < p256; }

// Canvas-style wide stroke with round joins: pixels whose centre lies within w/2 of a segment.
static void dither_polyline(const Canvas *c, const Vec2 *p, int n, float w, Col8 col, int p256) {
  const float r = w / 2, r2 = r * r;
  for (int i = 1; i < n; i++) {
    const Vec2 a = p[i - 1], b = p[i];
    const float dx = b.x - a.x, dy = b.y - a.y, len2 = dx * dx + dy * dy;
    const int x0 = fb_floor((a.x < b.x ? a.x : b.x) - r), x1 = fb_floor((a.x > b.x ? a.x : b.x) + r);
    const int y0 = fb_floor((a.y < b.y ? a.y : b.y) - r), y1 = fb_floor((a.y > b.y ? a.y : b.y) + r);
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        const float px = x + 0.5f - a.x, py = y + 0.5f - a.y;
        float u = len2 > 0 ? (px * dx + py * dy) / len2 : 0;
        u = u < 0 ? 0 : u > 1 ? 1 : u;
        const float ex = px - u * dx, ey = py - u * dy;
        if (ex * ex + ey * ey < r2 && dith(x, y, p256)) put(c, x, y, col);
      }
  }
}

// 1 px polyline (Bresenham between the pixels the canvas points fall in).
static void core_polyline(const Canvas *c, const Vec2 *p, int n, Col8 col) {
  for (int i = 1; i < n; i++) {
    int x = fb_floor(p[i - 1].x), y = fb_floor(p[i - 1].y);
    const int x1 = fb_floor(p[i].x), y1 = fb_floor(p[i].y);
    const int dx = x1 > x ? x1 - x : x - x1, sx = x < x1 ? 1 : -1;
    const int dy = y1 > y ? y - y1 : y1 - y, sy = y < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
      put(c, x, y, col);
      if (x == x1 && y == y1) break;
      const int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x += sx; }
      if (e2 <= dx) { err += dx; y += sy; }
    }
  }
}

static void draw_flash(const Canvas *c, const Layout *L) {
  const int rows = fb_floor(layout_ridge_y(L, L->W / 2.0f) + 0.5f);
  for (int y = 0; y < rows && y < c->H; y++)
    for (int x = 0; x < c->W; x++)
      if (dith(x, y, P256(0.35f))) put(c, x, y, C8(0xFFFFFF));
}

static void draw_bolt(const Canvas *c, const Bolt *b) {
  dither_polyline(c, b->main, b->n_main, 3, C8(0xAAAAFF), P256(0.55f));
  for (int j = 0; j < BOLT_BRANCHES; j++) dither_polyline(c, b->br[j], b->n_br[j], 2, C8(0x5555AA), P256(0.5f));
  for (int j = 0; j < BOLT_BRANCHES; j++) core_polyline(c, b->br[j], (b->n_br[j] * 4 + 4) / 5, C8(0xAAFFFF));   // ceil(80%)
  core_polyline(c, b->main, b->n_main, C8(0xFFFFFF));
}

static void draw_flake(const Canvas *c, int x, int y, bool big) {
  if (big) {
    put(c, x + 1, y + 2, C8(0x5555AA));
    put(c, x - 1, y, C8(0xAAAAFF)); put(c, x + 1, y, C8(0xAAAAFF));
    put(c, x, y - 1, C8(0xAAAAFF)); put(c, x, y + 1, C8(0xAAAAFF));
    put(c, x, y, C8(0xFFFFFF));
  } else {
    put(c, x + 1, y + 1, C8(0x5555AA));
    put(c, x, y, C8(0xAAAAFF));
  }
}

void wx_draw(GContext *ctx, const Layout *L, WxKind w, Daypart d, bool still, int8_t wind, uint32_t ms) {
  static Bolt s_bolt;
  static bool s_bolt_built;
  const WxDef *df = wx_def(w);
  const float t = (ms % 600000u) / 1000.0f;   // wrapped so the float clock keeps its precision
  bool flash = false;
  const bool bolt_on = df->lightning && bolt_visible(ms, still, &flash);
  if (bolt_on && !s_bolt_built) { bolt_build(&s_bolt, L); s_bolt_built = true; }

  if (bolt_on || df->precip == PRECIP_SNOW) {
    GBitmap *fb = graphics_capture_frame_buffer(ctx);
    if (!fb) return;
    const Canvas c = { fb, L->W, L->H };
    if (flash) draw_flash(&c, L);
    if (bolt_on) draw_bolt(&c, &s_bolt);
    if (df->precip == PRECIP_SNOW) {
      const int n = wx_drop_count(df, still);
      for (int i = 0; i < n; i++) {
        int16_t x, y; bool big;
        if (wx_flake(L, df, i, t, still, wind, &x, &y, &big)) draw_flake(&c, x, y, big);
      }
    }
    graphics_release_frame_buffer(ctx, fb);
  }

  if (df->precip == PRECIP_RAIN) {
    graphics_context_set_antialiased(ctx, false);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_stroke_color(ctx, (GColor){ .argb = wx_rain_colour(d) });
    const int n = wx_drop_count(df, still);
    for (int i = 0; i < n; i++) {
      Vec2 a, b;
      if (wx_rain_drop(L, df, i, t, still, wind, &a, &b))
        graphics_draw_line(ctx, GPoint(fb_floor(a.x), fb_floor(a.y)), GPoint(fb_floor(b.x), fb_floor(b.y)));
    }
  }
}
#endif
