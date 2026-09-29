#include "aval_draw.h"
#include "avalanche.h"
#include "scene.h"
#include "fb.h"
#include "geom.h"

#define CRACK_END_MS (AV_FLOW_START_MS + 150)

static uint8_t s_streak[AV_MAX_S];
static AvalFrame s_A;

// Prototype hash(m*5.3), hash(m*9.1) and hash(m*3.7), scaled to 0..255.
static const uint8_t H53[28] = { 210, 127, 250, 243, 41, 26, 90, 240, 95, 58, 82, 171, 117, 32, 194, 222, 191, 118, 77, 51, 85, 60, 32, 145, 190, 95, 165, 162 };
static const uint8_t H91[28] = { 210, 131, 174, 119, 18, 242, 204, 43, 132, 227, 126, 17, 242, 254, 166, 233, 26, 64, 10, 106, 1, 12, 132, 129, 128, 73, 231, 114 };
static const uint8_t H37[70] = { 210, 253, 6, 177, 232, 240, 229, 18, 109, 179, 13, 33, 161, 185, 129, 200, 57, 122, 144, 76,
  36, 123, 240, 195, 115, 223, 117, 88, 126, 154, 22, 199, 145, 250, 49, 115, 59, 93, 174, 34, 122, 221, 139, 212, 29, 54,
  123, 209, 124, 122, 200, 15, 218, 220, 160, 198, 205, 20, 182, 144, 163, 83, 139, 153, 253, 178, 124, 140, 13, 22 };

// Per-frame stand-in for hash(m*7.1 + fr): only needs to look random and change every frame step.
static float hash2(uint32_t m, uint32_t fr) {
  uint32_t h = m * 0x9E3779B1u ^ (fr + 0x7F4A7C15u) * 0x85EBCA77u;
  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
  return (h & 0xFF) / 256.0f;
}

static Vec2 pt(float u, float s) { return (Vec2){ FALL.x * u + ACROSS.x * s, FALL.y * u + ACROSS.y * s }; }
static bool in_view(const Layout *L, Vec2 q) { return q.x > -14 && q.x < L->W + 14 && q.y > -14 && q.y < L->H + 14; }
static float fabsf_(float v) { return v < 0 ? -v : v; }

static bool on_slope(const Layout *L, GPoint p) {
  if (p.x < 0 || p.x >= L->W) return false;
  return p.y >= scene_ridge_rows()[p.x] && p.y < scene_berm_rows()[p.x];
}

static void disc(GContext *ctx, float x, float y, float r, GColor c) {
  int16_t ri = fb_round(r);
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_circle(ctx, GPoint(fb_floor(x), fb_floor(y)), ri < 1 ? 1 : ri);
}

static void dot(GContext *ctx, int x, int y, int w, int h, GColor c) {
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone);
}

void aval_draw_init(void) { aval_streaks(s_streak); }

static void draw_crack(GContext *ctx, const Layout *L, uint16_t t) {
  float k = L->k, grow = (t - AV_CRACK_MS) / 150.0f, step = 2.5f * k;
  if (grow > 1) grow = 1;
  float half = 70 * k * grow;
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorLiberty);
  GPoint prev = GPointZero;
  bool have = false;
  for (float s = -half; s <= half; s += step) {
    float j = ((fb_round(s / step) & 1) ? 1 : -1) * 1.1f * k;
    Vec2 q = pt(L->u0 + j, L->s0 + s);
    GPoint p = GPoint(fb_floor(q.x), fb_floor(q.y));
    if (have && on_slope(L, prev) && on_slope(L, p)) graphics_draw_line(ctx, prev, p);
    prev = p; have = true;
  }
}

static void pixel_pass(GContext *ctx, const Layout *L, uint16_t t) {
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  const int16_t *top = scene_ridge_rows(), *bot = scene_berm_rows();
  int W = L->W;
  uint8_t src[W];
  for (int y = 0; y < L->comp_top; y++) {
    uint8_t *row = fb_row(fb, y);
    scene_bg_row(y, src, W);
    for (int x = 0; x < W; x++) {
      if (y < top[x] || y >= bot[x]) continue;
      switch (aval_pixel(&s_A, s_streak, L->k, x, y, t)) {
        case AV_SHADOW:  row[x] = GColorBabyBlueEyes.argb; break;
        case AV_RESTORE: row[x] = src[x]; break;
        case AV_WHITE:   row[x] = GColorWhite.argb; break;
        case AV_CELESTE: row[x] = GColorCeleste.argb; break;
        case AV_BLUE:    row[x] = GColorBabyBlueEyes.argb; break;
        default: break;
      }
    }
  }
  graphics_release_frame_buffer(ctx, fb);
}

// Billows are recomputed for each paint layer instead of stored: basalt has no RAM to spare.
enum { HALO, SHADE, MID, TOP };

