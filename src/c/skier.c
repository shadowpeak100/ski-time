#include "skier.h"
#include "trig.h"
#include <stdlib.h>

static Vec2 s_rest;   // hockey-stop ski direction, norm(0.92, 0.39)

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static float smoothf(float t) { return t * t * (3 - 2 * t); }
static int16_t q4(float v) { return (int16_t)(v * 16 + (v >= 0 ? 0.5f : -0.5f)); }
static int32_t angle_of(Vec2 unit) { return atan2_lookup((int16_t)(unit.y * 1024), (int16_t)(unit.x * 1024)); }
static Vec2 pv(const SkPt *p) { return (Vec2){ p->x / 16.0f, p->y / 16.0f }; }
static Vec2 tangent_pts(const SkPt *p, int n, int i, int w) {
  int a = i - w < 0 ? 0 : i - w, b = i + w > n - 1 ? n - 1 : i + w;
  return geom_norm((p[b].x - p[a].x) / 16.0f, (p[b].y - p[a].y) / 16.0f);
}

typedef struct { const Layout *L; uint8_t slot; } BoxCtx;
static Vec2 box_map(float u, float v, const void *c) {
  const BoxCtx *b = c;
  return layout_glyph_point(b->L, b->slot, u, v);
}
static Vec2 id_map(float u, float v, const void *c) { (void)c; return (Vec2){ u, v }; }

static Vec2 cub(const Vec2 *c, float t) {
  float m = 1 - t, a = m * m * m, b = 3 * m * m * t, d = 3 * m * t * t, e = t * t * t;
  return (Vec2){ a * c[0].x + b * c[1].x + d * c[2].x + e * c[3].x, a * c[0].y + b * c[1].y + d * c[2].y + e * c[3].y };
}
static Vec2 cub_d(const Vec2 *c, float t) {
  float m = 1 - t;
  return (Vec2){ 3 * m * m * (c[1].x - c[0].x) + 6 * m * t * (c[2].x - c[1].x) + 3 * t * t * (c[3].x - c[2].x),
                 3 * m * m * (c[1].y - c[0].y) + 6 * m * t * (c[2].y - c[1].y) + 3 * t * t * (c[3].y - c[2].y) };
}

static void build_exit(Skier *sk, const Layout *L) {
  float k = L->k;
  Vec2 P = sk->rest_pos, X = { -14, P.y + (P.x + 14) * 0.75f };
  if (X.y > L->H + 14) X = (Vec2){ P.x - (L->H + 14 - P.y) / 0.75f, L->H + 14.0f };
  sk->ex[0] = P;
  sk->ex[1] = (Vec2){ P.x + FALL.x * 14 * k, P.y + FALL.y * 14 * k };
  sk->ex[2] = (Vec2){ X.x - FALL.x * 22 * k, X.y - FALL.y * 22 * k };
  sk->ex[3] = X;
  sk->ex_len[0] = 0;
  Vec2 prev = P;
  for (int i = 1; i <= SKI_EXIT_SEGS; i++) {
    Vec2 q = cub(sk->ex, (float)i / SKI_EXIT_SEGS);
    float dx = q.x - prev.x, dy = q.y - prev.y;
    sk->ex_len[i] = sk->ex_len[i - 1] + geom_sqrt(dx * dx + dy * dy);
    prev = q;
  }
}

