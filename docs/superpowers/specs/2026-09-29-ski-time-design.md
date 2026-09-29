# Ski Time: design spec

Date: 2026-09-29
Status: approved design (design board rounds 1 to 4)
Reference prototype: `docs/prototypes/design-board.html`. It is also published as the "Ski Time" artifact.

The prototype's JavaScript is the reference implementation for geometry, glyphs, timings and palette. When this spec and the prototype disagree, this spec wins. Where the spec is silent, match the prototype.

## 1. Summary

Ski Time is a Pebble watch face, written in C with a PebbleKit JS companion. It shows a ski slope that falls from the top right to the bottom left. Small skiers carve the time into the snow as curly script digits.

Each minute, this sequence runs:

1. The resting skiers ski out of frame.
2. An avalanche clears the slope.
3. New skiers drop in from the right and draw the new time. They leave dark grey tracks only on the digit strokes.
4. Each skier stops at the bottom of their digit and waits for the next minute.

## 2. Scope

- **Platforms:** `basalt` (144×168) and `emery` (200×228). Both are rectangular with 64 colours.
- **Out of scope:** round screens (chalk, gabbro) and black-and-white platforms (aplite, diorite).
- **Project style:** the same as `../constellation`:
  - pure C, SDK 3
  - `enableMultiJS`
  - a custom HTML settings page (not Clay) served as a `data:` URL
  - settings persisted as one struct blob
  - host unit tests for the pure-math modules under `tests/` (`make -C tests`)

## 3. Scene (static layers, back to front)

All coordinates below are for Emery. Basalt scales them by `k = W/200`, and the layout values are derived per platform as in the prototype's `makeLayout`.

| Layer | Details |
|---|---|
| Sky | 4×4 Bayer dither from PictonBlue (top) to Celeste (horizon). |
| Sun | Icterine disc, r = 7k, at (0.13W, 0.08H). PastelYellow halo, 6k wide, dithered. |
| Far peaks | BabyBlueEyes lit side, Liberty shade side, White/Celeste snowcaps. 5 peaks. |
| Near peaks | CobaltBlue lit side, OxfordBlue shade side, White/BabyBlueEyes caps. 3 peaks on the left. |
| Distant forest | Small CadetBlue triangles on the crest, from 0.16W to 0.8W. |
| Back pine row | Pines with a base 2k below the ridge, 8–15k tall, from 0.17W to 0.86W. The slope fill partly covers them. |
| Slope | White fill below the ridge curve `ridgeY(x) = yL + (yR−yL)·x/W − sin(πx/W)·0.035H`, where yL = 0.37H and yR = 0.035H. |
| Crest shade | Celeste dither, 8k deep under the ridge. |
| Front pine row | From 0.2W to 0.8W, spaced 5–11k, 11–22k tall (clamped to the sky above). Three-tier pines: DarkGreen sun side, MidnightGreen shade side, White snow, BulgarianRose trunk. Seeded PRNG, so the pattern is deterministic. |
| Drift line | A wavy lip at `compTop` (Celeste dither with a soft shadow below). It is the upper edge of the complication strip. |
| Chalet | Bottom right, drawn every frame after the avalanche. See §3.1. |
| Complication strip | Untouched snow of height 0.14H at the bottom. Nothing marks it: no tracks, avalanche or chalet. |

### 3.1 Chalet

The chalet sits in the bottom-right corner, drawn in three-quarter view:

- **Front (gable end):** faces the viewer and is lit by the sun. WindsorTan with BulgarianRose log lines, a door, a balcony rail, and a lit gable window (ChromeYellow/Icterine).
- **Side wall:** recedes to the right and rises 0.1h. BulgarianRose with two lit windows, drawn as parallelograms.
- **Side roof plane:** snow-covered, with a Celeste eave band, a BulgarianRose fascia and icicle pixels.
- **Front bargeboards:** BulgarianRose with a white snow line.
- **Chimney:** DarkGray, on the side roof, with a snow cap. Smoke is 4 looping LightGray/Celeste puffs that drift up and to the right.
- **Surroundings:** a Celeste shadow ellipse on the snow and a pine behind the chalet.
- **No red details** anywhere on the chalet.

