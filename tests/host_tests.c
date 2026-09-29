#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "geom.h"
#include "glyphs.h"

static int g_fail = 0;

#define CHECK(cond) do { if (!(cond)) { \
  fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); g_fail++; } } while (0)

#define CHECK_EQ_INT(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
  fprintf(stderr, "FAIL %s:%d  %s == %s   (%ld != %ld)\n", __FILE__, __LINE__, #a, #b, _a, _b); \
  g_fail++; } } while (0)

#define CHECK_NEAR(a, b, eps) do { double _a = (double)(a), _b = (double)(b); if (fabs(_a - _b) > (eps)) { \
  fprintf(stderr, "FAIL %s:%d  %s ~= %s   (%f vs %f)\n", __FILE__, __LINE__, #a, #b, _a, _b); \
  g_fail++; } } while (0)

static void test_harness_self(void) {
  CHECK(1 + 1 == 2);
  CHECK_EQ_INT(40 + 2, 42);
  CHECK_NEAR(0.1 + 0.2, 0.3, 1e-9);
}

static Vec2 ident_map(float u, float v, const void *c) { (void)c; return (Vec2){u, v}; }

static void test_geom_sqrt_trig(void) {
  CHECK_NEAR(geom_sqrt(2.0f), 1.41421356, 1e-5);
  CHECK_NEAR(geom_sqrt(10000.0f), 100.0, 1e-3);
  CHECK_NEAR(geom_sqrt(0.0f), 0.0, 1e-9);
  CHECK_NEAR(geom_sin(1.5707963f), 1.0, 1e-3);
  CHECK_NEAR(geom_cos(3.1415926f), -1.0, 1e-3);
  CHECK_NEAR(geom_sin(-0.5f), sin(-0.5), 1e-3);
}

static void test_geom_sample_spacing(void) {
  // Straight line 0,0 -> 10,0 as one cubic segment, sampled every 1 unit.
  const float ch[] = { 0,0, 3,0, 7,0, 10,0 };
  Vec2 out[32];
  int n = geom_sample_chain(ch, 8, ident_map, NULL, 1.0f, out, 32);
  CHECK_EQ_INT(n, 12);                        // 0..10 at 1.0 plus the exact end point
  CHECK_NEAR(out[0].x, 0, 1e-4);
  CHECK_NEAR(out[5].x, 5, 1e-3);
  CHECK_NEAR(out[n - 1].x, 10, 1e-4);
  for (int i = 1; i < n - 1; i++) CHECK_NEAR(out[i].x - out[i - 1].x, 1.0, 1e-3);
  Vec2 t = geom_tangent(out, n, 3, 2);
  CHECK_NEAR(t.x, 1.0, 1e-4); CHECK_NEAR(t.y, 0.0, 1e-4);
  CHECK_EQ_INT(geom_sample_chain(ch, 8, ident_map, NULL, 1.0f, out, 4), 4);   // respects max
}

static void test_mulberry_matches_prototype(void) {
  Mulberry m = { 7 };
  CHECK_NEAR(mulberry_next(&m), 0.011704753, 1e-6);
  CHECK_NEAR(mulberry_next(&m), 0.061958258, 1e-6);
  CHECK_NEAR(mulberry_next(&m), 0.976907633, 1e-6);
}

static void test_glyph_data(void) {
  const uint8_t counts[10] = { 2,1,1,1,2,1,1,1,2,1 };
  for (uint8_t d = 0; d < 10; d++) {
    CHECK_EQ_INT(glyph_stroke_count(d), counts[d]);
    for (uint8_t j = 0; j < glyph_stroke_count(d); j++) {
      GlyphStroke g = glyph_stroke(d, j);
      CHECK((g.n - 2) % 6 == 0);
      CHECK(g.n >= 8);
      float y0 = g.pts[1], y1 = g.pts[g.n - 1];
      CHECK(y1 > y0);                         // every stroke finishes lower than it starts
      CHECK(y1 >= 0.6f);                      // and finishes in the lower part of the digit
    }
  }
}

static void test_time_slots(void) {
  DigitSlot s[4];
  CHECK_EQ_INT(time_slots(20, 46, true, true, s), 4);
  CHECK_EQ_INT(s[0].digit, 2); CHECK_EQ_INT(s[0].slot, 0);
  CHECK_EQ_INT(s[1].digit, 0); CHECK_EQ_INT(s[3].digit, 6); CHECK_EQ_INT(s[3].slot, 3);
  CHECK_EQ_INT(time_slots(8, 5, true, false, s), 3);           // "8:05"
  CHECK_EQ_INT(s[0].digit, 8); CHECK_EQ_INT(s[0].slot, 1);
  CHECK_EQ_INT(s[1].digit, 0); CHECK_EQ_INT(s[2].digit, 5);
  CHECK_EQ_INT(time_slots(8, 5, true, true, s), 4);            // "08:05"
  CHECK_EQ_INT(s[0].digit, 0);
  CHECK_EQ_INT(time_slots(0, 0, false, false, s), 4);          // 12-hour midnight -> "12:00"
  CHECK_EQ_INT(s[0].digit, 1); CHECK_EQ_INT(s[1].digit, 2);
  CHECK_EQ_INT(time_slots(13, 7, false, false, s), 3);         // "1:07"
  CHECK_EQ_INT(s[0].digit, 1); CHECK_EQ_INT(s[0].slot, 1);
  CHECK_EQ_INT(time_slots(13, 7, false, true, s), 4);          // "01:07"
  CHECK_EQ_INT(time_slots(12, 30, false, false, s), 4);        // noon -> "12:30"
}

