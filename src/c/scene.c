#include <pebble.h>
#include "scene.h"
#include "fb.h"
#include "geom.h"
#include "avalanche.h"

#define SLOPE_PTS 110
#define MAX_TREES 40

// Background packed at 4 bits per pixel (low nibble = even x) with a palette of up to 16 GColor8
// bytes: the scene uses about 13 colours, and on basalt the 8-bit copy would not leave room for the skiers.
static uint8_t *s_bg, s_pal[16], s_npal;
static int16_t s_stride;
static int16_t *s_ridge_rows, *s_berm_rows;
static bool s_bg_valid;
static float s_chim_x, s_chim_y;   // smoke origin, kept in float so the puffs drift sub-pixel as in the prototype

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
  rect(ctx, fb_round(x - 0.5f), fb_round(base - h * 0.16f), 1, iceil(h * 0.16f), GColorBulgarianRose);
  for (int i = 0; i < 3; i++) {
    float top = base - h + i * h * 0.27f, bot = top + h * 0.46f, hw = h * (0.2f + i * 0.1f), th = bot - top;
    fb_fill_tri(ctx, x - hw, bot, x, top, x, bot, GColorDarkGreen);
    fb_fill_tri(ctx, x, top, x + hw, bot, x, bot, GColorMidnightGreen);
    const float snow[8] = { x, top, x - hw * 0.55f, top + th * 0.55f, x - hw * 0.2f, top + th * 0.42f,
                            x + hw * 0.25f, top + th * 0.45f };
    poly(ctx, snow, 4, GColorWhite);
  }
}

static void peak(GContext *ctx, float px, float py, float hw, float base,
                 GColor lit, GColor shade, GColor cap_lit, GColor cap_shade) {
  float split = px + hw * 0.12f;
  fb_fill_tri(ctx, px - hw, base, px, py, split, base, lit);
  fb_fill_tri(ctx, px, py, px + hw, base, split, base, shade);
  float dep = (base - py) * 0.3f, lx = px - hw * 0.3f, rx = px + hw * 0.3f, ly = py + dep, ry = py + dep * 1.1f;
  const float cl[10] = { px, py, lx, ly, lx + (px - lx) * 0.35f, ly - dep * 0.25f, px - hw * 0.05f, ly + dep * 0.1f,
                         px + hw * 0.02f, ly - dep * 0.2f };
  poly(ctx, cl, 5, cap_lit);
  const float cr[10] = { px, py, px + hw * 0.02f, ly - dep * 0.2f, px + hw * 0.12f, ry - dep * 0.05f,
                         px + hw * 0.2f, ry - dep * 0.35f, rx, ry };
  poly(ctx, cr, 5, cap_shade);
}

static void sky_pass(GContext *ctx, const Layout *L) {
  int W = L->W, H = L->H;
  float k = L->k, sx = W * 0.13f, sy = H * 0.08f, sr = 7 * k, halo = 6 * k, horizon = layout_ridge_y(L, 0) * 0.92f;
  uint8_t celeste = GColorCeleste.argb, picton = GColorPictonBlue.argb;
  uint8_t sun = GColorIcterine.argb, glow = GColorPastelYellow.argb;
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  for (int y = 0; y < H; y++) {
    float f = y / horizon;
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    float t = f * root5(f), dy = y + 0.5f - sy;
    uint8_t *row = fb_row(fb, y);
    for (int x = 0; x < W; x++) {
      row[x] = dith(x, y, t) ? celeste : picton;
      float dx = x + 0.5f - sx, d2 = dx * dx + dy * dy;
      if (d2 < sr * sr) row[x] = sun;
      else if (d2 < (sr + halo) * (sr + halo)) {
        float q = 1 - (geom_sqrt(d2) - sr) / halo, q5 = root5(q);
        if (dith(x, y, q * q5 * q5 * 0.85f)) row[x] = glow;
      }
    }
  }
  graphics_release_frame_buffer(ctx, fb);
}

