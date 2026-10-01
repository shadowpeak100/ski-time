# Ski Time phase 3: weather and animation modes

Date: 2026-09-30
Status: approved design. The user reviewed the weather and battery-mode design pages and approved them.
Reference prototypes (their JavaScript is the reference implementation):
- `docs/prototypes/weather.html`: `WEATHER`, `WX_SKY`, `WX_ROLE`, `WX_STORM_CLOUD`, `drawFlatCloud`, `drawCloud` (deck), the fog pass in `buildBackground`, `drawWeather`, `boltPath`, `drawBolt`, `flake`, `WX_MODE`, `SNOW_STYLE='mixed'`
- `docs/prototypes/battery-modes.html`: the still weather frames and the mode descriptions

This spec wins where it differs from the prototypes.
Builds on: `2026-09-29-ski-time-design.md` and `2026-09-29-times-of-day-design.md`.

## 1. Summary

The sky shows the current weather. It comes from the same Open-Meteo request the phone already makes for temperature and sun times.

- **Colours:** every weather layer takes its colours from the active time of day.
- **Animations setting:** controls how much the face moves:
  - Full animations
  - Balanced
  - Battery saver
- **Battery saver schedule:** keeps working.
- **Platforms:** weather rendering is Emery only. Basalt keeps its phase 2 look, but its Animations setting still honours Battery saver.

## 2. Weather types

The phone maps WMO codes to weather types. The watch receives the WMO code. `wx_from_wmo` maps it; unknown codes map to clear.

| Weather | WMO | Layers |
|---|---|---|
| Clear | 0 | none |
| Partly cloudy | 1, 2 | 3 flat fluffy clouds (`drawFlatCloud`) behind the peaks, at (0.62W, 0.07H, s 1), (0.2W, 0.06H, 0.8), (0.9W, 0.13H, 0.7); the sun, moon and stars still show |
| Overcast | 3 | two-row cloud deck (`drawCloud` rows, back row darker); overcast sky stops; no sun, moon or stars |
| Fog | 45, 48 | overcast plus a dithered fog pass. The pass has density 0.85 × (1 − clamp((y − 0.08H)/(0.5H))), is painted in the fog colour, and covers rows above `comp_top` |
| Drizzle | 51–57 | overcast plus rain: rate 32, length 2, speed 110 |
| Rain | 61–67, 80–82 | overcast plus rain: rate 70, length 4, speed 190 |
| Snow | 71–77, 85, 86 | overcast plus snow: rate 95, speed 26, look D. Big flakes (every third) are plus-shaped crystals: white centre, AAAAFF arms and a 5555AA shadow. Small flakes are an AAAAFF dot with a 5555AA shadow |
| Thunderstorm | 95–99 | storm sky, storm clouds, rain (rate 90, length 5, speed 230), and the bolt |

### Colours per time of day

| | Overcast sky | Storm sky | Storm clouds (lit, mid, shade) | Rain | Fog |
|---|---|---|---|---|---|
| Sunrise | AAAAAA, FFAAFF | 555555, AA55AA | AA55AA, 550055, 550055 | AA55AA | FFAAFF |
| Midday | AAAAAA, FFFFFF | 555555, AAAAAA | AAAAAA, 555555, 555555 | 5555AA | FFFFFF |
| Sunset | 555555, AA5555, FFAA55 | 000000, 550000, AA5555 | AA5555, 550000, 550000 | 550055 | FFAAAA |
| Night | 000000, 000055 | 000000, 555555 | 555555, 000055, 000055 | AAAAFF | 555555 |

Non-storm clouds use the theme's `cloud[3]`.

**Background budget:** every phase × weather combination must stay within 16 background colours. The measured maximum is 14. A host test enforces this.

## 3. The lightning bolt

The bolt is drawn in the style of FireWatchDynamic's storm art. It is always the same bolt: `mulberry(307)`, starting at (0.2W, 6k) and ending at (0.1W, ridge − 2), between the two big peaks on the left.

- **Main path:** midpoint displacement, depth 5 (33 points), roughness 9k.
- **Branches:** three, at 28%, 45% and 62% of the main path. They alternate side, have length 12–24k, depth 4 and roughness 4k.
- **Drawing order:**
  1. main glow: a 3 px dithered AAAAFF line at 55%
  2. branch glow: a 2 px dithered 5555AA line at 50%
  3. branch core: AAFFFF along the first 80% of each branch
  4. main core: 1 px FFFFFF