static bool build_one(SkierSet *set, const Layout *L, Vec2 *st, int cap_pts, Skier *sk, GlyphStroke g, uint8_t slot, uint16_t t0, uint8_t color) {
  const float k = L->k;
  BoxCtx bc = { L, slot };
  int ns = geom_sample_chain(g.pts, g.n, box_map, &bc, 1.0f, st, cap_pts);
  if (ns >= cap_pts) set->truncated = true;
  if (ns < 2) return false;
  Vec2 start = st[0], d0 = geom_tangent(st, ns, 0, 3);
  Vec2 P0 = { L->W + 10.0f, clampf(start.y - (L->W + 10 - start.x) * 0.45f, layout_ridge_y(L, L->W) + 8 * k, L->comp_top - 10.0f) };
  float dx = start.x - P0.x, dy = start.y - P0.y, len = geom_sqrt(dx * dx + dy * dy);
  Vec2 a = geom_norm(-1, 0.45f);
  const float entry[8] = { P0.x, P0.y, P0.x + a.x * len * 0.35f, P0.y + a.y * len * 0.35f,
                           start.x - d0.x * len * 0.4f, start.y - d0.y * len * 0.4f, start.x, start.y };
  int ne = geom_sample_chain(entry, 8, id_map, NULL, 1.0f, st + ns, cap_pts - ns);
  if (ns + ne >= cap_pts) set->truncated = true;

  SkPt *p = set->pool + set->pool_used;
  int cap = set->pool_cap - set->pool_used, n = 0;
  for (int i = 0; i < ne - 1 && n < cap; i++) p[n++] = (SkPt){ q4(st[ns + i].x), q4(st[ns + i].y), 0 };
  uint16_t s_idx = (uint16_t)n;
  for (int i = 0; i < ns && n < cap; i++) p[n++] = (SkPt){ q4(st[i].x), q4(st[i].y), 0 };
  if (n < ne - 1 + ns) set->truncated = true;
  if (n <= s_idx + 1) return false;

  float t = t0;
  p[0].t = t0;
  for (int i = 1; i < n; i++) {
    Vec2 a0 = pv(&p[i - 1]), a1 = pv(&p[i]);
    float ddx = a1.x - a0.x, ddy = a1.y - a0.y, ds = geom_sqrt(ddx * ddx + ddy * ddy);
    Vec2 tg = tangent_pts(p, n, i, 2);
    float down = tg.x * FALL.x + tg.y * FALL.y;
    float v = 78 * k * (0.72f + 0.55f * (down > 0 ? down : 0)) * (1 - 0.35f * (tg.y < 0 ? -tg.y : 0));
    if (i < s_idx) {
      float e = smoothf(clampf((s_idx - i) / (30 * k), 0, 1));
      v = v + (170 * k - v) * e;
    }
    t += ds / v * 1000;
    p[i].t = (uint16_t)(t + 0.5f);
  }

  sk->pts = p; sk->n = (uint16_t)n; sk->s_idx = s_idx; sk->t0 = t0; sk->t_end = p[n - 1].t; sk->color = color;
  sk->end = pv(&p[n - 1]);
  sk->d_end = tangent_pts(p, n, n - 1, 3);
  sk->end_h = angle_of(sk->d_end);
  Vec2 rd = (sk->d_end.x * s_rest.x + sk->d_end.y * s_rest.y) >= 0 ? s_rest : (Vec2){ -s_rest.x, -s_rest.y };
  sk->rest_h = sk->end_h + (int16_t)(angle_of(rd) - sk->end_h);
  sk->stop_len = 4 * k;
  sk->rest_pos = (Vec2){ sk->end.x + sk->d_end.x * sk->stop_len, sk->end.y + sk->d_end.y * sk->stop_len };
  build_exit(sk, L);
  set->pool_used += (uint16_t)n;
  return true;
}

void skiers_init(SkierSet *set, SkPt *pool, uint16_t cap) {
  set->pool = pool; set->pool_cap = cap; set->pool_used = 0; set->count = 0; set->truncated = false;
  if (s_rest.x == 0) s_rest = geom_norm(0.92f, 0.39f);
}

void skiers_build(SkierSet *set, const Layout *L, const DigitSlot *slots, uint8_t nslots) {
  set->count = 0; set->pool_used = 0; set->truncated = false;
  const int cap_pts = SKI_SCRATCH_FOR(L->W);
  Vec2 *st = malloc(cap_pts * sizeof(Vec2));   // held only while building, to keep it off the resting heap
  if (!st) { set->truncated = true; return; }
  for (uint8_t i = 0; i < nslots; i++) {
    uint8_t d = slots[i].digit, slot = slots[i].slot;
    for (uint8_t j = 0; j < glyph_stroke_count(d) && set->count < SKI_MAX; j++) {
      uint16_t t0 = SKI_DRAW_START_MS + slot * SKI_STAGGER_MS + j * SKI_PAIR_MS;
      if (build_one(set, L, st, cap_pts, &set->sk[set->count], glyph_stroke(d, j), slot, t0, (uint8_t)((slot * 2 + j) % 8)))
        set->count++;
    }
  }
  free(st);
}

