#include "theme.h"

static const Theme THEMES[4] = {
  [DAY_SUNRISE] = {
    .sky = { C8(0xAAAAFF), C8(0xFFAAFF), C8(0xFFFFAA) }, .nsky = 3,
    .body = { BODY_SUN, 0.10f, 0.25f, 8, 10, C8(0xFFFF55), C8(0xFFFFAA) },
    .stars = false, .night = false,
    .far = { C8(0xFFAAFF), C8(0xAA55AA), C8(0xFFFFAA), C8(0xFFAAFF) },
    .near_ = { C8(0xAA55AA), C8(0x550055), C8(0xFFAAAA), C8(0xFFAAFF) },
    .forest = C8(0x5555AA), .pine_lit = C8(0x005500), .pine_shade = C8(0x005555), .pine_snow = C8(0xFFAAFF),
    .snow = C8(0xFFFFFF), .snow_shade = C8(0xFFAAFF), .snow_shadow = C8(0xAAAAFF), .powder = C8(0xFFFFFF),
    .text = C8(0xAA55AA),
    .cloud = { C8(0xFFFFAA), C8(0xFFAAFF), C8(0xAA55AA) },
  },
  [DAY_MIDDAY] = {
    .sky = { C8(0x55AAFF), C8(0xAAFFFF) }, .nsky = 2,
    .body = { BODY_SUN, 0.13f, 0.08f, 7, 6, C8(0xFFFF55), C8(0xFFFFAA) },
    .stars = false, .night = false,
    .far = { C8(0xAAAAFF), C8(0x5555AA), C8(0xFFFFFF), C8(0xAAFFFF) },
    .near_ = { C8(0x0055AA), C8(0x000055), C8(0xFFFFFF), C8(0xAAAAFF) },
    .forest = C8(0x55AAAA), .pine_lit = C8(0x005500), .pine_shade = C8(0x005555), .pine_snow = C8(0xFFFFFF),
    .snow = C8(0xFFFFFF), .snow_shade = C8(0xAAFFFF), .snow_shadow = C8(0xAAAAFF), .powder = C8(0xFFFFFF),
    .text = C8(0x5555AA),
    .cloud = { C8(0xFFFFFF), C8(0xAAFFFF), C8(0xAAAAFF) },
  },
  [DAY_SUNSET] = {
    .sky = { C8(0xAA0055), C8(0xFF0000), C8(0xFF5500), C8(0xFFAA00) }, .nsky = 4,
    .body = { BODY_SUN, 0.30f, 0.20f, 10, 8, C8(0xFFFF55), C8(0xFFAA00) },
    .stars = false, .night = false,
    .far = { C8(0xFFAA55), C8(0xAA55AA), C8(0xFFFFAA), C8(0xFFAAAA) },
    .near_ = { C8(0xAA0055), C8(0x550055), C8(0xFFAA55), C8(0xFFAAAA) },
    .forest = C8(0x550055), .pine_lit = C8(0x005500), .pine_shade = C8(0x000000), .pine_snow = C8(0xFFAAAA),
    .snow = C8(0xFFFFFF), .snow_shade = C8(0xFFAAAA), .snow_shadow = C8(0xFFAAFF), .powder = C8(0xFFFFFF),
    .text = C8(0xAA0055),
    .cloud = { C8(0xFFAA55), C8(0xFF5500), C8(0xAA0055) },
  },
  [DAY_NIGHT] = {
    .sky = { C8(0x000000), C8(0x000000), C8(0x000055) }, .nsky = 3,
    .body = { BODY_MOON, 0.15f, 0.10f, 6, 0, 0, 0 },
    .stars = true, .night = true,
    .far = { C8(0x5555AA), C8(0x000055), C8(0xAAAAFF), C8(0x5555AA) },
    .near_ = { C8(0x000055), C8(0x000000), C8(0x5555AA), C8(0x000055) },
    .forest = C8(0x000055), .pine_lit = C8(0x005555), .pine_shade = C8(0x000000), .pine_snow = C8(0xAAAAFF),
    .snow = C8(0xAAAAFF), .snow_shade = C8(0x5555AA), .snow_shadow = C8(0x5555AA), .powder = C8(0xFFFFFF),
    .text = C8(0x000055),
    .cloud = { C8(0x5555AA), C8(0xAAAAFF), C8(0x000055) },
  },
};

const Theme *theme_for(Daypart d) { return &THEMES[(unsigned)d & 3]; }

static uint8_t add_distinct(Col8 *out, uint8_t n, Col8 c) {
  for (uint8_t i = 0; i < n; i++) if (out[i] == c) return n;
  out[n] = c;
  return n + 1;
}

uint8_t theme_bg_palette(const Theme *t, Col8 out[32]) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < t->nsky; i++) n = add_distinct(out, n, t->sky[i]);
  if (t->body.kind == BODY_SUN) {
    n = add_distinct(out, n, t->body.core);
    n = add_distinct(out, n, t->body.glow);
  } else {
    n = add_distinct(out, n, C8(0xFFFFFF));
    n = add_distinct(out, n, C8(0xAAAAAA));
    n = add_distinct(out, n, C8(0x000055));
  }
  if (t->stars) {
    n = add_distinct(out, n, C8(0xFFFFFF));
    n = add_distinct(out, n, C8(0xAAFFFF));
    n = add_distinct(out, n, C8(0xFFFFAA));
    n = add_distinct(out, n, C8(0x5555AA));
  }
  for (uint8_t i = 0; i < 4; i++) n = add_distinct(out, n, t->far[i]);
  for (uint8_t i = 0; i < 4; i++) n = add_distinct(out, n, t->near_[i]);
  n = add_distinct(out, n, t->forest);
  n = add_distinct(out, n, t->pine_lit);
  n = add_distinct(out, n, t->pine_shade);
  n = add_distinct(out, n, t->pine_snow);
  n = add_distinct(out, n, C8(0x550000));
  n = add_distinct(out, n, t->snow);
  n = add_distinct(out, n, t->snow_shade);
  if (t->night) {
    n = add_distinct(out, n, C8(0xFFFFAA));
    n = add_distinct(out, n, C8(0xFFFFFF));
    n = add_distinct(out, n, C8(0xFFFF55));
    n = add_distinct(out, n, C8(0x000055));
  }
  return n;
}

Daypart theme_daypart(const Theme *t) {
  for (int d = 0; d < 4; d++) if (theme_for((Daypart)d) == t) return (Daypart)d;
  return DAY_MIDDAY;
}
