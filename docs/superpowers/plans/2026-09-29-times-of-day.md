# Ski Time Phase 2 (Times of Day) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The Ski Time face changes through sunrise, midday, sunset and night, following real sunrise and sunset times. The night scene gets a moon, stars, floodlights, glowing windows and a campfire. Nine pines stand on the open slope on Emery.

**Architecture:**
- **Pure, host-tested modules:**
  - `daypart`: phase timing and moon phase
  - `theme`: colour-role tables and the background palette budget
  - `flood`: floodlight geometry and intensity
  - `decor`: slope-tree data
- **Watch modules:** `scene` builds the background from the active theme. The avalanche, sprites, chalet and complications read their changing colours from `scene_theme()`.
- **Driver:** `main.c` picks the phase each minute and applies a change when the minute change starts.

**Tech Stack:** C99 Pebble SDK 3 (pebble tool v5), PebbleKit JS, host tests with `cc`.

**Spec:** `docs/superpowers/specs/2026-09-29-times-of-day-design.md`. The reference prototype is `docs/prototypes/times-of-day.html` (JS: `THEMES`, `buildBackground`, `floodHeads`, `floodAt`, `drawCampfire`, `SLOPE_TREES`, `phaseAt`, `moonPhase`).

## Global Constraints

- **Platforms:** basalt and emery. Emery is primary. Slope trees are Emery only (`W >= 200`). Everything else runs on both.
- **Pure modules** include only `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `trig.h`, `geom.h` and each other. No `pebble.h`, no `math.h`.
- **Colours** are Pebble GColor8 bytes, built with `C8(0xRRGGBB)` = `0xC0 | (R>>6)<<4 | (G>>6)<<2 | (B>>6)`. On the watch, convert with `(GColor){ .argb = c }`.
- **Background budget:** at most 16 distinct colours per theme, because the background is stored at 4 bpp. Draw it without antialiasing.
- **Phase windows:** sunrise `[sr−60, sr+60)`, sunset `[ss−60, ss+60)`, midday `[sr+60, ss−60)`, night otherwise. Everything wraps at midnight.
- **Defaults:** mode Follow, fixed times 420 (07:00) and 1140 (19:00), and phone times 420 and 1140 until received.
- **Commits:** every message ends with a blank line and exactly `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Output:** pipe `pebble build` through `tail -40`.
- **Emulator:** if it fails in the command sandbox, retry with the sandbox disabled. If it hangs, run `pebble kill` and `pebble wipe`, then reinstall.

## Review Focus

1. **Phase boundaries at midnight and odd sun times**, for example a sunset window that crosses midnight (sunset 23:30). The daypart test covers it.
2. **Phase switch during an animation.** The background rebuild must happen before the frame renders, never between the avalanche pixel pass and restore. The Task 5 check covers this.
3. **The 16-colour background budget per theme.** The Task 2 test covers it. If the watch's palette builder exceeds it, it logs once, as in phase 1.
4. **The settings struct grows, so persisted phase 1 settings reset to defaults on upgrade.** This is acceptable, because the size check makes the reset automatic, but it must not crash.
5. **Missing phone data** (no location or no network): Follow mode uses the last persisted times, or 07:00 and 19:00.

---

### Task 1: Daypart and moon phase (pure)

**Files:** create `src/c/daypart.h` and `src/c/daypart.c`. Modify `tests/host_tests.c` and `tests/Makefile` (add `../src/c/daypart.c` to PURE).

**Interfaces (produces):**
```c
typedef enum { DAY_SUNRISE = 0, DAY_MIDDAY = 1, DAY_SUNSET = 2, DAY_NIGHT = 3 } Daypart;
typedef enum { DPM_FOLLOW = 0, DPM_FIXED = 1, DPM_ALWAYS_DAY = 2 } DaypartMode;
Daypart daypart_at(int minute_of_day, uint16_t sunrise, uint16_t sunset);
Daypart daypart_for(uint8_t mode, int minute_of_day, uint16_t phone_rise, uint16_t phone_set,
                    uint16_t fixed_rise, uint16_t fixed_set);
float moon_phase(int year, int month, int day, int hour, int minute);   // 0..1, 0 = new, 0.5 = full
```

