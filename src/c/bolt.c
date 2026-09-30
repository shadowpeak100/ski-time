#include <string.h>
#include "bolt.h"

// Midpoint displacement in place; pts[0] and pts[1] hold the endpoints. Returns the point count.
static uint8_t displace(Mulberry *m, Vec2 *pts, int depth, float rough) {
  int n = 2;
  for (int d = 0; d < depth; d++) {
    const float amp = rough / (float)(1 << d);
    for (int i = n - 1; i >= 1; i--) pts[2 * i] = pts[i];
    n = 2 * n - 1;
    for (int j = 1; j < n; j += 2) {
      const Vec2 a = pts[j - 1], c = pts[j + 1];
      const float rx = mulberry_next(m);
      const float ry = mulberry_next(m);
      pts[j].x = (a.x + c.x) / 2 + (rx - 0.5f) * 2 * amp;
      pts[j].y = (a.y + c.y) / 2 + (ry - 0.5f) * amp * 0.5f;
    }
  }
  return (uint8_t)n;
}

void bolt_build(Bolt *b, const Layout *L) {
  static const float BRANCH_AT[BOLT_BRANCHES] = { 0.28f, 0.45f, 0.62f };
  Mulberry m = { 307 };
  memset(b, 0, sizeof *b);   // unused tail slots stay zero so builds compare equal
  const float k = L->k, x1 = 0.1f * L->W;
  b->main[0] = (Vec2){ 0.2f * L->W, 6 * k };
  b->main[1] = (Vec2){ x1, layout_ridge_y(L, x1) - 2 };
  b->n_main = displace(&m, b->main, 5, 9 * k);
  for (int j = 0; j < BOLT_BRANCHES; j++) {
    const Vec2 s = b->main[(int)(b->n_main * BRANCH_AT[j])];
    const float side = j % 2 ? 1.0f : -1.0f;
    const float len = (12 + mulberry_next(&m) * 12) * k;
    const float ex = s.x + side * len * 0.7f, ridge = layout_ridge_y(L, ex) - 3;
    b->br[j][0] = s;
    b->br[j][1] = (Vec2){ ex, s.y + len < ridge ? s.y + len : ridge };
    b->n_br[j] = displace(&m, b->br[j], 4, 4 * k);
  }
}

bool bolt_visible(uint32_t ms, bool still, bool *flash) {
  if (still) { *flash = false; return true; }
  const uint32_t c = ms % 5000;
  *flash = c < 60;
  return c < 120 || (c > 200 && c < 340);
}
