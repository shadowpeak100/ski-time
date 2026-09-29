# Ski Time phase 2: times of day

Date: 2026-09-29
Status: approved design. The user reviewed the design page and accepted every recommended decision.
Reference prototype: `docs/prototypes/times-of-day.html`, also published as the "Ski Time Days" artifact. Its JavaScript `THEMES`, `buildBackground`, `floodAt` and `floodHeads`, `drawCampfire`, `SLOPE_TREES` and `phaseAt` are the reference implementation. This spec wins where the two disagree.
Builds on: `2026-09-29-ski-time-design.md` (phase 1).

## 1. Summary

The scene changes with the time of day. There are four phases:

| Phase | Look |
|---|---|
| Sunrise | Lavender to butter-yellow sky, a low sun, first light on the snowcaps |
| Midday | The phase 1 look |
| Sunset | Crimson to gold sky, a big sun sinking behind the peaks, alpenglow |
| Night | Black sky, stars, a moon in its real phase, blue moonlit snow, lit windows with light spilling onto the snow, a campfire, and five floodlights that light the digits |

The design must also let phase 3 add weather layers later (clouds, fog, rain, snow, lightning), with colours taken from the active phase.

## 2. Phase timing

- Sunrise is active from 60 minutes before sunrise to 60 minutes after it: `[sr-60, sr+60)`. Sunset uses the same window around sunset: `[ss-60, ss+60)`.
- Midday is `[sr+60, ss-60)`. Night is everything else. All arithmetic wraps at midnight.
- There is one setting, **Time of day**, with three modes:
  - **Follow sunrise and sunset** (default): uses today's times from the phone.
  - **Fixed times**: uses the sunrise and sunset times set on the settings page. Defaults are 07:00 and 19:00.
  - **Always midday**.
- Before the phone has sent today's times, the watch uses the last times it received. On first install it uses 07:00 and 19:00.
- A new phase takes effect with a minute change. When the change starts, the background is rebuilt in the new theme, so the avalanche runs over the new sky and the new skiers drop into it. In battery-saver mode, or when settings change, the new phase takes effect immediately.

## 3. Themes: colour roles

A theme is a table of colour roles. Every scene, avalanche, sprite and complication colour that changes with the time of day is read from this table. Phase 3 weather layers will read `cloud[3]` and future roles from it.

| Role | Used for |
|---|---|
| `sky[2..4]` | Vertical Bayer-dithered stops, top to horizon |
| `body` | Sun (position, radius, core and glow colours, halo size) or moon (position, radius) |
| `stars` | Night only |
| `night` | Enables window glow, floodlights and campfire |
| `far[4]`, `near[4]` | Peak lit, peak shade, cap lit, cap shade |
| `forest` | Distant forest on the ridge |
| `pineLit`, `pineShade`, `pineSnow` | Pine trees |
| `snow`, `snowShade`, `snowShadow`, `powder` | Slope fill, crest shade and drift, avalanche shadow, spray, and billow tops. `powder` replaces White in the avalanche, chalet roof snow and debris. `snowShade` replaces Celeste. `snowShadow` replaces BabyBlueEyes. |
| `text` | Complication text |
| `cloud[3]` | Reserved for phase 3 |

Exact values per phase are copied from the prototype's `THEMES` (hex values in 64-colour Pebble space):

- **sunrise**
  - sky AAAAFF, FFAAFF, FFFFAA; sun at (0.10W, 0.25H), r 8, halo 10, core FFFF55, glow FFFFAA
  - far FFAAFF, AA55AA, FFFFAA, FFAAFF; near AA55AA, 550055, FFAAAA, FFAAFF; forest 5555AA
  - pines 005500, 005555, FFAAFF
  - snow FFFFFF, snowShade FFAAFF, snowShadow AAAAFF, powder FFFFFF; text AA55AA
- **day** (phase 1 values)
  - sky 55AAFF, AAFFFF; sun at (0.13W, 0.08H), r 7, halo 6, core FFFF55, glow FFFFAA
  - far AAAAFF, 5555AA, FFFFFF, AAFFFF; near 0055AA, 000055, FFFFFF, AAAAFF; forest 55AAAA
  - pines 005500, 005555, FFFFFF
  - snow FFFFFF, snowShade AAFFFF, snowShadow AAAAFF, powder FFFFFF; text 5555AA
- **sunset**
  - sky AA0055, FF0000, FF5500, FFAA00; sun at (0.30W, 0.20H), r 10, halo 8, core FFFF55, glow FFAA00
  - far FFAA55, AA55AA, FFFFAA, FFAAAA; near AA0055, 550055, FFAA55, FFAAAA; forest 550055
  - pines 005500, 000000, FFAAAA
  - snow FFFFFF, snowShade FFAAAA, snowShadow FFAAFF, powder FFFFFF; text AA0055
- **night**
  - sky 000000, 000000, 000055; moon at (0.15W, 0.10H), r 6; stars
  - far 5555AA, 000055, AAAAFF, 5555AA; near 000055, 000000, 5555AA, 000055; forest 000055
  - pines 005555, 000000, AAAAFF
  - snow AAAAFF, snowShade 5555AA, snowShadow 5555AA, powder FFFFFF; text 000055

**Background budget:** the stored background is 4 bits per pixel, so it holds at most 16 colours per theme. A host test must enforce this: sunrise 12, day 13, sunset 14, night 13 or fewer. The background is drawn without antialiasing.