- [ ] **Step 1: Tests** (add them above `main` and call them from `main`)
```c
#include "daypart.h"
static void test_daypart_windows(void) {
  CHECK_EQ_INT(daypart_at(442, 442, 1158), DAY_SUNRISE);
  CHECK_EQ_INT(daypart_at(382, 442, 1158), DAY_SUNRISE);   // 60 min before
  CHECK_EQ_INT(daypart_at(381, 442, 1158), DAY_NIGHT);
  CHECK_EQ_INT(daypart_at(501, 442, 1158), DAY_SUNRISE);   // 59 min after
  CHECK_EQ_INT(daypart_at(502, 442, 1158), DAY_MIDDAY);
  CHECK_EQ_INT(daypart_at(1097, 442, 1158), DAY_MIDDAY);
  CHECK_EQ_INT(daypart_at(1098, 442, 1158), DAY_SUNSET);
  CHECK_EQ_INT(daypart_at(1218, 442, 1158), DAY_NIGHT);
  CHECK_EQ_INT(daypart_at(0, 442, 1158), DAY_NIGHT);
  CHECK_EQ_INT(daypart_at(10, 400, 1410), DAY_SUNSET);     // sunset 23:30: window wraps past midnight
  CHECK_EQ_INT(daypart_at(1439, 400, 1410), DAY_SUNSET);
}
static void test_daypart_modes(void) {
  CHECK_EQ_INT(daypart_for(DPM_ALWAYS_DAY, 0, 442, 1158, 420, 1140), DAY_MIDDAY);
  CHECK_EQ_INT(daypart_for(DPM_FOLLOW, 442, 442, 1158, 600, 900), DAY_SUNRISE);
  CHECK_EQ_INT(daypart_for(DPM_FIXED, 442, 442, 1158, 600, 900), DAY_NIGHT);
  CHECK_EQ_INT(daypart_for(DPM_FIXED, 600, 442, 1158, 600, 900), DAY_SUNRISE);
}
static void test_moon_phase(void) {
  float n = moon_phase(2024, 1, 11, 11, 57);                // new moon (UTC)
  CHECK(n < 0.03f || n > 0.97f);
  float f = moon_phase(2024, 1, 25, 17, 54);                // full moon (UTC)
  CHECK_NEAR(f, 0.5, 0.03);
  float q = moon_phase(2024, 1, 18, 3, 52);                 // first quarter
  CHECK_NEAR(q, 0.25, 0.04);
}
```

- [ ] **Step 2:** run `make -C tests` and confirm it fails (missing `daypart.h`).

- [ ] **Step 3: Implement**
```c
// daypart.c
#include "daypart.h"

static bool near_event(int m, int ev) {
  int d = ((m - ev) % 1440 + 1440) % 1440;
  return d < 60 || d >= 1380;
}
Daypart daypart_at(int m, uint16_t sr, uint16_t ss) {
  if (near_event(m, sr)) return DAY_SUNRISE;
  if (near_event(m, ss)) return DAY_SUNSET;
  return (m >= sr + 60 && m < ss - 60) ? DAY_MIDDAY : DAY_NIGHT;
}
Daypart daypart_for(uint8_t mode, int m, uint16_t pr, uint16_t ps, uint16_t fr, uint16_t fs) {
  if (mode == DPM_ALWAYS_DAY) return DAY_MIDDAY;
  return mode == DPM_FIXED ? daypart_at(m, fr, fs) : daypart_at(m, pr, ps);
}
static long jdn(int y, int mo, int d) {
  int a = (14 - mo) / 12, yy = y + 4800 - a, mm = mo + 12 * a - 3;
  return d + (153L * mm + 2) / 5 + 365L * yy + yy / 4 - yy / 100 + yy / 400 - 32045L;
}
float moon_phase(int y, int mo, int d, int h, int mi) {
  // reference new moon 2000-01-06 18:14 UTC; synodic month 29.530588853 days
  float days = (float)(jdn(y, mo, d) - jdn(2000, 1, 6)) + ((h * 60 + mi) - 1094) / 1440.0f;
  float p = days / 29.530588853f;
  p -= (float)(long)p;
  if (p < 0) p += 1;
  return p;
}
```
The header has include guards and includes `<stdint.h>` and `<stdbool.h>`.