## 4. Digits

### 4.1 Layout

- The digits sit in a 2×2 grid: hours above minutes.
- Glyph box: `gw = round(0.21W)`, `gh = round(1.2·gw)`.
- Gaps: `gx = round(0.035W)` between columns and `gy = round(0.028H)` between rows.
- Row 2 bottom sits 6k above `compTop`.
- The minutes row ends 8k left of the chalet.
- The hours row is shifted `round(0.07W)` right of the minutes row, which follows the fall line.
- Italic slant: `x += (0.5 − v)·0.16·gw`, where `v` is the vertical coordinate in the box (0 = top).

### 4.2 Glyphs

Each digit is 1 or 2 strokes. A stroke is a chain of cubic Béziers in unit box coordinates. The data is exactly `GLYPHS` in the prototype.

- Two-skier digits: 0, 4 and 8.
- The second skier of a pair starts 0.15 s after the first.

### 4.3 Time format (settings)

- 12 or 24 hour. The default is 24.
- Leading zero on or off. The default is on.
- With the leading zero off and a one-digit hour, the hour goes in the right-hand hour slot (over the minutes' ones digit). The left slot is empty and has no skier.

### 4.4 Tracks (setting)

- **Twin grooves (default):** two 1 px DarkGray lines offset ±1 px along the stroke normal.
- **Solid:** one DarkGray line, 3 px wide.

Tracks exist only on digit strokes. Ski-in and ski-out paths leave no marks.

## 5. Skiers

- **Sprite (about 8 px):**
  - two OxfordBlue ski lines along the heading, ±1.05 px apart, about 7k long
  - DarkGray poles trailing the body
  - a jacket disc (r = 1.9) and an OxfordBlue helmet (r = 1.25), offset up the screen
- **Jacket colours:** one per skier, cycling Red, ChromeYellow, BlueMoon, VividViolet, Orange, VividCerulean, Folly, JaegerGreen, indexed by `slot·2 + stroke`.
- **Heading:** the tangent of the path, smoothed over ±2.5 px and unwrapped, so rotation is continuous.
- **Ski-in path:** a cubic from `P0 = (W+10, clamp(start.y − (W+10−start.x)·0.45, ridgeY(W)+8k, compTop−10))` to the stroke start. Its tangent matches the stroke's start tangent.
- **Speed:**
  - On a stroke: `78k·(0.72 + 0.55·max(0, t·F))·(1 − 0.35·max(0, −t.y))`, where `t` is the unit tangent and `F = norm(−0.6, 0.8)` is the fall line.
  - On the ski-in path: 170k px/s, easing into the stroke speed over the last 30k px.
- **Start times:** slot `i` starts at `drawStart + i·0.2 s`. The second stroke of a pair adds 0.15 s.
- **Stop:** over 0.3 s, the skier slides 4k further along the end tangent and rotates to the hockey-stop heading. That heading is ±`norm(0.92, 0.39)`, whichever is nearer the direction of travel. A 6-pixel spray (BabyBlueEyes, then Celeste) shows during the stop.
- **Ski-out:**
  - Path: a cubic from the rest position toward the fall line, off the left edge or the bottom edge.
  - Motion: `s = ½·380k·τ² + 20k·τ`, starting 0.03 s after the previous skier.
  - The heading blends from the rest heading to the path heading over 0.35 s.

## 6. Minute-change timeline (target about 5 s)

| Time (s) | Event |
|---|---|
| 0 | Resting skiers begin to ski out. They are gone by about 0.95. |
| 0.55 | A crack (Liberty zigzag, growing over 0.15 s) opens across the slope near the top right. |
| 0.70 – 1.65 | The avalanche front travels along F from `u0` (the crack) to `u1` (past the bottom-left corner). Its position eases as `a^1.25`. |
| 1.65 – 1.95 | The wake and billows fade. |
| 1.45 | The first new skier enters. |
| about 4.0 – 5.2 | The last skier stops. The exact time depends on the digits. |

Old tracks are drawn until the avalanche passes over them. The avalanche erases them by restoring background pixels.

### 6.1 Avalanche

The avalanche is drawn in the slope region only, between the ridge and the drift line.

- **Front:** `front(s) = uf + amp·k·(14·(noise(0.045s+1.3)−0.5) + 6·(noise(0.13s+1.7τ)−0.5))`. Here `s` is the coordinate across the slope along `D ⊥ F`, and `amp = min(1, 2.5a)`.
- **Per-pixel pass**, where `depth = front(s) − u`:
  - `−9k < depth < −2k`: dithered BabyBlueEyes cast shadow.
  - `depth ≥ 0`: restore the background pixel, which erases the tracks.
  - `depth < body`, where body = 26k·grow: churn texture, White with Celeste/BabyBlueEyes from `n2(0.3s, 0.3u − 9τ)`.
  - `body ≤ depth < body + 40k`: wake streaks along F (Celeste), fading.
- **Vector pass:**
  - a back rank of billows
  - leading billows every 3.2k, with r = k·(3.5 + 5n + 1.8n′)·grow, churning ±1.2k. Each has a dithered Celeste halo, a BabyBlueEyes shadow offset to the lower right, a Celeste mid tone and a White top.
  - 28 hopping 2×2 debris chunks with shadows
  - 70 spray pixels

## 7. Complications

- Three slots (left, centre, right) in the strip below the drift line, drawn in Liberty.
- Font: `GOTHIC_24_BOLD` on Emery and `GOTHIC_18_BOLD` on Basalt.
- Types and formatting are the same as constellation: seconds, weather (°F from Open-Meteo through pkjs), date, steps, heart rate and battery. Configurable per slot. The default is weather / date / seconds.
- A seconds slot redraws every second while at rest. Any other combination ticks per minute.

## 8. Battery saver (setting, as in constellation)

A saver window (on/off, start and end in minutes of the day, handling midnight wrap; default off, 22:00 to 07:00). Inside the window:

- the time changes instantly, with no ski-out, avalanche or drawing
- ticks are per minute
- seconds show as `-`

## 9. Rendering on the watch

- **Background:** render the static background once into an 8-bit `GBitmap` the size of the screen. Direct pixel writes do the dithers, and `GContext` drawing into a bitmap-backed context does the shapes. Each frame copies it into the framebuffer (`graphics_capture_frame_buffer`) row by row.
- **Frame order:**
  1. background copy
  2. old tracks
  3. avalanche pixel pass (reads the background bitmap)
  4. avalanche vector pass
  5. new tracks
  6. chalet and smoke
  7. complications
  8. skiers
- **Frame rate:** a 40 ms `app_timer`, as in constellation. At rest, only redraw on the tick.
- **Numbers:** fixed point (Q4 px positions, ms times, Pebble trig angles) and deterministic PRNGs.
- **Paths:** skier paths are built once per minute change, at about 1 px spacing. The per-skier memory budget must fit basalt (allow ≤ 20 KB total for paths).

## 10. Settings page

The settings page follows constellation's `config.js` pattern: a self-contained HTML string, a live canvas preview if practical, and it returns JSON through `pebblejs://close`.

| Group | Fields |
|---|---|
| Complications | slots 1 to 3 |
| Clock | 24 hour on/off, leading zero on/off |
| Tracks | Twin grooves or Solid |
| Battery saver | on/off, start, end |

Message keys: `TEMP_F, COMP_1, COMP_2, COMP_3, TIME_24H, LEAD_ZERO, TRACK_STYLE, SAVER_ON, SAVER_START, SAVER_END`.

## 11. Testing

- **Host tests (`make -C tests`)**, using analog-adapt's trig shim:
  - cubic evaluation and resampling spacing
  - glyph data sanity: every stroke's end is lower than its start, or its climb is within the prototype's ratings
  - time-to-slots for 12/24 hour and leading zero
  - skier timing: monotonic times, and total duration between 3.5 and 5.5 s for all 1440 minutes
  - avalanche front monotonic in τ
  - saver window wrap
- **Emulator:**
  - `pebble build`, then `pebble install --emulator emery` and `--emulator basalt`
  - `MOCK_TIME` build flag, as in analog-adapt, for deterministic screenshots
  - compare screenshots against the design board's storyboard frames