## 4. Scene elements

- **Sky:** the dithered stops use `t = clamp(y/(ridgeY(0)*0.92))^1.2 × (stops−1)`. At each pixel choose stop `i` or `i+1` with Bayer threshold `t−i`.
- **Sun:** a disc plus a dithered halo, as in phase 1, but with the theme's position, radius and colours. The sunrise and sunset suns are drawn before the peaks, so the peaks can overlap them.
- **Moon:** a disc of radius r, lit by the real phase `p` (0 = new, 0.5 = full):
  - `w = sqrt(1−yn²)` and `xt = w·cos(2πp)`
  - waxing (`p < 0.5`) is lit where `xn > xt`; waning is lit where `xn < −xt`
  - lit pixels are FFFFFF, or AAAAAA where `xn²+yn² > 0.72`; unlit pixels are 000055
  - a 3k halo is dithered in 000055 at p = 0.3
  - `p` comes from the watch date: a synodic month of 29.530588853 days, with the reference new moon at 2000-01-06 18:14 UTC.
- **Stars:** 48 from `mulberry(99)` at y < 0.36H. A star is skipped within 3 px of the ridge. Colours are FFFFFF (70%), AAFFFF (20%) and FFFFAA (10%). Every ninth star gets a 5555AA plus-shaped glint.
- **Window glow (night):** a dithered FFFFAA patch on the snow in front of the chalet, as in the prototype.
- **Floodlights (night):**
  - There are 5 poles. Each is given as a base position and an aim point, as fractions of W and H:
    - (0.95, 0.30) aimed at (0.40, 0.36)
    - (0.97, 0.50) aimed at (0.38, 0.53)
    - (0.965, 0.64) aimed at (0.40, 0.70)
    - (0.07, 0.45) aimed at (0.62, 0.36)
    - (0.05, 0.76) aimed at (0.62, 0.62)
  - Each lamp head sits `16k` above its base. Cones reach `215k`, with a half-angle of 24°.
  - Intensity is the maximum over all lamps of `edge·fall·start`, where:
    - `edge = smooth(clamp((cosθ−cosH)/(1−cosH)·2.2))`
    - `fall = (1−d/len)^0.85`
    - `start = smooth(clamp((d−4k)/12k))`
  - Snow pixels between the ridge and the drift are painted from Bayer threshold `b`: FFFFAA where `b < 1.3I − 0.68`, else FFFFFF where `b < 1.3I`.
  - Poles are 000055. The lamp housing and lit face are drawn as in the prototype.
  - Every possible digit stroke is well lit. A host test checks that at least 95% of stroke points have I > 0.35 on Emery.
- **Campfire (night):** drawn every frame after the chalet at `(chalet.x − 9k, compTop − 5k)`:
  - FFAA55 and FFFFAA dithered glow ellipses, 550000 crossed logs and 555555 stones
  - three flame tongues in red, orange, chrome yellow and a yellow core, plus sparks
  - it animates from the same continuous phase clock as the smoke, so it moves only while the face redraws
- **Slope trees (Emery only, every phase):** 9 pines, each given as (x, base, height) and drawn back to front, each with a `snowShade` ellipse shadow `(x+2, base, 0.42h × 1.6)`:
  - (54, 80, 16), (41, 86, 16), (30, 100, 16), (24, 109, 9)
  - (180, 132, 10)
  - (46, 190, 8), (70, 190, 9), (100, 188, 10), (114, 189, 12)
  - A host test pins that no skier path, stop or ski-out point, over every minute and all 4 clock formats, comes within 5 px of any tree's rectangle including its shadow. It also checks the trees stay clear of the digit boxes, chalet, strip and poles.

## 5. Data from the phone

- pkjs asks Open-Meteo for `&daily=sunrise,sunset&timezone=auto&forecast_days=1` in the same request as the current temperature.
- It converts the local ISO times (`YYYY-MM-DDTHH:MM`) to minutes of the day and sends them as `SUNRISE` and `SUNSET`.
- The watch persists both (persist key 2) and uses them in Follow mode.

## 6. Settings

The settings page gets a new section, **Time of day**, with a mode select (Follow sunrise and sunset, Fixed times, Always midday) and sunrise and sunset time inputs used by Fixed mode.

The message keys are `DAYPART_MODE` (0 = follow, 1 = fixed, 2 = always day), `SUN_FIXED_RISE` and `SUN_FIXED_SET` (minutes).

## 7. Out of scope

- Weather (phase 3).
- Gradual blending inside a window.
- Slope trees on Basalt. Basalt gets the themes, moon, stars, glow, floodlights and campfire.

## 8. Testing

- **Host tests:**
  - daypart boundaries, including wrap and all three modes
  - moon phase for known dates (a new and a full moon)
  - palette budget for each theme
  - flood coverage of the digit strokes on Emery
  - flood intensity is zero behind each lamp and past the end of its cone
  - slope-tree clearance
- **Emulator:**
  - a `MOCK_DAYPART=0..3` build define forces a phase, for screenshots of the four phases on Emery at `MOCK_TIME=20:47` (rest, plus night at `MOCK_ANIM_MS=1200`)
  - `pebble send-app-message` checks `SUNRISE`/`SUNSET` and `DAYPART_MODE`
  - a live minute change across a phase boundary