- [ ] **Step 4:** run `make -C tests` and confirm it passes. Then run `pebble build 2>&1 | tail -40`.
- [ ] **Step 5:** commit `Add daypart timing and moon phase`.

---

### Task 2: Themes, floodlights, slope trees (pure)

**Files:**
- Create `src/c/theme.{h,c}`, `src/c/flood.{h,c}` and `src/c/decor.{h,c}`.
- Modify `tests/host_tests.c` and `tests/Makefile` (add the three `.c` files to PURE).

**Interfaces (produces):**
```c
// theme.h
typedef uint8_t Col8;
#define C8(hex) ((Col8)(0xC0 | ((((hex) >> 22) & 3) << 4) | ((((hex) >> 14) & 3) << 2) | (((hex) >> 6) & 3)))
typedef enum { BODY_SUN = 0, BODY_MOON = 1 } BodyKind;
typedef struct { BodyKind kind; float x, y; uint8_t r, halo; Col8 core, glow; } Body;   // x,y fractions of W,H
typedef struct {
  Col8 sky[4]; uint8_t nsky; Body body; bool stars, night;
  Col8 far[4], near_[4];              // lit, shade, cap lit, cap shade
  Col8 forest, pine_lit, pine_shade, pine_snow;
  Col8 snow, snow_shade, snow_shadow, powder, text;
  Col8 cloud[3];                      // reserved for phase 3
} Theme;
const Theme *theme_for(Daypart d);
uint8_t theme_bg_palette(const Theme *t, Col8 out[32]);  // distinct colours the stored background may hold

// flood.h
#define FLOOD_COUNT 5
typedef struct { float bx, by, hx, hy, dx, dy; } FloodHead;   // base, head, unit aim direction (px)
void  flood_heads(int W, int H, float k, FloodHead out[FLOOD_COUNT]);
float flood_at(const FloodHead h[FLOOD_COUNT], float k, float x, float y);   // 0..1

// decor.h
typedef struct { int16_t x, base, h; } SlopeTree;
#define SLOPE_TREE_COUNT 9
extern const SlopeTree SLOPE_TREES[SLOPE_TREE_COUNT];   // Emery pixel coordinates
```

**Values** (copy exactly from the spec §3 and §4):
- **`theme.c`:** four `static const Theme` values built from the spec §3 hex lists with `C8(...)`. Sun and moon positions, radius and halo also come from spec §3. Night has `stars = night = true`. `cloud[]` values per the prototype: sunrise FFFFAA/FFAAFF/AA55AA; day FFFFFF/AAFFFF/AAAAFF; sunset FFAA55/FF5500/AA0055; night 5555AA/AAAAFF/000055.
- **`theme_bg_palette`** collects distinct colours from:
  - the sky stops
  - the sun core and glow, or for the moon FFFFFF, AAAAAA and 000055
  - if stars: FFFFFF, AAFFFF, FFFFAA and 5555AA
  - the far and near peak colours, the forest, the three pine colours, trunk 550000, snow and snow_shade
  - if night: FFFFAA, FFFFFF, FFFF55 and 000055 (glow, floods, lamps, poles)
- **`flood.c`:**
  - Store the 5 {base, aim} fraction pairs from the spec §4. Head = base − (0, 16k). The aim direction is the unit vector from the head to the aim point. Use `LEN = 215`, and precompute `COSH = 0.9135455f` (cos 24°).
  - `flood_at` takes the maximum over the heads of `edge·fall·start`. Compute `fall^0.85` as `0.5f*(f + f75)` where `f75 = sqrt(f)*sqrt(sqrt(f))` (within 1.5% of the true power, no libm). Return 0 when `d > LEN·k` or `d < 1` or `cos ≤ COSH`.
- **`decor.c`:** the 9 trees from the spec §4, in that order.

