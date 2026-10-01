#include <pebble.h>
#include "scene.h"
#include "fb.h"
#include "geom.h"
#include "avalanche.h"
#include "flood.h"
#include "decor.h"

#define MAX_TREES 40

// Background packed at 4 bits per pixel (low nibble = even x) with a palette of up to 16 GColor8
// bytes: the scene uses about 13 colours, and on basalt the 8-bit copy would not leave room for the skiers.
static uint8_t *s_bg, s_pal[16], s_npal;
static int16_t s_stride;
static int16_t *s_ridge_rows, *s_berm_rows;
static bool s_bg_valid;
static float s_chim_x, s_chim_y;   // smoke origin, kept in float so the puffs drift sub-pixel as in the prototype
static const Theme *s_theme;
static float s_moon;
#ifdef PBL_PLATFORM_EMERY
static WxKind s_wx;

void scene_set_weather(WxKind w) {
  if (w == s_wx) return;
  s_wx = w;
  s_bg_valid = false;
}

WxKind scene_weather(void) { return s_wx; }
#endif

void scene_set_theme(const Theme *t, float moon_phase) {
  if (t == s_theme && moon_phase == s_moon) return;
  s_theme = t;
  s_moon = moon_phase;
  s_bg_valid = false;
}

const Theme *scene_theme(void) { return s_theme ? s_theme : theme_for(DAY_MIDDAY); }

static GColor gcol(Col8 c) { return (GColor){ .argb = c }; }

static int16_t iceil(float v) { return -fb_floor(-v); }
static bool dith(int x, int y, float p) { return aval_bayer(x, y) * 16 + 8 < p * 256; }

// x^0.2 on [0,1] by Newton from x^0.25; pow(x,1.2) = x*root5(x), pow(x,1.4) = x*root5(x)^2.
static float root5(float x) {
  if (x <= 0) return 0;
  float y = geom_sqrt(geom_sqrt(x));
  for (int i = 0; i < 5; i++) { float y2 = y * y; y = (4 * y + x / (y2 * y2)) / 5; }
  return y;
}

static void poly(GContext *ctx, const float *xy, int n, GColor c) {
  GPoint pts[8];
  for (int i = 0; i < n; i++) pts[i] = GPoint(fb_floor(xy[2 * i]), fb_floor(xy[2 * i + 1]));
  fb_fill_poly(ctx, pts, (uint16_t)n, c);
}

static void rect(GContext *ctx, int x, int y, int w, int h, GColor c) {
  if (w <= 0 || h <= 0) return;
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone);
}

// Round-capped stroke of width w: pixels whose centre lies within w/2 of the segment.
static void capsule(GContext *ctx, float x0, float y0, float x1, float y1, float w, GColor c) {
  float r = w / 2, dx = x1 - x0, dy = y1 - y0, ll = dx * dx + dy * dy;
  int ya = fb_floor((y0 < y1 ? y0 : y1) - r), yb = fb_floor((y0 > y1 ? y0 : y1) + r);
  int xa = fb_floor((x0 < x1 ? x0 : x1) - r), xb = fb_floor((x0 > x1 ? x0 : x1) + r);
  graphics_context_set_fill_color(ctx, c);
  for (int y = ya; y <= yb; y++) {
    int lo = xb + 1, hi = xa - 1;
    for (int x = xa; x <= xb; x++) {
      float px = x + 0.5f - x0, py = y + 0.5f - y0;
      float t = ll > 0 ? (px * dx + py * dy) / ll : 0;
      t = t < 0 ? 0 : t > 1 ? 1 : t;
      float ex = px - t * dx, ey = py - t * dy;
      if (ex * ex + ey * ey <= r * r) { if (x < lo) lo = x; hi = x; }
    }
    if (hi >= lo) graphics_fill_rect(ctx, GRect(lo, y, hi - lo + 1, 1), 0, GCornerNone);
  }
}

// Canvas ellipse(cx, cy, rx, ry, rot): per row, solve the rotated conic for its x span.
static void fill_rot_ellipse(GContext *ctx, float cx, float cy, float rx, float ry, float rot, GColor c) {
  float cs = geom_cos(rot), sn = geom_sin(rot), irx = 1 / (rx * rx), iry = 1 / (ry * ry);
  float A = cs * cs * irx + sn * sn * iry, Bk = 2 * sn * cs * (irx - iry), Ck = sn * sn * irx + cs * cs * iry;
  float ext = geom_sqrt(rx * rx * sn * sn + ry * ry * cs * cs);
  graphics_context_set_fill_color(ctx, c);
  for (int y = fb_floor(cy - ext); y <= fb_floor(cy + ext); y++) {
    float Y = y + 0.5f - cy, B = Bk * Y, C = Ck * Y * Y - 1, disc = B * B - 4 * A * C;
    if (disc < 0) continue;
    float sq = geom_sqrt(disc);
    int16_t x0 = fb_round(cx + (-B - sq) / (2 * A)), x1 = fb_round(cx + (-B + sq) / (2 * A));
    if (x1 > x0) graphics_fill_rect(ctx, GRect(x0, y, x1 - x0, 1), 0, GCornerNone);
  }
}

