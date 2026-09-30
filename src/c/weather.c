#include "weather.h"

static const WxDef DEFS[WX_COUNT] = {
  [WX_CLEAR]    = { 0, false, WXSKY_CLEAR,    0,   PRECIP_NONE, 0,  0, 0,   false },
  [WX_PARTLY]   = { 3, false, WXSKY_CLEAR,    0,   PRECIP_NONE, 0,  0, 0,   false },
  [WX_OVERCAST] = { 0, true,  WXSKY_OVERCAST, 0,   PRECIP_NONE, 0,  0, 0,   false },
  [WX_FOG]      = { 0, true,  WXSKY_OVERCAST, 218, PRECIP_NONE, 0,  0, 0,   false },
  [WX_DRIZZLE]  = { 0, true,  WXSKY_OVERCAST, 0,   PRECIP_RAIN, 32, 2, 110, false },
  [WX_RAIN]     = { 0, true,  WXSKY_OVERCAST, 0,   PRECIP_RAIN, 70, 4, 190, false },
  [WX_SNOW]     = { 0, true,  WXSKY_OVERCAST, 0,   PRECIP_SNOW, 95, 0, 26,  false },
  [WX_STORM]    = { 0, true,  WXSKY_STORM,    0,   PRECIP_RAIN, 90, 5, 230, true  },
};
static const Col8 OVC[4][3] = {
  [DAY_SUNRISE] = { C8(0xAAAAAA), C8(0xFFAAFF) }, [DAY_MIDDAY] = { C8(0xAAAAAA), C8(0xFFFFFF) },
  [DAY_SUNSET]  = { C8(0x555555), C8(0xAA5555), C8(0xFFAA55) }, [DAY_NIGHT] = { C8(0x000000), C8(0x000055) } };
static const Col8 STM[4][3] = {
  [DAY_SUNRISE] = { C8(0x555555), C8(0xAA55AA) }, [DAY_MIDDAY] = { C8(0x555555), C8(0xAAAAAA) },
  [DAY_SUNSET]  = { C8(0x000000), C8(0x550000), C8(0xAA5555) }, [DAY_NIGHT] = { C8(0x000000), C8(0x555555) } };
static const uint8_t SKY_N[4] = { 2, 2, 3, 2 };
static const Col8 STORM_CLOUD[4][3] = {
  [DAY_SUNRISE] = { C8(0xAA55AA), C8(0x550055), C8(0x550055) }, [DAY_MIDDAY] = { C8(0xAAAAAA), C8(0x555555), C8(0x555555) },
  [DAY_SUNSET]  = { C8(0xAA5555), C8(0x550000), C8(0x550000) }, [DAY_NIGHT]  = { C8(0x555555), C8(0x000055), C8(0x000055) } };
static const Col8 RAIN[4] = { C8(0xAA55AA), C8(0x5555AA), C8(0x550055), C8(0xAAAAFF) };
static const Col8 FOG[4]  = { C8(0xFFAAFF), C8(0xFFFFFF), C8(0xFFAAAA), C8(0x555555) };

const WxDef *wx_def(WxKind w) { return &DEFS[w < WX_COUNT ? w : WX_CLEAR]; }
bool wx_moves(WxKind w) { const WxDef *d = wx_def(w); return d->precip != PRECIP_NONE || d->lightning; }

WxKind wx_from_wmo(int c) {
  if (c == 0) return WX_CLEAR;
  if (c == 1 || c == 2) return WX_PARTLY;
  if (c == 3) return WX_OVERCAST;
  if (c == 45 || c == 48) return WX_FOG;
  if (c >= 51 && c <= 57) return WX_DRIZZLE;
  if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return WX_RAIN;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return WX_SNOW;
  if (c >= 95 && c <= 99) return WX_STORM;
  return WX_CLEAR;
}

uint8_t wx_sky(Daypart d, WxKind w, Col8 out[4]) {
  const WxDef *df = wx_def(w);
  if (df->sky == WXSKY_CLEAR) { const Theme *t = theme_for(d); for (uint8_t i = 0; i < t->nsky; i++) out[i] = t->sky[i]; return t->nsky; }
  const Col8 *src = df->sky == WXSKY_STORM ? STM[d] : OVC[d];
  for (uint8_t i = 0; i < SKY_N[d]; i++) out[i] = src[i];
  return SKY_N[d];
}
const Col8 *wx_cloud_roles(Daypart d, WxKind w) { return wx_def(w)->sky == WXSKY_STORM ? STORM_CLOUD[d] : theme_for(d)->cloud; }
Col8 wx_rain_colour(Daypart d) { return RAIN[d]; }
Col8 wx_fog_colour(Daypart d)  { return FOG[d]; }