- [ ] **Step 1: Tests**
```c
#include "theme.h"
#include "flood.h"
#include "decor.h"
static void test_theme_palette_budget(void) {
  Col8 buf[32];
  for (int d = 0; d < 4; d++) CHECK(theme_bg_palette(theme_for((Daypart)d), buf) <= 16);
  CHECK(theme_for(DAY_MIDDAY)->sky[0] == C8(0x55AAFF));   // phase 1 look preserved
  CHECK(theme_for(DAY_NIGHT)->night && theme_for(DAY_NIGHT)->body.kind == BODY_MOON);
  CHECK_EQ_INT(C8(0xFFFFFF), 0xFF); CHECK_EQ_INT(C8(0x000000), 0xC0); CHECK_EQ_INT(C8(0x55AAFF), 0xDB);
}
static void test_flood_coverage(void) {
  Layout L; layout_init(&L, 200, 228);
  FloodHead fh[FLOOD_COUNT]; flood_heads(200, 228, L.k, fh);
  long all = 0, lit = 0;
  for (uint8_t slot = 0; slot < 4; slot++) for (uint8_t d = 0; d < 10; d++)
    for (uint8_t j = 0; j < glyph_stroke_count(d); j++) {
      GlyphStroke g = glyph_stroke(d, j);
      for (int i = 2; i + 5 < g.n; i += 6) for (int s = 0; s <= 8; s++) {   // control-polygon sample is enough
        float t = s / 8.0f, m = 1 - t;
        float u = m*m*m*g.pts[i-2] + 3*m*m*t*g.pts[i] + 3*m*t*t*g.pts[i+2] + t*t*t*g.pts[i+4];
        float v = m*m*m*g.pts[i-1] + 3*m*m*t*g.pts[i+1] + 3*m*t*t*g.pts[i+3] + t*t*t*g.pts[i+5];
        Vec2 p = layout_glyph_point(&L, slot, u, v);
        all++; if (flood_at(fh, L.k, p.x, p.y) > 0.35f) lit++;
      }
    }
  CHECK(lit * 100 >= all * 95);
  CHECK(flood_at(fh, L.k, fh[0].hx + 20, fh[0].hy) == 0);     // behind the lamp (lamp 0 aims left)
  CHECK(flood_at(fh, L.k, fh[0].hx + fh[0].dx * 230, fh[0].hy + fh[0].dy * 230) == 0 ||
        flood_at(fh, L.k, fh[0].hx + fh[0].dx * 230, fh[0].hy + fh[0].dy * 230) < 0.35f);  // other cones may overlap there
}
static void test_slope_trees_clear(void) {
  Layout L; layout_init(&L, 200, 228);
  static SkPt pool[2000];
  const bool fm[4][2] = { {true,true}, {true,false}, {false,true}, {false,false} };
  for (int t = 0; t < SLOPE_TREE_COUNT; t++) {                  // static obstacles
    const SlopeTree *tr = &SLOPE_TREES[t];
    int hw = (tr->h * 42 + 99) / 100, x0 = tr->x - hw, x1 = tr->x + hw + 2, y0 = tr->base - tr->h, y1 = tr->base + 1;
    CHECK(y1 < L.comp_top - 3);
    CHECK(!(x1 >= L.chalet.x - 22 && y1 >= L.chalet.y - 14));
    for (int b = 0; b < 4; b++) CHECK(x1 < L.boxes[b].x - 4 || x0 > L.boxes[b].x + L.gw + 4 ||
                                      y1 < L.boxes[b].y || y0 > L.boxes[b].y + L.gh);
    (void)x0;
  }
  for (int c = 0; c < 4; c++) for (int h = 0; h < 24; h++) for (int m = 0; m < 60; m++) {
    DigitSlot s[4]; uint8_t n = time_slots(h, m, fm[c][0], fm[c][1], s);
    SkierSet set; skiers_init(&set, pool, SKI_POOL_CAP(200)); skiers_build(&set, &L, s, n);
    for (uint8_t i = 0; i < set.count; i++) {
      const Skier *sk = &set.sk[i];
      for (int t = 0; t < SLOPE_TREE_COUNT; t++) {
        const SlopeTree *tr = &SLOPE_TREES[t];
        int hw = (tr->h * 42 + 99) / 100;
        float x0 = tr->x - hw - 5, x1 = tr->x + hw + 2 + 5, y0 = tr->base - tr->h - 5, y1 = tr->base + 1 + 5;
        for (uint16_t p = 0; p < sk->n; p += 2) { float x = sk->pts[p].x / 16.0f, y = sk->pts[p].y / 16.0f;
          CHECK(!(x >= x0 && x <= x1 && y >= y0 && y <= y1)); }
        for (uint16_t tt = 0; tt <= 1500; tt += 30) { SkierState e = skier_exit_state(sk, i, tt, L.k);
          if (e.visible) { float x = e.x / 16.0f, y = e.y / 16.0f; CHECK(!(x >= x0 && x <= x1 && y >= y0 && y <= y1)); } }
      }
    }
  }
}
```
(The chalet and box checks match the occupancy used to choose the trees. If a CHECK in `test_slope_trees_clear` fails, report the tree and the minute. Do not move trees without flagging it.)