static void pine(GContext *ctx, float x, float base, float h) {
  const Theme *tm = scene_theme();
  rect(ctx, fb_round(x - 0.5f), fb_round(base - h * 0.16f), 1, iceil(h * 0.16f), GColorBulgarianRose);
  for (int i = 0; i < 3; i++) {
    float top = base - h + i * h * 0.27f, bot = top + h * 0.46f, hw = h * (0.2f + i * 0.1f), th = bot - top;
    // Whole tier in shade first: gpath leaves the shared centre edge unfilled.
    fb_fill_tri(ctx, x - hw, bot, x, top, x + hw, bot, gcol(tm->pine_shade));
    fb_fill_tri(ctx, x - hw, bot, x, top, x, bot, gcol(tm->pine_lit));
    const float snow[8] = { x, top, x - hw * 0.55f, top + th * 0.55f, x - hw * 0.2f, top + th * 0.42f,
                            x + hw * 0.25f, top + th * 0.45f };
    poly(ctx, snow, 4, gcol(tm->pine_snow));
  }
}

static void peak(GContext *ctx, float px, float py, float hw, float base,
                 GColor lit, GColor shade, GColor cap_lit, GColor cap_shade) {
  float split = px + hw * 0.12f;
  // Shade under the whole peak so antialias-free edge pixels shared by both halves never show the sky.
  fb_fill_tri(ctx, px - hw, base, px, py, px + hw, base, shade);
  fb_fill_tri(ctx, px - hw, base, px, py, split, base, lit);
  float dep = (base - py) * 0.3f, lx = px - hw * 0.3f, rx = px + hw * 0.3f, ly = py + dep, ry = py + dep * 1.1f;
  const float cl[10] = { px, py, lx, ly, lx + (px - lx) * 0.35f, ly - dep * 0.25f, px - hw * 0.05f, ly + dep * 0.1f,
                         px + hw * 0.02f, ly - dep * 0.2f };
  const float cr[10] = { px, py, px + hw * 0.02f, ly - dep * 0.2f, px + hw * 0.12f, ry - dep * 0.05f,
                         px + hw * 0.2f, ry - dep * 0.35f, rx, ry };
  // gpath leaves the shared edge between the halves unfilled, so the shade underlay spans the whole cap.
  const float cap[16] = { cl[0], cl[1], cl[2], cl[3], cl[4], cl[5], cl[6], cl[7],
                          cr[2], cr[3], cr[4], cr[5], cr[6], cr[7], cr[8], cr[9] };
  poly(ctx, cap, 8, cap_shade);
  poly(ctx, cl, 5, cap_lit);
}

// Sky gradient dithered across the theme's (or weather's) stops, with the sun disc and its dithered halo.
static void sky_pass(GContext *ctx, const Layout *L) {
  const Theme *th = scene_theme();
  const Body *B = &th->body;
#ifdef PBL_PLATFORM_EMERY
  Col8 sky[4];
  int W = L->W, H = L->H, ns = wx_sky(theme_daypart(th), s_wx, sky) - 1;
#else
  const Col8 *sky = th->sky;
  int W = L->W, H = L->H, ns = th->nsky - 1;
#endif
  float k = L->k, sx = W * B->x, sy = H * B->y, sr = B->r * k, halo = B->halo * k, horizon = layout_ridge_y(L, 0) * 0.92f;
  bool sun = B->kind == BODY_SUN;
#ifdef PBL_PLATFORM_EMERY
  sun = sun && !wx_def(s_wx)->deck;   // cloud cover hides the sun
#endif
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  for (int y = 0; y < H; y++) {
    float f = y / horizon;
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    float t = f * root5(f) * ns, dy = y + 0.5f - sy;
    int i = (int)t < ns - 1 ? (int)t : ns - 1;
    uint8_t lo = sky[i], hi = sky[i + 1];
    t -= i;
    uint8_t *row = fb_row(fb, y);
    for (int x = 0; x < W; x++) {
      row[x] = dith(x, y, t) ? hi : lo;
      if (!sun) continue;
      float dx = x + 0.5f - sx, d2 = dx * dx + dy * dy;
      if (d2 < sr * sr) row[x] = B->core;
      else if (d2 < (sr + halo) * (sr + halo)) {
        float q = 1 - (geom_sqrt(d2) - sr) / halo, q5 = root5(q);
        if (dith(x, y, q * q5 * q5 * 0.85f)) row[x] = B->glow;
      }
    }
  }
  graphics_release_frame_buffer(ctx, fb);
}

