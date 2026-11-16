# Ski Time: weather location and animation length

Date: 2026-10-02
Status: draft, for user review.
User choices:
- **Location:** a city search, where you pick from the matches.
- **Animation length:** the slider sets the exact total time of the change.

## 1. Weather location

### Settings page
A new **Location** section, placed under Display, holds:

- **"Use my location"**, an On/Off toggle, default On. When it is on, the phone's location is used as it is today.
- When it is off, three more things appear:
  - A search row: a text box (placeholder "Town or city") and a **Find** button.
  - Under the search row, a list of up to 5 matches. Each match reads as `Name, Region, Country`, for example `Park City, Utah, United States`. Tapping a match selects it.
  - A line under the list: **"Weather for: <selected name>"**, or "No place chosen yet" when nothing is selected.

How the search works:

- Find calls `https://geocoding-api.open-meteo.com/v1/search?name=<encoded text>&count=5&language=en&format=json` from the page itself. It is the same provider as the weather, with no key.
- The search row shows "Searching…" while the request runs.
- It shows "No matches" when the request returns nothing, and "Couldn't search — check your connection" when the request fails.
- Pressing Enter in the text box also runs Find.

Saving:

- Saving with the toggle off and no place chosen keeps "Use my location" on. The page says so beside the toggle instead of blocking Save.

### Phone behaviour
- The saved config gains these fields:
  - `locAuto` (bool, default true)
  - `locName` (string)
  - `locLat` (number)
  - `locLon` (number)

  They live only on the phone. Nothing new is sent to the watch for location.
- `fetchWeather` uses `locLat`/`locLon` when `locAuto` is false and a place is set. Otherwise it uses geolocation, as today.
- The sunrise and sunset times come from the same request, so they follow the chosen place too.
- After a Save that changes the location, the phone fetches the weather at once, so the watch picks up the new weather and sun times at its next minute change.
- If geolocation fails while `locAuto` is on, the behaviour is the same as today: an error is logged and nothing is sent.

## 2. Animation length

### Settings page
Under **Animations**, add an **"Change length"** row with a range slider and a number box.

**Slider**
- The slider runs from 0 to 20 seconds in 0.1 s steps.
- A notch (a tick mark under the track) sits at 6.0 s, labelled "Recommended".

**Label**
- The label beside the slider reads `6.0 s` style text, or "Instant" at 0.
- At the recommended setting it reads "About 6 s (recommended)".

**Number box**
- The number box (`type=number`, `step=0.1`, `min=0`, `max=20`) shows the same value. Typing a value moves the slider.
- Values are clamped to 0–20 and rounded to 0.1 s.

**Default**
- The default is **Recommended**. This keeps today's natural timing: each change takes its own length, 5.2–6.8 s depending on the digits, with no scaling.
- Moving the slider to within ±0.2 s of the notch snaps it back to Recommended.
- Any other value is an exact total time.

**Battery saver**
- When Animations is Battery saver, the row is shown but disabled, with the note "Battery saver changes instantly".

**Preview**
- The preview's Run button uses the same length.

### Watch behaviour
- **New setting `anim_len`** (uint8, tenths of a second):
  - 0 means instant
  - 1–200 means an exact total time
  - 255 means Recommended (the default)
  - New message key `ANIM_LEN`
- **Exact time:**
  - The whole minute change is time-scaled to fit the chosen length: ski-out, crack, avalanche and the new skiers drawing.
  - The animation clock is `t = elapsed × natural / chosen`, where natural is `skiers_duration` of the new set, and the change ends when `elapsed ≥ chosen`.
  - Only the change's own timeline is scaled. Chimney smoke, the campfire and falling weather keep real time.
- **Instant (0):** the change behaves like Battery saver's change, a single rebuild with no animation. Seconds still tick, and weather still moves in Full.
- **Replay on open:** the one-second replay when the face opens uses the same length. At Instant it is skipped.
- **Frame rate:** unchanged, at 40 ms frames. Longer lengths give smoother avalanches; very short lengths skip frames by design.

### Storage and messages
- `Settings` grows by 1 byte. The 20 B layout is migrated: read it, then set `anim_len = 255`.
- The settings message grows to 17 keys: 1 + 17 × 11 = 188 B, within the 192 B inbox.
- Basalt's heap at rest must stay at least 2720 B.

## 3. Testing
- **Host tests (C):**
  - the scaled-time function, covering 255 natural, 0 instant, 10 = 1.0 s and 200 = 20 s, where the mapped `t` reaches the natural duration exactly at the chosen length
  - settings migration from 20 B
- **Node tests (page):**
  - the slider and number box stay in sync, including clamping and rounding
  - snap to Recommended
  - `toMessage` sends `ANIM_LEN` (255, 0 or tenths)
  - the location UI shows and hides correctly
  - selecting a match sets `locName`/`locLat`/`locLon`
  - Save with no place keeps `locAuto` on
  - the geocoding request URL is encoded
  - "No matches" and error states, using a stub `fetch`/`XMLHttpRequest`
- **Node tests (phone):** `fetchWeather` builds its URL from the manual coordinates when they are set, and asks for geolocation otherwise. Use a stub `Pebble`, `navigator` and `XMLHttpRequest`.
- **Emulator:**
  - On Emery, check a live minute change at 2.0 s, 12.0 s and Instant, timing the change from the logs.
  - On basalt, check one live change and the heap at rest.