In Full animations the bolt flashes every 5 s as a double flicker. It is on at t mod 5 < 0.12 and 0.2–0.34. During the first 0.06 s of each strike, the sky above the ridge gets a dithered white flash at 35%. In Balanced and Battery saver the bolt is always shown and there is no flash.

## 4. Animations setting

| Mode | Minute change | Weather between minute changes |
|---|---|---|
| Full animations (may use more battery) | full animation | Rain, drizzle and snow fall, and the bolt flickers. When the weather has motion, the face redraws on a 100 ms timer at rest. |
| Balanced (default) | full animation | Held still: fixed positions, with more drops (rain ×1.7, drizzle ×2, snow ×1.5) and the bolt always shown. No rest timer. |
| Battery saver | instant, as in phase 1 saver | held still, as in Balanced; seconds show `-` |

- **Battery saver schedule:** "Use Battery saver on a schedule" replaces the phase 1 saver switch. It reuses the `SAVER_ON`, `SAVER_START` and `SAVER_END` keys and defaults to off, 22:00–07:00. Inside the window the effective mode is Battery saver.
- **Show weather:** a setting, on by default. When it is off, the watch always renders clear.
- **Wind:** rain and snow slant with the wind. The phone sends `WIND_X`:
  - −1 when the wind blows toward the west, +1 toward the east
  - 0 when the wind is below 5 km/h, meaning vertical
  - The slant is 0.3 px per px of fall × `WIND_X`.

## 5. Data and timing

- **Request:** pkjs adds `weather_code,wind_speed_10m,wind_direction_10m` to Open-Meteo `current`.
- **Message:** it sends `WX_CODE` (int) and `WIND_X` (−1, 0, 1) in the same message as `TEMP_F`, `SUNRISE` and `SUNSET`. That message is 1 + 5 × 11 = 56 B.
- **Persistence:** the watch saves `{wmo, wind}` under persist key 3. Before any data arrives, the weather is clear.
- **Timing:** a new weather takes effect at the next minute change, together with the time of day, through the same background rebuild. Changing the Show weather setting applies immediately.
- **Message sizes:** the settings message grows to 14 keys (1 + 14 × 11 = 155 B), which still fits the 160 B inbox.

## 6. Settings

Two keys are added:
- `ANIM_MODE`: 0 full, 1 balanced, 2 saver; default 1
- `WEATHER_ON`: default 1

The phase 2 Settings blob (16 B) is an exact prefix of the new one, so it is migrated: read 16 B, then default the new fields.

On the settings page:
- the Animations select appears first under a Display section
- the old battery saver toggle is relabelled "Use Battery saver on a schedule"
- a new "Show weather" toggle is added

## 7. Rendering and performance

- **Background (Emery):** clouds, the deck and fog are baked into the background during the existing rebuild.
- **Per frame (Emery):** rain, snow, the bolt and the flash are drawn each frame after the slope trees, and never at or below `comp_top − 2`.
- **Full-mode rest timer:** 100 ms. It runs only when all of these hold: weather is on, the weather has precipitation or lightning, the effective mode is Full, and no minute animation is running. A minute animation's own 40 ms timer takes over while it runs.
- **Memory:** basalt compiles no weather drawing (`PBL_PLATFORM_EMERY`) and its heap stays at least 2720 B at rest (measured 3004 B). Emery must log its heap at rest.

## 8. Testing

- **Host tests:**
  - `wx_from_wmo` for every code range
  - palette budget, 16 or fewer for all 32 combinations
  - bolt geometry: deterministic, every point within the screen and above the ridge, 3 branches
  - precipitation: every drop and flake above `comp_top − 2`; still positions identical for any t; counts per mode
  - effective animation mode, including the schedule
  - settings migration size
- **Emulator:**
  - `MOCK_WEATHER=0..7` and `MOCK_ANIM_MODE=0..2` build defines
  - Emery screenshots of all 8 weathers at midday, and of storm, rain, snow and partly at night
  - Balanced and Full comparison for rain and storm
  - a live Full-mode run showing falling rain and bolt flicker with no crash
  - basalt still builds, and its heap rule holds