#include "layout.h"

static void test_layout_emery(void) {
  Layout L; layout_init(&L, 200, 228);
  CHECK_NEAR(L.k, 1.0, 1e-6);
  CHECK_EQ_INT(L.comp_top, 196); CHECK_EQ_INT(L.gw, 42); CHECK_EQ_INT(L.gh, 50);
  CHECK_EQ_INT(L.boxes[0].x, 64);  CHECK_EQ_INT(L.boxes[0].y, 84);
  CHECK_EQ_INT(L.boxes[1].x, 113); CHECK_EQ_INT(L.boxes[1].y, 84);
  CHECK_EQ_INT(L.boxes[2].x, 50);  CHECK_EQ_INT(L.boxes[2].y, 140);
  CHECK_EQ_INT(L.boxes[3].x, 99);  CHECK_EQ_INT(L.boxes[3].y, 140);
  CHECK_EQ_INT(L.chalet.x, 149); CHECK_EQ_INT(L.chalet.y, 149);
  CHECK_EQ_INT(L.chalet.w, 48);  CHECK_EQ_INT(L.chalet.h, 46);
  CHECK_NEAR(layout_ridge_y(&L, 0), 84.0, 0.05);
  CHECK_NEAR(layout_ridge_y(&L, 100), 38.02, 0.05);
  CHECK_NEAR(layout_ridge_y(&L, 200), 8.0, 0.05);
  CHECK_NEAR(layout_berm_y(&L, 50), 194.579, 0.05);
  CHECK_NEAR(L.u0, -85.431, 0.1);  CHECK_NEAR(L.s0, 153.426, 0.1);
  CHECK_NEAR(L.u1, 176.4, 0.05);
  CHECK_NEAR(L.s_min, 0, 1e-4);    CHECK_NEAR(L.s_max, 296.8, 0.05);
}

static void test_layout_basalt(void) {
  Layout L; layout_init(&L, 144, 168);
  CHECK_NEAR(L.k, 0.72, 1e-6);
  CHECK_EQ_INT(L.comp_top, 144); CHECK_EQ_INT(L.gw, 30); CHECK_EQ_INT(L.gh, 36);
  CHECK_EQ_INT(L.boxes[0].x, 46); CHECK_EQ_INT(L.boxes[0].y, 63);
  CHECK_EQ_INT(L.boxes[1].x, 81); CHECK_EQ_INT(L.boxes[2].x, 36);
  CHECK_EQ_INT(L.boxes[2].y, 104); CHECK_EQ_INT(L.boxes[3].x, 71);
  CHECK_EQ_INT(L.chalet.x, 107); CHECK_EQ_INT(L.chalet.y, 110);
  CHECK_EQ_INT(L.chalet.w, 35);  CHECK_EQ_INT(L.chalet.h, 33);
  CHECK_NEAR(layout_ridge_y(&L, 0), 62.0, 0.05);
  CHECK_NEAR(layout_ridge_y(&L, 72), 28.12, 0.05);
  CHECK_NEAR(layout_berm_y(&L, 50), 142.697, 0.05);
  CHECK_NEAR(L.u0, -61.228, 0.1); CHECK_NEAR(L.u1, 130.32, 0.05);
  CHECK_NEAR(L.s_max, 216.0, 0.05);
}

static void test_layout_glyph_point(void) {
  Layout L; layout_init(&L, 200, 228);
  Vec2 top = layout_glyph_point(&L, 0, 0, 0), bot = layout_glyph_point(&L, 0, 0, 1);
  CHECK_NEAR(top.x, 64 + 0.08 * 42, 1e-3); CHECK_NEAR(top.y, 84, 1e-3);   // tops lean right
  CHECK_NEAR(bot.x, 64 - 0.08 * 42, 1e-3); CHECK_NEAR(bot.y, 134, 1e-3);
}

int main(void) {
  test_harness_self();
  test_geom_sqrt_trig();
  test_geom_sample_spacing();
  test_mulberry_matches_prototype();
  test_glyph_data();
  test_time_slots();
  test_layout_emery();
  test_layout_basalt();
  test_layout_glyph_point();
  if (g_fail) { fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
  printf("all host tests passed\n");
  return 0;
}