- [ ] **Step 2:** confirm the tests fail. **Step 3:** implement. **Step 4:** confirm the tests pass, and that `pebble build` succeeds.
- [ ] **Step 5:** commit `Add time-of-day themes, floodlights and slope trees data`.

---

### Task 3: Themed background on the watch (sky, sun or moon, stars, peaks, snow)

**Files:** modify `src/c/scene.{h,c}`, `src/c/aval_draw.c`, `src/c/sprites.c`, `src/c/comps.c` and `src/c/main.c`, plus `wscript` (MOCK_DAYPART).

**Interfaces:**
- Consumes `theme.h` and `daypart.h`.
- Produces:
  - `void scene_set_theme(const Theme *t, float moon_phase)`, which stores the theme and moon phase and invalidates the background if either changed
  - `const Theme *scene_theme(void)`

- [ ] **Step 1:** port the prototype's themed `buildBackground` into `scene.c`'s background builder.
  - **Sky:** N-stop dither.
  - **Sun:** disc and halo from `body`.
  - **Stars:** `Mulberry{99}`, 48 stars, including the plus-shaped glints.
  - **Moon:** per-pixel phase from the stored moon phase.
  - **Peaks:** use `far` and `near`.
  - **Rest of the background:** forest, pine colours (pass the theme to `pine()`), slope fill `snow`, crest and drift `snow_shade`.
  - Keep AA off. Everything else in the builder is unchanged.
- [ ] **Step 2: Per-frame role colours.**
  - **Avalanche:** everything `aval_draw.c` drew in White, Celeste or BabyBlueEyes now uses `powder`, `snow_shade` and `snow_shadow`.
  - **Sprites and chalet:** the `sprites_spray` colours and the chalet roof snow, eave and chimney cap (White and Celeste) use the same mapping.
  - **Smoke:** it uses `snow_shade` where it used Celeste. LightGray stays.
  - **Comps:** text uses `text`.
  - Tracks, skis and jackets are unchanged.
- [ ] **Step 3:** in `main.c`, call `scene_set_theme(theme_for(DAY_MIDDAY), 0)` at startup, so the face looks exactly like phase 1 until Task 5. Add `#ifdef MOCK_DAYPART` to force `theme_for(MOCK_DAYPART)` with the moon phase from today's date. Add `MOCK_DAYPART` to `wscript` the same way as `MOCK_ANIM_MS`.
- [ ] **Step 4: Verify.**
  - `make -C tests` passes and `pebble build` succeeds.
  - With `pebble wipe` first, take Emery screenshots at `MOCK_TIME=20:47` with `MOCK_DAYPART` = 0, 1, 2 and 3. Save them to the task's `shots/` folder as `t3-emery-{sunrise,day,sunset,night}.png`.
  - Compare them with the prototype's phase cards. MIDDAY must be pixel-identical in look to phase 1.
  - Log the background palette size once per theme build, and check that it is 16 or fewer.
- [ ] **Step 5:** commit `Build the background from the time-of-day theme`.

---

### Task 4: Night layers and slope trees on the watch

**Files:** modify `src/c/scene.{h,c}`. Add `scene_draw_campfire` and call it from `main.c`'s `render_frame` right after `scene_draw_chalet`, when `scene_theme()->night` is set.

- [ ] **Step 1: Background additions**, ported from the prototype and in the prototype's order:
  - **Window glow (night):** in the drift/crest pixel pass.
  - **Floodlight cones (night):** use `flood_heads` and `flood_at` over snow pixels between the ridge and the drift, with the FFFFAA/FFFFFF thresholds from the spec §4. Do this in the same pixel pass.
  - **Poles and lamps (night):** after the pixel pass.
  - **Slope trees (every theme, `W >= 200` only):** after the ridge pines, back to front by base, each with its `snow_shade` shadow ellipse.
