#include "geom.h"
#include "trig.h"

float geom_sqrt(float v) {
  if (v <= 0) return 0;
  union { float f; uint32_t i; } u = { v };
  u.i = 0x1fbd1df5u + (u.i >> 1);
  float r = u.f;
  for (int i = 0; i < 4; i++) r = 0.5f * (r + v / r);
  return r;
}

static int32_t rad_to_angle(float rad) {
  return (int32_t)(rad * (TRIG_MAX_ANGLE / 6.28318531f)) & (TRIG_MAX_ANGLE - 1);
}
float geom_sin(float rad) { return (float)sin_lookup(rad_to_angle(rad)) / TRIG_MAX_RATIO; }
float geom_cos(float rad) { return (float)cos_lookup(rad_to_angle(rad)) / TRIG_MAX_RATIO; }

Vec2 geom_norm(float x, float y) {
  float l = geom_sqrt(x * x + y * y);
  if (l == 0) l = 1;
  return (Vec2){ x / l, y / l };
}

static float cubic1(float a, float b, float c, float d, float t) {
  float m = 1 - t;
  return m * m * m * a + 3 * m * m * t * b + 3 * m * t * t * c + t * t * t * d;
}

int geom_sample_chain(const float *ch, int n, GeomMap map, const void *ctx, float step, Vec2 *out, int max) {
  if (max < 1 || n < 8) return 0;
  int cnt = 0;
  Vec2 prev = map(ch[0], ch[1], ctx);
  out[cnt++] = prev;
  float need = step, x0 = ch[0], y0 = ch[1];
  for (int i = 2; i + 5 < n; i += 6) {
    for (int s = 1; s <= GEOM_STEPS; s++) {
      float t = (float)s / GEOM_STEPS;
      Vec2 cur = map(cubic1(x0, ch[i], ch[i + 2], ch[i + 4], t), cubic1(y0, ch[i + 1], ch[i + 3], ch[i + 5], t), ctx);
      float ax = prev.x, ay = prev.y, dx = cur.x - ax, dy = cur.y - ay;
      float seg = geom_sqrt(dx * dx + dy * dy);
      while (seg >= need) {
        float r = need / seg;
        ax += (cur.x - ax) * r; ay += (cur.y - ay) * r;
        if (cnt < max) out[cnt++] = (Vec2){ ax, ay };
        seg -= need; need = step;
      }
      need -= seg;
      prev = cur;
    }
    x0 = ch[i + 4]; y0 = ch[i + 5];
  }
  if (cnt < max) out[cnt++] = prev;
  return cnt;
}

Vec2 geom_tangent(const Vec2 *p, int n, int i, int w) {
  int a = i - w < 0 ? 0 : i - w, b = i + w > n - 1 ? n - 1 : i + w;
  return geom_norm(p[b].x - p[a].x, p[b].y - p[a].y);
}

float mulberry_next(Mulberry *m) {
  m->a += 0x6D2B79F5u;
  uint32_t a = m->a;
  uint32_t t = (a ^ (a >> 15)) * (a | 1u);
  t = (t + (t ^ (t >> 7)) * (t | 61u)) ^ t;
  return (float)(t ^ (t >> 14)) / 4294967296.0f;
}
