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