static void add(Col8 *out, uint8_t *n, Col8 c) { for (uint8_t i = 0; i < *n; i++) if (out[i] == c) return; out[(*n)++] = c; }
uint8_t wx_bg_palette(Daypart d, WxKind w, Col8 out[32]) {
  const Theme *t = theme_for(d); const WxDef *df = wx_def(w); uint8_t n = 0; Col8 s[4];
  uint8_t ns = wx_sky(d, w, s);
  for (uint8_t i = 0; i < ns; i++) add(out, &n, s[i]);
  if (!df->deck) {
    if (t->body.kind == BODY_SUN) { add(out, &n, t->body.core); add(out, &n, t->body.glow); }
    else { add(out, &n, C8(0xFFFFFF)); add(out, &n, C8(0xAAAAAA)); add(out, &n, C8(0x000055)); }
    if (t->stars) { add(out, &n, C8(0xFFFFFF)); add(out, &n, C8(0xAAFFFF)); add(out, &n, C8(0xFFFFAA)); add(out, &n, C8(0x5555AA)); }
  }
  if (df->clouds || df->deck) { const Col8 *c = wx_cloud_roles(d, w); for (int i = 0; i < 3; i++) add(out, &n, c[i]); }
  if (df->fog256) add(out, &n, FOG[d]);
  for (int i = 0; i < 4; i++) { add(out, &n, t->far[i]); add(out, &n, t->near_[i]); }
  add(out, &n, t->forest); add(out, &n, t->pine_lit); add(out, &n, t->pine_shade); add(out, &n, t->pine_snow);
  add(out, &n, C8(0x550000)); add(out, &n, t->snow); add(out, &n, t->snow_shade);
  if (t->night) { add(out, &n, C8(0xFFFFAA)); add(out, &n, C8(0xFFFFFF)); add(out, &n, C8(0xFFFF55)); add(out, &n, C8(0x000055)); }
  return n;
}

static uint32_t mix(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static float hsh(int i, uint32_t salt) { return (mix((uint32_t)i * 0x9E3779B9u ^ salt) >> 8) / 16777216.0f; }
static float fmodp(float a, float m) { float q = (float)(int)(a / m); float r = a - q * m; return r < 0 ? r + m : r; }

int wx_drop_count(const WxDef *d, bool still) {
  if (d->precip == PRECIP_RAIN) return still ? (d->len < 3 ? d->rate * 2 : d->rate * 17 / 10) : d->rate;
  if (d->precip == PRECIP_SNOW) return still ? d->rate * 3 / 2 : d->rate;
  return 0;
}

bool wx_rain_drop(const Layout *L, const WxDef *d, int i, float t, bool still, int8_t wind, Vec2 *a, Vec2 *b) {
  if (still) t = 2.3f;
  const float k = L->k, top = L->comp_top - 2, span = top + 20, slant = 0.3f * wind, len = d->len * k;
  const float x0 = hsh(i, 0xA1u) * (L->W + 40) - (wind > 0 ? 40 : wind == 0 ? 20 : 0);
  const float y = fmodp(hsh(i, 0xB7u) * span + t * d->speed * k, span) - 10;
  if (y + len > top) return false;
  const float x = x0 + (y + 10) * slant;
  *a = (Vec2){ x, y }; *b = (Vec2){ x + len * slant, y + len };
  return true;
}

bool wx_flake(const Layout *L, const WxDef *d, int i, float t, bool still, int8_t wind, int16_t *x, int16_t *y, bool *big) {
  if (still) t = 2.3f;
  const float k = L->k, top = L->comp_top - 2;
  const float sp = d->speed * k * (0.6f + hsh(i, 0xD4u) * 0.8f);
  const float yy = fmodp(hsh(i, 0xC3u) * top + t * sp, top);
  if (yy > top - 3) return false;
  const float xx = fmodp(hsh(i, 0xB2u) * L->W + geom_sin(t * 1.3f + i) * 3 * k + 0.3f * wind * yy, (float)L->W);
  *x = (int16_t)xx; *y = (int16_t)yy; *big = (i % 3) == 0;
  return true;
}
