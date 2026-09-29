#include "avalanche.h"
#include "trig.h"

// Integer maths throughout the per-pixel path: u5 = 5u = 4y - 3x, s = (4x + 3y) / 5.
static const uint8_t BAYER[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
uint8_t aval_bayer(int x, int y) { return BAYER[((y & 3) << 2) | (x & 3)]; }
static bool dith(int x, int y, int32_t p256) { return (int32_t)aval_bayer(x, y) * 16 + 8 < p256; }

float aval_noise(float x) {
  float v = 0.5f + 0.3f * geom_sin(x) + 0.2f * geom_sin(x * 2.3f + 1.7f);
  return v < 0 ? 0 : v > 1 ? 1 : v;
}

void aval_streaks(uint8_t st[AV_MAX_S]) {
  for (int s = 0; s < AV_MAX_S; s++) {
    float v = 0.5f + 0.5f * geom_sin(s * 0.55f + 3 * aval_noise(s * 0.1f));
    float p = v * v; p *= p; p *= p;
    st[s] = (uint8_t)(p * 255);
  }
}

void aval_frame(AvalFrame *A, const Layout *L, uint16_t t) {
  A->active = t >= AV_FLOW_START_MS && t <= AV_FLOW_END_MS + AV_FADE_MS;
  if (!A->active) return;
  float k = L->k, a = (t - AV_FLOW_START_MS) / (float)(AV_FLOW_END_MS - AV_FLOW_START_MS);
  if (a > 1) a = 1;
  float p = a * geom_sqrt(geom_sqrt(a));                               // a^1.25
  A->a = a;
  A->uf = L->u0 + (L->u1 - L->u0) * p;
  A->fade = t > AV_FLOW_END_MS ? 1 - (t - AV_FLOW_END_MS) / (float)AV_FADE_MS : 1;
  A->grow = (a * 5 < 1 ? a * 5 : 1) * A->fade;
  float amp = a * 2.5f < 1 ? a * 2.5f : 1, tau = t / 1000.0f;
  for (int s = 0; s < AV_MAX_S; s++) {
    float f = A->uf + amp * k * (14 * (aval_noise(s * 0.045f + 1.3f) - 0.5f) + 6 * (aval_noise(s * 0.13f + tau * 1.7f) - 0.5f));
    A->front5[s] = (int16_t)(f * 5);
  }
  A->body5 = (int16_t)((26 * k * A->grow + 1) * 5);
}

float aval_front(const AvalFrame *A, float s) {
  int i = (int)(s + 0.5f);
  if (i < 0) i = 0;
  if (i >= AV_MAX_S) i = AV_MAX_S - 1;
  return A->front5[i] / 5.0f;
}

// Churned-snow texture, Q8 (0..255). Integer port of the prototype's n2(0.3s, 0.3u - 9tau).
static int32_t churn(int32_t s, int32_t u5, int32_t t) {
  int32_t a1 = s * 4068 + 20861 * sin_lookup((438 * u5 - 66 * t) & 0xFFFF) / TRIG_MAX_RATIO;
  int32_t a2 = 688 * u5 - 103 * t + 15646 * cos_lookup((2816 * s) & 0xFFFF) / TRIG_MAX_RATIO;
  int32_t c = 128 + 72 * sin_lookup(a1 & 0xFFFF) / TRIG_MAX_RATIO + 56 * sin_lookup(a2 & 0xFFFF) / TRIG_MAX_RATIO;
  return c < 0 ? 0 : c > 255 ? 255 : c;
}

AvPix aval_pixel(const AvalFrame *A, const uint8_t *streak, float k, int x, int y, uint16_t t) {
  int32_t u5 = 4 * y - 3 * x, s = (4 * x + 3 * y) / 5;
  if (s < 0) s = 0;
  if (s >= AV_MAX_S) s = AV_MAX_S - 1;
  int32_t dep = A->front5[s] - u5;
  int32_t sh0 = (int32_t)(-45 * k), sh1 = (int32_t)(-10 * k);
  if (dep < sh0) return AV_NONE;
  if (dep < 0) {
    if (dep < sh1) {
      int32_t p = 128 * (dep - sh0) / (sh1 - sh0) * (int32_t)(A->grow * 256) / 256;
      if (dith(x, y, p)) return AV_SHADOW;
    }
    return AV_NONE;
  }
  int32_t body = A->body5;
  if (dep < body) {
    int32_t t256 = dep * 256 / body, c = churn(s, u5, t);
    if (c > 220 && t256 < 154) return AV_BLUE;
    int32_t p = (c - 102) * 461 / 256 * (256 - t256) / 256;
    return dith(x, y, p) ? AV_CELESTE : AV_WHITE;
  }
  int32_t wake = (int32_t)(200 * k);
  if (dep < body + wake) {
    int32_t w256 = (256 - (dep - body) * 256 / wake) * (int32_t)(A->fade * 256) / 256;
    int32_t p = streak[s] * w256 / 256 * 205 / 256;
    if (dith(x, y, p)) return AV_CELESTE;
  }
  return AV_RESTORE;
}
