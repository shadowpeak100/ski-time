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
  CHECK_EQ_INT(L.boxes[0].x, 64);  CHECK_EQ_INT(L.boxes[0].y, 61);
  CHECK_EQ_INT(L.boxes[1].x, 113); CHECK_EQ_INT(L.boxes[1].y, 61);
  CHECK_EQ_INT(L.boxes[2].x, 50);  CHECK_EQ_INT(L.boxes[2].y, 117);
  CHECK_EQ_INT(L.boxes[3].x, 99);  CHECK_EQ_INT(L.boxes[3].y, 117);
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
  CHECK_EQ_INT(L.boxes[0].x, 46); CHECK_EQ_INT(L.boxes[0].y, 46);
  CHECK_EQ_INT(L.boxes[1].x, 81); CHECK_EQ_INT(L.boxes[1].y, 46); CHECK_EQ_INT(L.boxes[2].x, 36);
  CHECK_EQ_INT(L.boxes[2].y, 87); CHECK_EQ_INT(L.boxes[3].y, 87); CHECK_EQ_INT(L.boxes[3].x, 71);
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
  CHECK_NEAR(top.x, 64 + 0.08 * 42, 1e-3); CHECK_NEAR(top.y, 61, 1e-3);   // tops lean right
  CHECK_NEAR(bot.x, 64 - 0.08 * 42, 1e-3); CHECK_NEAR(bot.y, 111, 1e-3);
}

#include "skier.h"

static SkPt g_pool[2000];

static void build_time(SkierSet *set, const Layout *L, int h, int m, bool use24, bool lead0) {
  DigitSlot s[4]; uint8_t n = time_slots(h, m, use24, lead0, s);
  skiers_init(set, g_pool, SKI_POOL_CAP(L->W));
  skiers_build(set, L, s, n);
}

static void check_set(const SkierSet *set, const Layout *L) {
  CHECK(!set->truncated);
  CHECK(set->pool_used <= set->pool_cap);
  uint16_t d = skiers_duration(set);
  CHECK(d >= 3500 && d <= 6000);
  for (uint8_t i = 0; i < set->count; i++) {
    const Skier *s = &set->sk[i];
    CHECK(s->n > s->s_idx && s->s_idx > 0);
    for (uint16_t j = 1; j < s->n; j++) CHECK(s->pts[j].t >= s->pts[j - 1].t);
    CHECK(!skier_state(s, s->t0 - 1).visible);
    SkierState r = skier_state(s, s->t_end + SKI_STOP_MS);
    CHECK(r.visible && r.spray == 0);
    CHECK_NEAR(r.x / 16.0, s->rest_pos.x, 0.2);
    CHECK_NEAR(r.y / 16.0, s->rest_pos.y, 0.2);
    CHECK(!skier_exit_state(s, i, 1500, L->k).visible);         // everyone gone 1.5 s in
    SkierState e0 = skier_exit_state(s, i, 0, L->k);
    CHECK(e0.visible && e0.h == s->rest_h);
  }
}

static void test_skiers_2046(void) {
  Layout L; layout_init(&L, 200, 228);
  SkierSet set; build_time(&set, &L, 20, 46, true, true);
  CHECK_EQ_INT(set.count, 6);              // 2 (one) + 0 (two) + 4 (two) + 6 (one)
  check_set(&set, &L);
  // The first skier enters from off the right edge and its stroke starts on digit slot 0.
  CHECK(set.sk[0].pts[0].x / 16 >= L.W);
  CHECK(set.sk[0].t0 == SKI_DRAW_START_MS);
  CHECK(set.sk[2].t0 == SKI_DRAW_START_MS + SKI_STAGGER_MS + SKI_PAIR_MS);   // 0's second skier
}

static void test_skiers_all_minutes(void) {
  const int sizes[2][2] = { { 200, 228 }, { 144, 168 } };
  for (int p = 0; p < 2; p++) {
    Layout L; layout_init(&L, sizes[p][0], sizes[p][1]);
    for (int f = 0; f < 4; f++) for (int h = 0; h < 24; h++) for (int m = 0; m < 60; m++) {   // every 12/24 h, lead-zero combo
      SkierSet set; build_time(&set, &L, h, m, f & 1, f & 2);
      CHECK(set.count >= ((f & 2) ? 4 : 3) && set.count <= SKI_MAX);   // no lead zero: 3 digits before 10:00
      check_set(&set, &L);
    }
  }
}

static void check_exit_stays_above_strip(int W, int H) {
  Layout L; layout_init(&L, W, H);
  CHECK_NEAR(L.exit_ceiling, L.comp_top - 8, 1e-4);
  for (int f = 0; f < 4; f++) for (int h = 0; h < 24; h++) for (int m = 0; m < 60; m++) {
    SkierSet set; build_time(&set, &L, h, m, f & 1, f & 2);
    for (uint8_t i = 0; i < set.count; i++) {
      const Skier *s = &set.sk[i];
      float last_x = s->rest_pos.x;
      for (uint16_t t = 0; t <= 1500; t += 20) {
        SkierState st = skier_exit_state(s, i, t, L.k);
        if (!st.visible) continue;
        CHECK(st.y / 16.0 <= L.exit_ceiling + 0.01);
        last_x = st.x / 16.0f;
      }
      CHECK(last_x < s->rest_pos.x);
    }
  }
}