- [ ] **Step 2: Campfire.** Port `drawCampfire` into `scene_draw_campfire(ctx, L, ms)`. Use `graphics_*` calls. The glow ellipses are dithered, so draw them with a framebuffer capture and bayer, like `fb_dither_circle`. Draw them first, then the logs, flames and sparks. Animate from the same ms as the smoke.
- [ ] **Step 3: Verify.**
  - Emery screenshots: `MOCK_DAYPART=3` at rest and at `MOCK_ANIM_MS=1200`, and `MOCK_DAYPART=1` at rest (trees in daylight). Save them as `t4-emery-{night,night-aval,day}.png`.
  - The digits must sit in bright snow at night. Trees must not overlap any skier or digit.
  - Check Basalt at night at rest (`t4-basalt-night.png`): no trees, and floods, moon and campfire present.
  - Report heap-free on both platforms.
- [ ] **Step 4:** commit `Add night floodlights, campfire, window glow and slope trees`.

---

### Task 5: Phase selection, sun times from the phone, settings

**Files:** modify `src/c/settings.{h,c}`, `src/c/main.c`, `package.json` (message keys), `src/pkjs/index.js` and `src/pkjs/config.js`.

- [ ] **Step 1: Settings.**
  - Add `uint8_t daypart_mode; uint16_t fixed_rise, fixed_set;` with defaults 0, 420 and 1140.
  - Add the message keys `DAYPART_MODE`, `SUN_FIXED_RISE`, `SUN_FIXED_SET`, `SUNRISE` and `SUNSET` to `package.json`.
  - `SUNRISE`/`SUNSET` are phone data, not settings. `inbox_received` stores them in `static uint16_t s_sun_rise = 420, s_sun_set = 1140;` and persists them as one struct under persist key 2. They are loaded at init.
- [ ] **Step 2: Phase selection in `main.c`.**
  - Add a helper `current_daypart()` that calls `daypart_for(settings mode, minute of day, s_sun_rise, s_sun_set, fixed_rise, fixed_set)`. Under `MOCK_DAYPART`, return that value instead.
  - Moon phase comes from `localtime` year, month, day, hour and minute.
  - `anim_start` calls `scene_set_theme(...)` **before** building the new set and starting the timer, so the first animated frame already uses the new background.
  - At init, `rebuild_current`, and in saver mode, apply the theme immediately.
  - The theme is only re-applied when the daypart changes, or the date changes for the moon (compare `tm_yday`).
- [ ] **Step 3: pkjs.**
  - In `index.js`, add `&daily=sunrise,sunset&timezone=auto&forecast_days=1` to the Open-Meteo URL.
  - Parse `data.daily.sunrise[0]` and `data.daily.sunset[0]`, which have the form `"YYYY-MM-DDTHH:MM"`, into minutes: `+s.substr(11,2)*60 + +s.substr(14,2)`.
  - Send them in the same message as `TEMP_F`. The watch must accept a dict containing `TEMP_F` plus `SUNRISE`/`SUNSET`: handle all three keys independently, and don't use `else if`.
  - In `config.js`, add a "Time of day" section with a mode select (Follow sunrise and sunset / Fixed times / Always midday) and two `type="time"` inputs converted to minutes. The inputs are disabled unless the mode is Fixed. Add the fields to DEFAULTS and `toMessage`.
- [ ] **Step 4: Verify.**
  - `make -C tests` passes and `pebble build` succeeds.
  - With `pebble send-app-message --int`, send SUNRISE and SUNSET values that put the current emulator time into each phase in turn. At the next minute change, the sky switches together with the avalanche. Take one screenshot per phase.
  - Test `DAYPART_MODE=2`: it is midday immediately, even at night.
  - Log heap-free on basalt. It must be at least 2048.
- [ ] **Step 5:** commit `Follow sunrise and sunset, add the time-of-day setting`.

---

### Task 6: Final verification and README

- [ ] Run `make -C tests`, `pebble build`, and a live check of 3 minute changes on Emery with no crash.
- [ ] Take the final screenshots of the four phases on Emery.
- [ ] Add a README section covering the time-of-day setting, the new MOCK_DAYPART recipe and the Open-Meteo sunrise/sunset data.
- [ ] Commit `Document times of day`.