// 48 stars above the ridge; every ninth gets a plus-shaped glint.
static void stars_pass(GContext *ctx, const Layout *L) {
  int W = L->W, H = L->H;
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  Mulberry r = { 99 };
  for (int n = 0; n < 48; n++) {
    int x = fb_floor(mulberry_next(&r) * W), y = fb_floor(mulberry_next(&r) * (H * 0.36f));
    float c = mulberry_next(&r);
    if (y > layout_ridge_y(L, x) - 3) continue;
    fb_row(fb, y)[x] = c < 0.7f ? C8(0xFFFFFF) : c < 0.9f ? C8(0xAAFFFF) : C8(0xFFFFAA);
    if (n % 9) continue;
    const int8_t d[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (int j = 0; j < 4; j++) {
      int gx = x + d[j][0], gy = y + d[j][1];
      if (gx >= 0 && gx < W && gy >= 0) fb_row(fb, gy)[gx] = C8(0x5555AA);
    }
  }
  graphics_release_frame_buffer(ctx, fb);
}

// Moon lit per pixel for the stored phase (0 new, 0.5 full; waxing lit on the right), with a faint dithered rim.
static void moon_pass(GContext *ctx, const Layout *L) {
  const Body *B = &scene_theme()->body;
  int W = L->W, H = L->H;
  float k = L->k, mx = W * B->x, my = H * B->y, mr = B->r * k, rim = 3 * k / mr, kx = geom_cos(6.2831853f * s_moon);
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  for (int y = fb_floor(my - mr - 3 * k); y <= my + mr + 3 * k; y++)
    for (int x = fb_floor(mx - mr - 3 * k); x <= mx + mr + 3 * k; x++) {
      if (x < 0 || y < 0 || x >= W || y >= H) continue;
      float xn = (x + 0.5f - mx) / mr, yn = (y + 0.5f - my) / mr, r2 = xn * xn + yn * yn, rr = geom_sqrt(r2);
      uint8_t *px = &fb_row(fb, y)[x];
      if (rr > 1) {
        if (rr < 1 + rim && dith(x, y, 0.3f * (1 - (rr - 1) / rim))) *px = C8(0x000055);
        continue;
      }
      float w = 1 - yn * yn, xt = geom_sqrt(w > 0 ? w : 0) * kx;
      bool lit = s_moon < 0.5f ? xn > xt : xn < -xt;
      *px = !lit ? C8(0x000055) : r2 > 0.72f ? C8(0xAAAAAA) : C8(0xFFFFFF);
    }
  graphics_release_frame_buffer(ctx, fb);
}

static void shade_pass(GContext *ctx, const Layout *L) {
  int W = L->W, H = L->H;
  float k = L->k;
  uint8_t celeste = scene_theme()->snow_shade;
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  for (int x = 0; x < W; x++) {
    float ry = layout_ridge_y(L, x);
    for (int y = iceil(ry); y < ry + 8 * k && y < H; y++)
      if (dith(x, y, (1 - (y - ry) / (8 * k)) * 0.6f)) fb_row(fb, y)[x] = celeste;
    float by = layout_berm_y(L, x);
    for (int y = fb_floor(by - 1); y < by + 5 * k && y < H; y++) {
      float dy = y - by, p = dy <= 0 ? 0.75f : (1 - dy / (5 * k)) * 0.5f;
      if (y >= 0 && dith(x, y, p)) fb_row(fb, y)[x] = celeste;
    }
  }
  graphics_release_frame_buffer(ctx, fb);
}

#ifdef PBL_PLATFORM_EMERY   // basalt has no heap left for the night extras' code
// Night: window light spilled on the snow in front of the chalet, then the floodlight cones over the open slope.
static void night_pass(GContext *ctx, const Layout *L, const FloodHead *heads) {
  int W = L->W;
  float k = L->k, cb = L->chalet.y + L->chalet.h, cx = L->chalet.x, cw = L->chalet.w;
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  for (int y = fb_round(cb - 1); y < cb + 5 * k && y < L->H; y++)
    for (int x = fb_round(cx - 6 * k); x < cx + cw * 0.6f && x < W; x++) {
      float ax = x - (cx + cw * 0.2f), p = 0.5f * (1 - (y - cb) / (5 * k)) * (1 - (ax < 0 ? -ax : ax) / (cw * 0.55f));
      if (x >= 0 && p > 0 && dith(x, y, p)) fb_row(fb, y)[x] = C8(0xFFFFAA);
    }
  int16_t ylo[W];
  for (int x = 0; x < W; x++) ylo[x] = iceil(layout_ridge_y(L, x) + 1);
  for (int x0 = 0; x0 < W; x0 += 2)       // flood intensity is sampled once per 2x2 block; the dither hides it
    for (int y0 = 0; y0 < L->H; y0 += 2) {
      int cream = 0, white = 0;   // dither thresholds, valid once evaluated
      bool evaluated = false;
      for (int x = x0; x < x0 + 2 && x < W; x++) {
        int yhi = s_berm_rows[x];
        for (int y = y0 > ylo[x] ? y0 : ylo[x]; y < y0 + 2 && y < yhi; y++) {
          if (!evaluated) {
            float I = flood_at(heads, FLOOD_COUNT, k, x0 + 1.0f, y0 + 1.0f) * 1.3f;
            evaluated = true;
            if (I > 0) { cream = iceil((I - 0.68f) * 256); white = iceil(I * 256); }
          }
          if (white <= 0) break;
          int b = aval_bayer(x, y) * 16 + 8;
          if (b < cream) fb_row(fb, y)[x] = C8(0xFFFFAA);
          else if (b < white) fb_row(fb, y)[x] = C8(0xFFFFFF);
        }
      }
    }
  graphics_release_frame_buffer(ctx, fb);
}

// Pole, foot, and a lamp housing tipped toward its aim with the lit face on the aimed side.
static void flood_poles(GContext *ctx, const FloodHead *heads) {
  for (int i = 0; i < FLOOD_COUNT; i++) {
    const FloodHead *f = &heads[i];
    int bx = fb_round(f->bx), by = fb_round(f->by), hx = fb_round(f->hx), hy = fb_round(f->hy), sx = f->dx < 0 ? -1 : 1;
    rect(ctx, bx, hy + 1, 1, by - hy, GColorOxfordBlue);
    rect(ctx, bx - 1, by - 1, 3, 1, GColorOxfordBlue);
    rect(ctx, hx - (sx < 0 ? 2 : 0), hy - 1, 3, 3, GColorOxfordBlue);
    rect(ctx, hx + 3 * sx, hy - 1, 1, 3, GColorIcterine);
    rect(ctx, hx + 4 * sx, hy, 1, 1, GColorPastelYellow);
  }
}

#endif

static uint8_t pal_index(uint8_t c) {
  for (uint8_t i = 0; i < s_npal; i++) if (s_pal[i] == c) return i;
  if (s_npal < 16) { s_pal[s_npal] = c; return s_npal++; }
  static bool logged;
  if (!logged) { logged = true; APP_LOG(APP_LOG_LEVEL_WARNING, "scene palette full, colour %02x dropped", c); }
  return 0;
}

static void pack_row(const uint8_t *src, uint8_t *dst, int W) {
  for (int x = 0; x < W; x += 2) {
    uint8_t lo = pal_index(src[x]), hi = x + 1 < W ? pal_index(src[x + 1]) : 0;
    dst[x >> 1] = (uint8_t)(lo | hi << 4);
  }
}

static void unpack_row(const uint8_t *src, uint8_t *dst, int W) {
  for (int x = 0; x < W; x += 2) {
    uint8_t b = src[x >> 1];
    dst[x] = s_pal[b & 15];
    if (x + 1 < W) dst[x + 1] = s_pal[b >> 4];
  }
}

#ifdef PBL_PLATFORM_EMERY
// Open-slope pines with behind == which, back to front (the list is sorted by base), each on its own shadow.
static void slope_trees(GContext *ctx, uint8_t which) {
  GColor shade = SCENE_ROLE(snow_shade);
  for (int i = 0; i < SLOPE_TREE_COUNT; i++) {
    const SlopeTree *t = &SLOPE_TREES[i];
    if (t->behind != which) continue;
    fb_fill_ellipse(ctx, t->x + 2, t->base, t->h * 0.42f, 1.6f, shade);
    pine(ctx, t->x, t->base, t->h);
  }
}
#endif

#ifdef PBL_PLATFORM_EMERY
// Partly-cloudy cloud: five bumps over a flat-based ellipse, in shade, mid and lit layers (roles lit, mid, shade).
static void flat_cloud(GContext *ctx, float x, float y, const Col8 *c, float s) {
  static const int8_t bumps[5][4] = { { -20, 1, 6, 4 }, { -10, -1, 8, 5 }, { 1, -2, 9, 6 }, { 12, -1, 8, 5 }, { 22, 1, 6, 4 } };
  static const float layer[3][6] = {   // dx, dy, rx scale, ry scale, base dy, base h
    { 1, 1.5f, 1, 1, 4, 3.2f }, { 0, 0.6f, 0.96f, 0.95f, 3, 2.4f }, { -0.8f, -0.8f, 0.82f, 0.75f, 0, 0 } };
  for (int l = 0; l < 3; l++) {
    const float *q = layer[l];
    GColor col = gcol(c[2 - l]);
    for (int i = 0; i < 5; i++)
      fb_fill_ellipse(ctx, x + bumps[i][0] * s + q[0], y + bumps[i][1] * s + q[1], bumps[i][2] * s * q[2], bumps[i][3] * s * q[3], col);
    if (q[5] > 0) fb_fill_ellipse(ctx, x + s, y + q[4] * s, 28 * s, q[5] * s, col);
  }
}

// Deck cloud: seven round puffs in shade, mid and lit layers.
static void deck_cloud(GContext *ctx, float x, float y, const Col8 *c, float s) {
  static const int8_t puffs[7][3] = { { -14, 4, 7 }, { -6, -2, 9 }, { 5, -4, 10 }, { 15, 1, 8 }, { 22, 5, 6 }, { 0, 6, 9 }, { -18, 7, 5 } };
  static const float layer[3][3] = { { 1, 2, 1 }, { 0, 0.8f, 0.96f }, { -0.8f, -0.8f, 0.8f } };   // dx, dy, r scale
  for (int l = 0; l < 3; l++) {
    GColor col = gcol(c[2 - l]);
    for (int i = 0; i < 7; i++) {
      float r = puffs[i][2] * s * layer[l][2];
      fb_fill_ellipse(ctx, x + puffs[i][0] * s + layer[l][0], y + puffs[i][1] * s + layer[l][1], r, r, col);
    }
  }
}

// Two rows of deck clouds; the back row uses the darker roles.
static void deck_pass(GContext *ctx, const Layout *L, const Col8 *cl) {
  float k = L->k;
  int W = L->W;
  const Col8 back[3] = { cl[1], cl[2], cl[2] };
  Mulberry r = { 5 };
  for (float x = -4; x < W + 20; x += 18 * k) {
    float y = (16 + mulberry_next(&r) * 6) * k;
    deck_cloud(ctx, x, y, back, (0.65f + mulberry_next(&r) * 0.25f) * k);
  }
  for (float x = -10; x < W + 20; x += 15 * k) {
    float y = (3 + mulberry_next(&r) * 6) * k;
    deck_cloud(ctx, x, y, cl, (0.75f + mulberry_next(&r) * 0.3f) * k);
  }
}

// Dithered fog over every row above the strip, thinning from 0.08H down to 0.58H.
static void fog_pass(GContext *ctx, const Layout *L, int fog256, Col8 c) {
  int W = L->W, H = L->H;
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  for (int y = 0; y < L->comp_top; y++) {
    float f = (y - H * 0.08f) / (H * 0.5f);
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    float p256 = fog256 * (1 - f);
    if (p256 <= 0) continue;
    uint8_t *row = fb_row(fb, y);
    for (int x = 0; x < W; x++)
      if (aval_bayer(x, y) * 16 + 8 < p256) row[x] = c;
  }
  graphics_release_frame_buffer(ctx, fb);
}
#endif

static bool build_bg(GContext *ctx, const Layout *L) {
  int W = L->W, H = L->H;
  float k = L->k;
  s_npal = 0;
  graphics_context_set_antialiased(ctx, false);   // crisp pixel art: palette colours only, no blended edges
  const Theme *th = scene_theme();
#ifdef PBL_PLATFORM_EMERY
  Daypart d = theme_daypart(th);
  const WxDef *df = wx_def(s_wx);
  sky_pass(ctx, L);
  if (!df->deck) {
    if (th->stars) stars_pass(ctx, L);
    if (th->body.kind == BODY_MOON) moon_pass(ctx, L);
  }
  static const float flat[3][3] = { { .62f, .07f, 1 }, { .2f, .06f, .8f }, { .9f, .13f, .7f } };
  for (int i = 0; i < df->clouds && i < 3; i++)
    flat_cloud(ctx, flat[i][0] * W, flat[i][1] * H, wx_cloud_roles(d, s_wx), flat[i][2] * k);
  if (df->deck) deck_pass(ctx, L, wx_cloud_roles(d, s_wx));
#else
  sky_pass(ctx, L);
  if (th->stars) stars_pass(ctx, L);
  if (th->body.kind == BODY_MOON) moon_pass(ctx, L);
#endif

  static const float far[5][3] = { { .05f, .17f, .17f }, { .22f, .12f, .15f }, { .40f, .10f, .17f },
                                   { .58f, .06f, .15f }, { .74f, .035f, .12f } };
  for (int i = 0; i < 5; i++)
    peak(ctx, far[i][0] * W, far[i][1] * H, far[i][2] * W, H * 0.45f,
         gcol(th->far[0]), gcol(th->far[1]), gcol(th->far[2]), gcol(th->far[3]));
  static const float near[3][3] = { { -.04f, .215f, .2f }, { .18f, .22f, .15f }, { .34f, .19f, .13f } };
  for (int i = 0; i < 3; i++)
    peak(ctx, near[i][0] * W, near[i][1] * H, near[i][2] * W, H * 0.5f,
         gcol(th->near_[0]), gcol(th->near_[1]), gcol(th->near_[2]), gcol(th->near_[3]));

  Mulberry r = { 7 };
  for (float x = W * 0.16f; x < W * 0.8f; x += 3.5f * k + mulberry_next(&r) * 3 * k) {
    float b = layout_ridge_y(L, x) + 1, h = (4 + mulberry_next(&r) * 5) * k * (x > W * 0.62f ? 0.6f : 1), hw = h * 0.35f;
    fb_fill_tri(ctx, x - hw, b, x, b - h, x + hw, b, gcol(th->forest));
  }

  Mulberry r3 = { 33 };
  for (float x = W * 0.17f; x < W * 0.86f; x += (4 + mulberry_next(&r3) * 5) * k) {
    float base = layout_ridge_y(L, x) + 2 * k, h = (8 + mulberry_next(&r3) * 7) * k;
    pine(ctx, x, base, h < base - 2 ? h : base - 2);
  }

  GPoint *slope = (GPoint *)s_bg;   // s_bg is free scratch until the pack below; basalt has no RAM for a static copy
  int n = 0;
  slope[n++] = GPoint(-2, fb_floor(layout_ridge_y(L, 0)));
  for (int i = 0; i <= (W + 1) / 2; i++) slope[n++] = GPoint(i * 2, fb_floor(layout_ridge_y(L, i * 2)));
  slope[n++] = GPoint(W + 2, fb_floor(layout_ridge_y(L, W)));
  slope[n++] = GPoint(W + 2, H + 2);
  slope[n++] = GPoint(-2, H + 2);
  fb_fill_poly(ctx, slope, (uint16_t)n, gcol(th->snow));

  shade_pass(ctx, L);
#ifdef PBL_PLATFORM_EMERY
  if (th->night) {
    FloodHead heads[FLOOD_COUNT];
    flood_heads(W, H, k, heads);
    night_pass(ctx, L, heads);
    flood_poles(ctx, heads);
  }
#endif

  float trees[MAX_TREES][3];
  int nt = 0;
  Mulberry r2 = { 21 };
  for (float x = W * 0.2f; x < W * 0.8f && nt < MAX_TREES; x += (5 + mulberry_next(&r2) * 6) * k) {
    float base = layout_ridge_y(L, x) + 1.5f * k, h = (11 + mulberry_next(&r2) * 11) * k;
    if (h > base - 3) h = base - 3;
    float tx = x + mulberry_next(&r2) * 3 * k;
    int j = nt++;
    for (; j > 0 && trees[j - 1][1] > base; j--) memcpy(trees[j], trees[j - 1], sizeof trees[j]);
    trees[j][0] = tx; trees[j][1] = base; trees[j][2] = h;
  }
  for (int i = 0; i < nt; i++) pine(ctx, trees[i][0], trees[i][1], trees[i][2]);
#ifdef PBL_PLATFORM_EMERY
  if (df->fog256) fog_pass(ctx, L, df->fog256, wx_fog_colour(d));
  slope_trees(ctx, 0);   // no skier passes behind these, so they cost nothing per frame
#endif

  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return false;
  for (int y = 0; y < H; y++) pack_row(fb_row(fb, y), s_bg + y * s_stride, W);
  graphics_release_frame_buffer(ctx, fb);
  return true;
}

bool scene_init(const Layout *L) {
  int W = L->W, H = L->H;
  s_stride = (int16_t)((W + 1) / 2);
  s_npal = 0;
  s_bg = malloc(s_stride * H);
  s_ridge_rows = malloc(W * sizeof(int16_t));
  s_berm_rows = malloc(W * sizeof(int16_t));
  if (!s_bg || !s_ridge_rows || !s_berm_rows) { scene_deinit(); return false; }
  for (int x = 0; x < W; x++) {
    s_ridge_rows[x] = iceil(layout_ridge_y(L, x) + 0.5f);
    int16_t b = fb_floor(layout_berm_y(L, x));
    s_berm_rows[x] = b < H ? b : H;
  }
  s_bg_valid = false;
  return true;
}

void scene_deinit(void) {
  free(s_bg); free(s_ridge_rows); free(s_berm_rows);
  s_bg = NULL; s_ridge_rows = s_berm_rows = NULL;
  s_bg_valid = false;
}

void scene_invalidate(void) { s_bg_valid = false; }

bool scene_blit(GContext *ctx, const Layout *L) {
  if (!s_bg) return false;
  if (!s_bg_valid) { s_bg_valid = build_bg(ctx, L); return true; }
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return false;
  for (int y = 0; y < L->H; y++) unpack_row(s_bg + y * s_stride, fb_row(fb, y), L->W);
  graphics_release_frame_buffer(ctx, fb);
  return false;
}

bool scene_ready(void) { return s_bg && s_bg_valid; }

bool scene_bg_row(int y, uint8_t *out, int W) {
  if (!s_bg || !s_bg_valid) return false;
  unpack_row(s_bg + y * s_stride, out, W);
  return true;
}
const int16_t *scene_ridge_rows(void) { return s_ridge_rows; }
const int16_t *scene_berm_rows(void) { return s_berm_rows; }

// Front log lines, clipped to the gable-ended front face (apex at ay + 2k).
static void front_logs(GContext *ctx, float fx0, float fx1, float ax, float apex, float ey, float y0, float b, float step) {
  float fw = fx1 - fx0;
  graphics_context_set_fill_color(ctx, GColorBulgarianRose);
  for (float y = y0; y < b; y += step) {
    int ry = fb_round(y);
    float yc = ry + 0.5f, hw = yc >= ey ? fw / 2 : (yc - apex) / (ey - apex) * fw / 2;
    if (hw <= 0) continue;
    int16_t xa = fb_round(ax - hw), xb = fb_round(ax + hw);
    if (xb > xa) graphics_fill_rect(ctx, GRect(xa, ry, xb - xa, 1), 0, GCornerNone);
  }
}

void scene_draw_chalet(GContext *ctx, const Layout *L) {
  float k = L->k;
  Rect16 c = L->chalet;
  float fw = c.w * 0.52f, fx0 = c.x, fx1 = c.x + fw, ex = c.x + c.w, b = c.y + c.h, rise = c.h * 0.1f, run = ex - fx1;
  float ey = c.y + c.h * 0.5f, ax = fx0 + fw / 2, ay = c.y + c.h * 0.06f;
#define UP(x) (rise * ((x) - fx1) / run)
  graphics_context_set_antialiased(ctx, false);
  const Theme *th = scene_theme();
  const GColor powder = gcol(th->powder), shade = gcol(th->snow_shade);

  pine(ctx, ex - 3 * k, b - rise - c.h * 0.25f, c.h * 0.9f);
  fill_rot_ellipse(ctx, fx1 + run * 0.55f, b - rise * 0.4f + 1, c.w * 0.42f, 2.5f * k, -0.1f, shade);

  // side wall in shade, glowing windows
  const float wall[8] = { fx1, ey, ex, ey - rise, ex, b - rise, fx1, b };
  poly(ctx, wall, 4, GColorBulgarianRose);
  float wh = (b - ey) * 0.36f;
  const float wf[2] = { 0.18f, 0.58f };
  for (int i = 0; i < 2; i++) {
    float x0 = fx1 + run * wf[i], x1 = x0 + run * 0.26f, y0 = ey + (b - ey) * 0.28f;
    const float win[8] = { x0, y0 - UP(x0), x1, y0 - UP(x1), x1, y0 + wh - UP(x1), x0, y0 + wh - UP(x0) };
    poly(ctx, win, 4, GColorChromeYellow);
    float xm = (x0 + x1) / 2;
    rect(ctx, fb_round(xm), fb_round(y0 - UP(xm)), 1, fb_round(wh), GColorBulgarianRose);
  }

  // front face in sun: logs, door, balcony, gable window
  const float front[10] = { ax, ay + 2 * k, fx0, ey, fx0, b, fx1, b, fx1, ey };
  poly(ctx, front, 5, GColorWindsorTan);
  front_logs(ctx, fx0, fx1, ax, ay + 2 * k, ey, ay + 4 * k, b, 3 * k);
  int dw = fb_round(fw * 0.3f), dh = fb_round((b - ey) * 0.58f);
  rect(ctx, fb_round(ax - dw / 2.0f), fb_round(b - dh), dw, dh, GColorBulgarianRose);
  rect(ctx, fb_round(ax + dw / 2.0f) - 2, fb_round(b - dh * 0.5f), 1, 1, GColorChromeYellow);
  rect(ctx, fb_round(fx0 - 1), fb_round(ey), fb_round(fw + 2), 2, GColorBulgarianRose);
  for (float x = fx0; x <= fx1; x += 3 * k)
    rect(ctx, fb_round(x), fb_round(ey - 3 * k), 1, fb_round(3 * k), GColorBulgarianRose);
  rect(ctx, fb_round(fx0 - 1), fb_round(ey - 3 * k), fb_round(fw + 2), 1, GColorBulgarianRose);
  int ws = fb_round(fw * 0.26f);
  if (ws < 4) ws = 4;
  int wx = fb_round(ax - ws / 2.0f), wy = fb_round(ay + (ey - ay) * 0.42f);
  rect(ctx, wx, wy, ws, ws, GColorChromeYellow);
  rect(ctx, wx + 1, wy + 1, ws - 2, ws - 2, GColorIcterine);
  rect(ctx, fb_round(ax), wy, 1, ws, GColorBulgarianRose);

  // side roof plane under snow, with eave band, fascia and icicles
  float o = 2 * k;
  const float roof[8] = { ax, ay, ax + run, ay - rise, ex + o, ey - rise + o, fx1 + o, ey + o };
  poly(ctx, roof, 4, powder);
  const float eave[8] = { fx1 + o, ey + o, ex + o, ey - rise + o, ex + o, ey - rise + o - 2.5f * k, fx1 + o, ey + o - 2.5f * k };
  poly(ctx, eave, 4, shade);
  // 1-px fascia band [edge, edge+1): the row whose centre falls inside it, per column
  for (int x = fb_floor(fx1 + o); x < fb_round(ex + o); x++) {
    float edge = ey + o - rise * (x + 0.5f - (fx1 + o)) / run;
    rect(ctx, x, iceil(edge - 0.5f), 1, 1, GColorBulgarianRose);
  }
  for (float x = fx1 + o + 2 * k; x < ex; x += 3 * k)
    rect(ctx, fb_round(x), fb_round(ey + o + 1 - UP(x)), 1, 1 + ((int)(x * 7) % 2), shade);

  // front bargeboards with snow
  const float ends[2][2] = { { fx0 - o, ey + o }, { fx1 + o, ey + o } };
  for (int i = 0; i < 2; i++) {
    capsule(ctx, ax, ay, ends[i][0], ends[i][1], 2 * k, GColorBulgarianRose);
    capsule(ctx, ax, ay - k, ends[i][0], ends[i][1] - 1.2f * k, 1.4f * k, powder);
  }

  // chimney on the side roof
  float cx = ax + run * 0.62f, cy = ay - rise * 0.62f + (ey - ay) * 0.3f;
  rect(ctx, fb_round(cx - 1.5f * k), fb_round(cy - 6 * k), fb_round(3 * k), fb_round(7 * k), GColorDarkGray);
  rect(ctx, fb_round(cx - 2 * k), fb_round(cy - 6 * k - 1), fb_round(4 * k), 2, powder);
  s_chim_x = cx; s_chim_y = cy - 6 * k - 2;
#undef UP
}

void scene_draw_smoke(GContext *ctx, const Layout *L, uint32_t ms) {
  float k = L->k, clock = ms / 1000.0f;
  GColor shade = SCENE_ROLE(snow_shade);
  for (int i = 0; i < 4; i++) {
    float p = clock * 0.22f + i / 4.0f;
    p -= fb_floor(p);
    if (p > 0.9f) continue;
    float r = (0.8f + p * 2.4f) * k;
    fb_fill_ellipse(ctx, s_chim_x + p * 8 * k + geom_sin(p * 6 + i) * 1.2f * k, s_chim_y - p * 16 * k, r, r,
                    p < 0.45f ? GColorLightGray : shade);
  }
}

void scene_restore_outside_slope(GContext *ctx, const Layout *L) {
  if (!s_bg || !s_bg_valid) return;
  int W = L->W, H = L->H;
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  uint8_t src[W];
  int y_lo = H, y_hi = 0;   // outside-slope pixels only exist above the highest ridge point or below the lowest berm point
  for (int x = 0; x < W; x++) {
    if (s_ridge_rows[x] > y_hi) y_hi = s_ridge_rows[x];
    if (s_berm_rows[x] < y_lo) y_lo = s_berm_rows[x];
  }
  if (y_hi > H) y_hi = H;
  if (y_lo < 0) y_lo = 0;
  for (int y = 0; y < H; y++) {
    if (y >= y_hi && y < y_lo) { y = y_lo - 1; continue; }
    uint8_t *row = fb_row(fb, y);
    unpack_row(s_bg + y * s_stride, src, W);
    for (int x = 0; x < W; x++)
      if (y < s_ridge_rows[x] || y >= s_berm_rows[x]) row[x] = src[x];
  }
  graphics_release_frame_buffer(ctx, fb);
}

#ifdef PBL_PLATFORM_EMERY
// Bayer-dithered ellipse at density p, pixel-centre spans as fb_fill_ellipse.
static void dither_ellipse(GBitmap *fb, int W, int H, float cx, float cy, float rx, float ry, Col8 c, float p) {
  for (int y = fb_floor(cy - ry); y <= fb_floor(cy + ry); y++) {
    float dy = (y + 0.5f - cy) / ry;
    if (y < 0 || y >= H || dy * dy > 1) continue;
    float hw = rx * geom_sqrt(1 - dy * dy);
    uint8_t *row = fb_row(fb, y);
    for (int x = fb_round(cx - hw); x < fb_round(cx + hw); x++)
      if (x >= 0 && x < W && dith(x, y, p)) row[x] = c;
  }
}

// One flame tongue: two quadratic curves from the base corners up to a swaying tip, filled at pixel centres
// (each row crosses the outline once on each side).
#define TONGUE_SEGS 6
static void tongue(GContext *ctx, float x, float y, float h, float sway, float s, float k, GColor c) {
  float px[2 * TONGUE_SEGS + 1], py[2 * TONGUE_SEGS + 1];
  float w = 1.9f * k * s, cw = 1.2f * k * s, cy = y - h * 0.6f * s, ty = y - h * s;
  for (int i = 0; i <= TONGUE_SEGS; i++) {
    float t = (float)i / TONGUE_SEGS, m = 1 - t, a = m * m, b = 2 * m * t, d = t * t;
    px[i] = a * (x - w) + b * (x - cw) + d * (x + sway);
    py[i] = a * y + b * cy + d * ty;
    px[TONGUE_SEGS + i] = a * (x + sway) + b * (x + cw) + d * (x + w);
    py[TONGUE_SEGS + i] = a * ty + b * cy + d * y;
  }
  graphics_context_set_fill_color(ctx, c);
  for (int row = fb_floor(ty); row <= fb_floor(y); row++) {
    float yc = row + 0.5f, lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 2 * TONGUE_SEGS; i++) {
      float y0 = py[i], y1 = py[i + 1];
      if ((y0 <= yc) == (y1 <= yc)) continue;
      float xc = px[i] + (yc - y0) * (px[i + 1] - px[i]) / (y1 - y0);
      if (xc < lo) lo = xc;
      if (xc > hi) hi = xc;
    }
    if (hi < lo) continue;
    int16_t x0 = fb_round(lo), x1 = fb_round(hi);
    if (x1 > x0) graphics_fill_rect(ctx, GRect(x0, row, x1 - x0, 1), 0, GCornerNone);
  }
}