static void test_exit_stays_above_strip(void) {
  check_exit_stays_above_strip(200, 228);   // emery
  check_exit_stays_above_strip(144, 168);   // basalt
}

static void test_skiers_digit_count_change(void) {
  Layout L; layout_init(&L, 200, 228);
  SkierSet a; build_time(&a, &L, 12, 59, false, false);   // "12:59"
  uint8_t na = a.count;
  check_set(&a, &L);                                       // before b reuses the shared pool
  SkierSet b; build_time(&b, &L, 13, 0, false, false);    // "1:00" (one hour digit)
  CHECK_EQ_INT(na, 4);                                     // "12:59": one skier per digit
  CHECK_EQ_INT(b.count, 5);                                // "1:00": 1 (one) + 0 (two) + 0 (two)
  check_set(&b, &L);
}

static void test_skier_mid_stroke(void) {
  Layout L; layout_init(&L, 200, 228);
  SkierSet set; build_time(&set, &L, 11, 11, true, true);
  const Skier *s = &set.sk[0];
  uint16_t mid = s->pts[s->s_idx + (s->n - s->s_idx) / 2].t;
  SkierState r = skier_state(s, mid);
  CHECK(r.visible && r.idx > s->s_idx && r.idx < s->n - 1);
}

#include "avalanche.h"
#include "saver.h"

static void test_aval_timing(void) {
  Layout L; layout_init(&L, 200, 228);
  AvalFrame A;
  aval_frame(&A, &L, AV_FLOW_START_MS - 1); CHECK(!A.active);
  aval_frame(&A, &L, AV_FLOW_END_MS + AV_FADE_MS + 1); CHECK(!A.active);
  aval_frame(&A, &L, 1000); CHECK(A.active);
  CHECK(A.a > 0 && A.a < 1);
}

static void test_aval_front_advances(void) {
  Layout L; layout_init(&L, 200, 228);
  AvalFrame A; int16_t prev[AV_MAX_S];
  aval_frame(&A, &L, AV_FLOW_START_MS);
  memcpy(prev, A.front5, sizeof prev);
  for (uint16_t t = AV_FLOW_START_MS + 100; t <= AV_FLOW_END_MS; t += 100) {
    aval_frame(&A, &L, t);
    for (int s = 0; s < 297; s += 7) CHECK(A.front5[s] >= prev[s]);
    memcpy(prev, A.front5, sizeof prev);
  }
}

static void test_aval_pixels(void) {
  Layout L; layout_init(&L, 200, 228);
  uint8_t st[AV_MAX_S]; aval_streaks(st);
  AvalFrame A;
  aval_frame(&A, &L, 900);                                 // front still near the top right
  CHECK_EQ_INT(aval_pixel(&A, st, L.k, 5, 190, 900), AV_NONE);   // bottom left untouched
  aval_frame(&A, &L, AV_FLOW_END_MS);                      // front at the bottom left
  AvPix p = aval_pixel(&A, st, L.k, 170, 60, AV_FLOW_END_MS);    // top right, long passed
  CHECK(p == AV_RESTORE || p == AV_CELESTE);
  aval_frame(&A, &L, AV_FLOW_START_MS);                    // grow == 0: no cast shadow yet
  for (int x = 0; x < 200; x += 3) for (int y = 10; y < 190; y += 3)
    CHECK(aval_pixel(&A, st, L.k, x, y, AV_FLOW_START_MS) != AV_SHADOW);
}

static void test_saver_window(void) {
  CHECK(!saver_active(false, 1320, 420, 23, 0));
  CHECK(saver_active(true, 1320, 420, 23, 0));             // wraps midnight
  CHECK(saver_active(true, 1320, 420, 6, 59));
  CHECK(!saver_active(true, 1320, 420, 7, 0));
  CHECK(!saver_active(true, 1320, 420, 12, 0));
  CHECK(saver_active(true, 60, 120, 1, 30));               // same-day window
  CHECK(!saver_active(true, 60, 120, 2, 0));
  CHECK(!saver_active(true, 300, 300, 5, 0));              // empty window
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
  test_skiers_2046();
  test_skiers_all_minutes();
  test_exit_stays_above_strip();
  test_skiers_digit_count_change();
  test_skier_mid_stroke();
  test_aval_timing();
  test_aval_front_advances();
  test_aval_pixels();
  test_saver_window();
  if (g_fail) { fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
  printf("all host tests passed\n");
  return 0;
}
