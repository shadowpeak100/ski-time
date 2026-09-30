#include "sprites.h"
#include "fb.h"
#include "scene.h"

#define QUARTER_TURN (TRIG_MAX_ANGLE / 4)
#define SPRAY_STEP 2921   // 0.28 rad in trig units

static const uint8_t SPRAY_HASH[6] = { 210, 27, 205, 161, 199, 144 };   // prototype hash(j) * 256

static GColor jacket(uint8_t i) {
  switch (i & 7) {
    case 0: return GColorRed;
    case 1: return GColorChromeYellow;
    case 2: return GColorBlueMoon;
    case 3: return GColorVividViolet;
    case 4: return GColorOrange;
    case 5: return GColorVividCerulean;
    case 6: return GColorFolly;
    default: return GColorJaegerGreen;
  }
}

// Canvas coordinates (pixel edges on integers): a stroke or disc centred at c lands in pixel floor(c).
static GPoint q4_point(const SkPt *p) { return GPoint(p->x >> 4, p->y >> 4); }
static GPoint px_point(float x, float y) { return GPoint(fb_floor(x), fb_floor(y)); }

static void polyline(GContext *ctx, const Skier *s, uint16_t from, uint16_t to) {
  GPoint a = q4_point(&s->pts[from]);
  for (uint16_t i = from + 2;; i += 2) {
    if (i > to) i = to;
    GPoint b = q4_point(&s->pts[i]);
    graphics_draw_line(ctx, a, b);
    a = b;
    if (i == to) break;
  }
}

void sprites_track(GContext *ctx, const Skier *s, uint16_t from, uint16_t to, TrackStyle style) {
  if (to <= from || to >= s->n) return;
  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  polyline(ctx, s, from, to);
  if (style == TRACK_TWIN) {
    graphics_context_set_antialiased(ctx, false);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    polyline(ctx, s, from, to);
  }
  graphics_context_set_stroke_width(ctx, 1);
}

static void line(GContext *ctx, float x0, float y0, float x1, float y1) {
  graphics_draw_line(ctx, px_point(x0, y0), px_point(x1, y1));
}

void sprites_skier(GContext *ctx, int16_t x_q4, int16_t y_q4, int32_t h, uint8_t color, float k) {
  float x = x_q4 / 16.0f, y = y_q4 / 16.0f;
  float s = k > 0.85f ? k : 0.85f, len = 3.6f * k > 3.2f ? 3.6f * k : 3.2f;
  float cx = (float)cos_lookup(h) / TRIG_MAX_RATIO, sy = (float)sin_lookup(h) / TRIG_MAX_RATIO;
  float px = -sy, py = cx;
  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  for (int o = -1; o <= 1; o += 2)
    line(ctx, x + px * o * 2.4f * s - cx * 0.5f, y + py * o * 2.4f * s - sy * 0.5f - 1.6f * s,
              x + px * o * 2.8f * s - cx * 3.2f * s, y + py * o * 2.8f * s - sy * 3.2f * s);
  graphics_context_set_stroke_color(ctx, GColorOxfordBlue);
  for (int o = -1; o <= 1; o += 2) {
    float ox = px * o * 1.05f * s, oy = py * o * 1.05f * s;
    line(ctx, x - cx * len + ox, y - sy * len + oy, x + cx * len * 1.15f + ox, y + sy * len * 1.15f + oy);
  }
  graphics_context_set_antialiased(ctx, false);
  float bx = x + cx * 0.5f, by = y + sy * 0.5f - 1.7f * s;
  graphics_context_set_fill_color(ctx, jacket(color));
  graphics_fill_circle(ctx, px_point(bx, by), 2);
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_circle(ctx, px_point(bx + cx * 0.5f, by - 1.9f * s), 1);
}

void sprites_spray(GContext *ctx, int16_t x_q4, int16_t y_q4, int32_t h, uint8_t spray, float k) {
  float x = x_q4 / 16.0f, y = y_q4 / 16.0f, u = spray / 255.0f;
  graphics_context_set_fill_color(ctx, u < 0.5f ? SCENE_ROLE(snow_shadow) : SCENE_ROLE(snow_shade));
  int32_t side = h + QUARTER_TURN;
  for (int j = 0; j < 6; j++) {
    int32_t a = side + (2 * j - 5) * SPRAY_STEP / 2;
    float r = (2 + u * 5) * k * (0.7f + SPRAY_HASH[j] / 256.0f * 0.5f);
    float dx = (float)cos_lookup(a) / TRIG_MAX_RATIO * r, dy = (float)sin_lookup(a) / TRIG_MAX_RATIO * r;
    graphics_fill_rect(ctx, GRect(fb_round(x + dx), fb_round(y + dy), 1, 1), 0, GCornerNone);
  }
}