void scene_draw_campfire(GContext *ctx, const Layout *L, uint32_t ms) {
  float k = L->k, clock = ms / 1000.0f, fx = L->chalet.x - 9 * k, fy = L->comp_top - 5 * k;
  graphics_context_set_antialiased(ctx, false);
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  dither_ellipse(fb, L->W, L->H, fx, fy, 10 * k, 3.2f * k, C8(0xFFAA55), 0.35f);
  dither_ellipse(fb, L->W, L->H, fx, fy, 5 * k, 1.8f * k, C8(0xFFFFAA), 0.3f);
  graphics_release_frame_buffer(ctx, fb);

  graphics_context_set_stroke_color(ctx, GColorBulgarianRose);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(fb_floor(fx - 4 * k), fb_floor(fy)), GPoint(fb_floor(fx + 4 * k), fb_floor(fy - 1.5f * k)));
  graphics_draw_line(ctx, GPoint(fb_floor(fx - 4 * k), fb_floor(fy - 1.5f * k)), GPoint(fb_floor(fx + 4 * k), fb_floor(fy)));

  static const uint8_t flame[4] = { C8(0xFF0000), C8(0xFF5500), C8(0xFFAA00), C8(0xFFFF55) };
  static const float scale[4] = { 1, 0.72f, 0.46f, 0.24f };
  for (int i = 0; i < 3; i++) {
    float ox = (i - 1) * 1.8f * k, h = (5 + 2.4f * aval_noise(clock * 6 + i * 2.1f)) * k * (i == 1 ? 1.3f : 0.85f);
    float sw = geom_sin(clock * 9 + i * 1.7f) * 0.9f * k;
    for (int j = 0; j < (i == 1 ? 4 : 3); j++)
      tongue(ctx, fx + ox, fy - 0.8f * k, h, sw, scale[j], k, gcol(flame[j]));
  }
  for (float x = fx - 5 * k; x <= fx + 5 * k; x += 2.5f * k) rect(ctx, fb_round(x), fb_round(fy), 1, 1, GColorDarkGray);
  for (int i = 0; i < 3; i++) {
    float q = clock * 0.8f + i / 3.0f;
    q -= fb_floor(q);
    if (q > 0.8f) continue;
    rect(ctx, fb_round(fx + geom_sin(q * 9 + i) * 2 * k), fb_round(fy - 7 * k - q * 10 * k), 1, 1,
         q < 0.45f ? GColorChromeYellow : GColorOrange);
  }
}
#endif

#ifdef PBL_PLATFORM_EMERY
// Per-frame open-slope pines (the ones skiers pass behind), back to front, drawn over the skiers.
void scene_draw_slope_trees(GContext *ctx, const Layout *L) {
  (void)L;
  graphics_context_set_antialiased(ctx, false);
  slope_trees(ctx, 1);
}
#endif