static void back_billows(GContext *ctx, const Layout *L, float tau, bool top) {
  float k = L->k, grow = s_A.grow;
  for (float s = L->s_min; s <= L->s_max; s += 5 * k) {
    float r = k * (2.5f + 4 * aval_noise(s * 0.17f + tau * 1.4f)) * grow;
    if (r < 0.6f) continue;
    Vec2 q = pt(aval_front(&s_A, s) - (11 + 5 * aval_noise(s * 0.09f)) * k * grow, s);
    if (!in_view(L, q)) continue;
    if (top) disc(ctx, q.x, q.y, r * 0.82f, GColorWhite);
    else disc(ctx, q.x + 0.8f * k, q.y + 1.1f * k, r, GColorCeleste);
  }
}

// Leading billows: they churn forward and back as they roll. HALO needs the captured framebuffer.
static void lead_billows(GContext *ctx, GBitmap *fb, const Layout *L, float tau, int layer) {
  float k = L->k, grow = s_A.grow;
  for (float s = L->s_min - 10; s <= L->s_max + 10; s += 3.2f * k) {
    float r = k * (3.5f + 5 * aval_noise(s * 0.11f + 3.1f) + 1.8f * aval_noise(s * 0.23f + tau * 2.6f)) * grow;
    if (r < 0.6f) continue;
    Vec2 q = pt(aval_front(&s_A, s) - r * 0.7f + geom_sin(tau * 6 + s * 0.3f) * 1.2f * k, s);
    if (!in_view(L, q)) continue;
    switch (layer) {
      case HALO:
        fb_dither_circle(fb, fb_floor(q.x), fb_floor(q.y), fb_round(r + 2 * k), GColorCeleste, 77,
                         scene_ridge_rows(), scene_berm_rows());
        break;
      case SHADE: disc(ctx, q.x + 0.8f * k, q.y + 1.2f * k, r, GColorBabyBlueEyes); break;
      case MID:   disc(ctx, q.x + 0.3f * k, q.y + 0.5f * k, r * 0.95f, GColorCeleste); break;
      default:    disc(ctx, q.x - 0.4f * k, q.y - 0.5f * k, r * 0.8f, GColorWhite); break;
    }
  }
}

static void vector_pass(GContext *ctx, const Layout *L, uint16_t t) {
  float k = L->k, tau = t / 1000.0f, grow = s_A.grow, span = L->s_max - L->s_min;
  back_billows(ctx, L, tau, false);   // second rank of billows, riding on the moving snow
  back_billows(ctx, L, tau, true);
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (fb) {
    lead_billows(ctx, fb, L, tau, HALO);
    graphics_release_frame_buffer(ctx, fb);
  }
  lead_billows(ctx, NULL, L, tau, SHADE);
  lead_billows(ctx, NULL, L, tau, MID);
  lead_billows(ctx, NULL, L, tau, TOP);
  // Tumbling debris thrown ahead of the front.
  int cs = fb_round(2 * k);
  if (cs < 1) cs = 1;
  for (int m = 0; m < 28; m++) {
    float s = L->s_min + H53[m] / 256.0f * span, lead = (2 + H91[m] / 256.0f * 10) * k * grow;
    Vec2 q = pt(aval_front(&s_A, s) + lead, s);
    if (!in_view(L, q)) continue;
    float hop = fabsf_(geom_sin(tau * 9 + m * 1.7f)) * 2.5f * k;
    int qx = fb_round(q.x), qy = fb_round(q.y), hy = fb_round(q.y - hop);
    dot(ctx, qx, qy + 1, cs, 1, GColorBabyBlueEyes);
    dot(ctx, qx, hy - cs + 1, cs, cs, GColorWhite);
    dot(ctx, qx + cs - 1, hy, 1, 1, GColorCeleste);
  }
  // Loose snow sparkling just ahead of the front.
  uint32_t fr = (uint32_t)(tau * 14);
  for (int m = 0; m < 70; m++) {
    float s = L->s_min + H37[m] / 256.0f * span;
    Vec2 q = pt(aval_front(&s_A, s) + hash2(m, fr) * 9 * k * grow, s);
    dot(ctx, fb_round(q.x), fb_round(q.y), 1, 1, m % 3 ? GColorBabyBlueEyes : GColorLightGray);
  }
}

void aval_draw(GContext *ctx, const Layout *L, uint16_t t) {
  if (!scene_ready()) return;
  aval_frame(&s_A, L, t);
  graphics_context_set_antialiased(ctx, false);
  bool crack = t >= AV_CRACK_MS && t < CRACK_END_MS;
  // Prototype order: the crack sits on top of the first moving snow, under the billows.
  if (s_A.active) pixel_pass(ctx, L, t);
  if (crack) draw_crack(ctx, L, t);
  if (s_A.active) vector_pass(ctx, L, t);
  if (crack || s_A.active) scene_restore_outside_slope(ctx, L);
}