static void shade_pass(GContext *ctx, const Layout *L) {
  int W = L->W, H = L->H;
  float k = L->k;
  uint8_t celeste = GColorCeleste.argb;
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

static uint8_t pal_index(uint8_t c) {
  for (uint8_t i = 0; i < s_npal; i++) if (s_pal[i] == c) return i;
  if (s_npal < 16) { s_pal[s_npal] = c; return s_npal++; }
  APP_LOG(APP_LOG_LEVEL_WARNING, "scene palette full, colour %02x dropped", c);
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

static void build_bg(GContext *ctx, const Layout *L) {
  int W = L->W, H = L->H;
  float k = L->k;
  s_npal = 0;
  graphics_context_set_antialiased(ctx, false);   // crisp pixel art: palette colours only, no blended edges
  sky_pass(ctx, L);

  static const float far[5][3] = { { .05f, .17f, .17f }, { .22f, .12f, .15f }, { .40f, .10f, .17f },
                                   { .58f, .06f, .15f }, { .74f, .035f, .12f } };
  for (int i = 0; i < 5; i++)
    peak(ctx, far[i][0] * W, far[i][1] * H, far[i][2] * W, H * 0.45f,
         GColorBabyBlueEyes, GColorLiberty, GColorWhite, GColorCeleste);
  static const float near[3][3] = { { -.04f, .215f, .2f }, { .18f, .22f, .15f }, { .34f, .19f, .13f } };
  for (int i = 0; i < 3; i++)
    peak(ctx, near[i][0] * W, near[i][1] * H, near[i][2] * W, H * 0.5f,
         GColorCobaltBlue, GColorOxfordBlue, GColorWhite, GColorBabyBlueEyes);

  Mulberry r = { 7 };
  for (float x = W * 0.16f; x < W * 0.8f; x += 3.5f * k + mulberry_next(&r) * 3 * k) {
    float b = layout_ridge_y(L, x) + 1, h = (4 + mulberry_next(&r) * 5) * k * (x > W * 0.62f ? 0.6f : 1), hw = h * 0.35f;
    fb_fill_tri(ctx, x - hw, b, x, b - h, x + hw, b, GColorCadetBlue);
  }

  Mulberry r3 = { 33 };
  for (float x = W * 0.17f; x < W * 0.86f; x += (4 + mulberry_next(&r3) * 5) * k) {
    float base = layout_ridge_y(L, x) + 2 * k, h = (8 + mulberry_next(&r3) * 7) * k;
    pine(ctx, x, base, h < base - 2 ? h : base - 2);
  }

  static GPoint slope[SLOPE_PTS];
  int n = 0;
  slope[n++] = GPoint(-2, fb_floor(layout_ridge_y(L, 0)));
  for (int i = 0; i <= (W + 1) / 2; i++) slope[n++] = GPoint(i * 2, fb_floor(layout_ridge_y(L, i * 2)));
  slope[n++] = GPoint(W + 2, fb_floor(layout_ridge_y(L, W)));
  slope[n++] = GPoint(W + 2, H + 2);
  slope[n++] = GPoint(-2, H + 2);
  fb_fill_poly(ctx, slope, (uint16_t)n, GColorWhite);

  shade_pass(ctx, L);

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

  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  for (int y = 0; y < H; y++) pack_row(fb_row(fb, y), s_bg + y * s_stride, W);
  graphics_release_frame_buffer(ctx, fb);
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

void scene_blit(GContext *ctx, const Layout *L) {
  if (!s_bg) return;
  if (!s_bg_valid) { build_bg(ctx, L); s_bg_valid = true; return; }
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  for (int y = 0; y < L->H; y++) unpack_row(s_bg + y * s_stride, fb_row(fb, y), L->W);
  graphics_release_frame_buffer(ctx, fb);
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

  pine(ctx, ex - 3 * k, b - rise - c.h * 0.25f, c.h * 0.9f);
  fill_rot_ellipse(ctx, fx1 + run * 0.55f, b - rise * 0.4f + 1, c.w * 0.42f, 2.5f * k, -0.1f, GColorCeleste);

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
  poly(ctx, roof, 4, GColorWhite);
  const float eave[8] = { fx1 + o, ey + o, ex + o, ey - rise + o, ex + o, ey - rise + o - 2.5f * k, fx1 + o, ey + o - 2.5f * k };
  poly(ctx, eave, 4, GColorCeleste);
  // 1-px fascia band [edge, edge+1): the row whose centre falls inside it, per column
  for (int x = fb_floor(fx1 + o); x < fb_round(ex + o); x++) {
    float edge = ey + o - rise * (x + 0.5f - (fx1 + o)) / run;
    rect(ctx, x, iceil(edge - 0.5f), 1, 1, GColorBulgarianRose);
  }
  for (float x = fx1 + o + 2 * k; x < ex; x += 3 * k)
    rect(ctx, fb_round(x), fb_round(ey + o + 1 - UP(x)), 1, 1 + ((int)(x * 7) % 2), GColorCeleste);

  // front bargeboards with snow
  const float ends[2][2] = { { fx0 - o, ey + o }, { fx1 + o, ey + o } };
  for (int i = 0; i < 2; i++) {
    capsule(ctx, ax, ay, ends[i][0], ends[i][1], 2 * k, GColorBulgarianRose);
    capsule(ctx, ax, ay - k, ends[i][0], ends[i][1] - 1.2f * k, 1.4f * k, GColorWhite);
  }

  // chimney on the side roof
  float cx = ax + run * 0.62f, cy = ay - rise * 0.62f + (ey - ay) * 0.3f;
  rect(ctx, fb_round(cx - 1.5f * k), fb_round(cy - 6 * k), fb_round(3 * k), fb_round(7 * k), GColorDarkGray);
  rect(ctx, fb_round(cx - 2 * k), fb_round(cy - 6 * k - 1), fb_round(4 * k), 2, GColorWhite);
  s_chim_x = cx; s_chim_y = cy - 6 * k - 2;
#undef UP
}

void scene_draw_smoke(GContext *ctx, const Layout *L, uint32_t ms) {
  float k = L->k, clock = ms / 1000.0f;
  for (int i = 0; i < 4; i++) {
    float p = clock * 0.22f + i / 4.0f;
    p -= fb_floor(p);
    if (p > 0.9f) continue;
    float r = (0.8f + p * 2.4f) * k;
    fb_fill_ellipse(ctx, s_chim_x + p * 8 * k + geom_sin(p * 6 + i) * 1.2f * k, s_chim_y - p * 16 * k, r, r,
                    p < 0.45f ? GColorLightGray : GColorCeleste);
  }
}

void scene_restore_outside_slope(GContext *ctx, const Layout *L) {
  if (!s_bg || !s_bg_valid) return;
  int W = L->W, H = L->H;
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  uint8_t src[W];
  for (int y = 0; y < H; y++) {
    uint8_t *row = fb_row(fb, y);
    unpack_row(s_bg + y * s_stride, src, W);
    for (int x = 0; x < W; x++)
      if (y < s_ridge_rows[x] || y >= s_berm_rows[x]) row[x] = src[x];
  }
  graphics_release_frame_buffer(ctx, fb);
}