int32_t skier_pt_heading(const Skier *s, uint16_t i) {
  int a = i < 2 ? 0 : i - 2, b = i + 2 > s->n - 1 ? s->n - 1 : i + 2;
  return atan2_lookup(s->pts[b].y - s->pts[a].y, s->pts[b].x - s->pts[a].x);
}

SkierState skier_state(const Skier *s, uint16_t t) {
  SkierState r = { 0 };
  if (s->n < 2 || t < s->t0) return r;
  r.visible = true;
  if (t <= s->t_end) {
    int lo = 0, hi = s->n - 1;
    while (hi - lo > 1) { int m = (lo + hi) / 2; if (s->pts[m].t <= t) lo = m; else hi = m; }
    const SkPt *a = &s->pts[lo], *b = &s->pts[hi];
    int32_t span = b->t - a->t, f = span > 0 ? (((int32_t)(t - a->t)) << 8) / span : 0;
    r.x = (int16_t)(a->x + (((b->x - a->x) * f) >> 8));
    r.y = (int16_t)(a->y + (((b->y - a->y) * f) >> 8));
    int32_t h0 = skier_pt_heading(s, lo), h1 = skier_pt_heading(s, hi);
    r.h = h0 + ((((int16_t)(h1 - h0)) * f) >> 8);
    r.idx = (uint16_t)lo;
    return r;
  }
  float u = (t - s->t_end) / (float)SKI_STOP_MS;
  if (u > 1) u = 1;
  float e = 1 - (1 - u) * (1 - u), d = s->stop_len * e;
  r.x = q4(s->end.x + s->d_end.x * d);
  r.y = q4(s->end.y + s->d_end.y * d);
  r.h = s->end_h + (int32_t)((s->rest_h - s->end_h) * smoothf(u));
  r.idx = s->n - 1;
  r.spray = u < 1 ? (uint8_t)(1 + u * 254) : 0;
  return r;
}

SkierState skier_exit_state(const Skier *s, uint8_t order, uint16_t t, float k) {
  SkierState r = { .visible = true, .idx = s->n - 1 };
  int32_t te_ms = (int32_t)t - order * SKI_EXIT_GAP_MS;
  if (te_ms <= 0) { r.x = q4(s->rest_pos.x); r.y = q4(s->rest_pos.y); r.h = s->rest_h; return r; }
  float te = te_ms / 1000.0f, dist = 0.5f * 380 * k * te * te + 20 * k * te;
  if (dist >= s->ex_len[SKI_EXIT_SEGS]) { r.visible = false; return r; }
  int i = 0;
  while (s->ex_len[i + 1] < dist) i++;
  float f = (dist - s->ex_len[i]) / (s->ex_len[i + 1] - s->ex_len[i]);
  float tt = (i + f) / SKI_EXIT_SEGS;
  Vec2 q = cub(s->ex, tt), dv = cub_d(s->ex, tt);
  Vec2 du = geom_norm(dv.x, dv.y);
  int32_t hp = s->rest_h + (int16_t)(angle_of(du) - s->rest_h);
  float w = smoothf(clampf(te / 0.35f, 0, 1));
  r.x = q4(q.x); r.y = q4(q.y);
  r.h = s->rest_h + (int32_t)((hp - s->rest_h) * w);
  return r;
}

uint16_t skiers_duration(const SkierSet *set) {
  uint16_t m = 0;
  for (uint8_t i = 0; i < set->count; i++) if (set->sk[i].t_end > m) m = set->sk[i].t_end;
  return m + SKI_STOP_MS + 300;
}
