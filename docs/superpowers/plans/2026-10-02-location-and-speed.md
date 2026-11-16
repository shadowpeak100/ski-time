# Weather Location and Animation Length Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Two new settings.
- **Location:** a manual weather location, chosen by searching for a city and picking a match. Otherwise the phone's own location is used, as today.
- **Change length:** a "Change length" slider and number box (0–20 s, with a notch at Recommended) that time-scales the whole minute change on the watch and in the preview.

**Architecture:**
- **Watch:** a new pure C module, `anim_len`, maps elapsed real time to timeline time. `main.c` uses it for:
  - the frame's `t`
  - the end of the animation
  - instant mode
  - the replay when the face opens

  The new setting is one byte, `anim_len`, sent as `ANIM_LEN`.
- **Phone:**
  - The page's Save is still bound first.
  - Location is phone-only. The config gains `locAuto`, `locName`, `locLat` and `locLon`, and `fetchWeather` uses them when they are set.
  - The page's slider logic and the time-scaling function live in `config.js` as plain functions. They are exported for tests and inlined into the page with `fn.toString()`.

**Tech Stack:** Pebble SDK C (host tests via `make -C tests`), PebbleKit JS, in-page ES2015, node tests (`tests/pkjs/*.js`), and the emulator.

**Spec:** `docs/superpowers/specs/2026-10-02-location-and-speed-design.md`

## Global Constraints

- **`anim_len` encoding** (uint8, tenths of a second):
  - `0` = instant
  - `1..200` = an exact total length
  - `255` = Recommended, the natural per-change length (default)
  - values 201–254 are treated as 200
- **Message key** `ANIM_LEN`, appended after `WX_FIXED` in `package.json` `messageKeys`. Do not reorder.
- **Settings message:** 17 keys = 188 B, which must fit the existing `app_message_open(192, 16)` inbox.
- **Basalt:** heap at rest must stay at least 2720 B.
- **Exact length:**
  - `t = elapsed × natural / (len × 100)`, where `natural = skiers_duration(s_new)`
  - the change ends when `elapsed ≥ len × 100`
  - only the change timeline is scaled; smoke, the campfire and weather keep real time
- **Instant (`0`):** behaves like Battery saver's minute change (`rebuild_current`, no animation). The replay when the face opens is skipped. Seconds and Full-mode weather are unaffected.
- **Slider:**
  - range 0–20 s in 0.1 s steps
  - the notch at 6.0 s is labelled "Recommended"
  - dragging the slider to within ±0.2 s of 6.0 snaps to Recommended (255)
  - typed values clamp to 0–20 and round to 0.1, and never snap
- **Labels:**
  - "About 6 s (recommended)" for 255
  - "Instant" for 0
  - otherwise `X.X s`
- **Battery saver:** when `animMode === 2`, the slider and number box are disabled and the note "Battery saver changes instantly" shows.
- **Geocoding URL:** `https://geocoding-api.open-meteo.com/v1/search?name=<encodeURIComponent(text)>&count=5&language=en&format=json`
- **Match label:** `[name, admin1, country]`, with empty parts dropped, joined by `", "`.
- **Location strings:**
  - "Searching…"
  - "No matches"
  - "Couldn't search — check your connection"
  - "Weather for: <name>"
  - "No place chosen yet"
  - the search box placeholder is "Town or city"
- **Save with no place:** saving with `locAuto` false and no place chosen saves `locAuto: true`. While the toggle is off with no place, the page shows "Pick a place, or your phone's location will be used."
- **Location never goes to the watch.** After a Save that changes `locAuto`, `locLat` or `locLon`, the phone calls `fetchWeather()` immediately.
- **Commits:** end each commit message with a blank line and then `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **Instant while a change is already animating.** Saving `animLen: 0` mid-animation must stop it cleanly; the existing settings path calls `anim_stop()`. The next minute change must be instant. Checked live in Task 4.
2. **Very short lengths, 0.1–0.5 s.** There may be only 0–12 frames. The last frame must still draw the finished time, never a half-erased slope. In practice the timeline clamps at `natural`, and the resting frame follows `anim_stop`. A Task 1 host test covers `scaled_t` at `elapsed ≥ total`.
3. **Typed junk in the number box** ("", "abc", "-3", "25", "6.04"). These must give the old value, 0, 20 and 6.0 (exact, not snapped) respectively, and never NaN in `CFG`. Covered by Task 2's test.
4. **Geocoding results with missing fields** (no `admin1`, no `results` key at all) and names containing `<`/`&`. These must render as text, not HTML. Covered by Task 3's test.
5. **Old saved phone config with no location fields.** It must keep using geolocation. Covered by Task 3's index test.

---

## File Structure

- **Create `src/c/anim_len.h` and `src/c/anim_len.c`:** pure, no `pebble.h`. They provide:
  - `#define ANIM_LEN_NATURAL 255`
  - `uint32_t anim_len_total_ms(uint16_t natural_ms, uint8_t len)`
  - `uint16_t anim_len_t(uint32_t elapsed_ms, uint16_t natural_ms, uint8_t len)`
- **Modify:**
  - `src/c/settings.h` and `src/c/settings.c`: the `anim_len` field, its migration and the message key
  - `src/c/main.c`: wiring
  - `package.json`: the key
  - `tests/Makefile`: add `anim_len.c` to `PURE`
  - `tests/host_tests.c`: tests
- **Modify `src/pkjs/config.js`:**
  - `DEFAULTS` gains `animLen: 255, locAuto: true, locName: '', locLat: null, locLon: null`
  - `toMessage` sends `ANIM_LEN`
  - new pure helpers `animScaledMs`, `lenFromSlider`, `lenFromTyped`, `lenLabel`, `geoUrl` and `placeLabel`, exported and inlined into the page
  - new markup and page wiring
- **Modify `src/pkjs/index.js`:**
  - `fetchWeather` takes its coordinates from `weatherCoords(cb)`
  - `webviewclosed` refetches after a location change
- **Create `tests/pkjs/index_test.js`:** stubs `Pebble`, `navigator`, `XMLHttpRequest` and `localStorage`, then `require`s `index.js`.
- **Modify:** `tests/pkjs/config_test.js` and `README.md`.

---

### Task 1: Watch: the `anim_len` setting and time scaling

**Files:**
- Create: `src/c/anim_len.h`, `src/c/anim_len.c`
- Modify: `src/c/settings.h`, `src/c/settings.c`, `src/c/main.c`, `package.json`, `tests/Makefile`, `tests/host_tests.c`

**Interfaces:**
- Produces:
  - `Settings.anim_len` (uint8)
  - message key `ANIM_LEN`
  - `anim_len_total_ms` and `anim_len_t` as specified below
- Consumes:
  - `skiers_duration(const SkierSet*)` (`skier.h`)
  - the existing `main.c` functions `anim_start`, `anim_stop`, `rebuild_current`, `tick_handler`, `intro_cb`, `anim_timer_cb` and `update`

- [ ] **Step 1: Write the failing host test.** In `tests/host_tests.c`, add `#include "anim_len.h"` with the other includes. Add this function, and call it from `main` next to the other test calls:

```c
static void test_anim_len(void) {
  // Recommended: natural timing, unscaled.
  CHECK_EQ_INT(anim_len_total_ms(6000, ANIM_LEN_NATURAL), 6000);
  CHECK_EQ_INT(anim_len_t(1234, 6000, ANIM_LEN_NATURAL), 1234);
  CHECK_EQ_INT(anim_len_t(70000, 6000, ANIM_LEN_NATURAL), 65534);   // clamps to uint16 range
  // Instant.
  CHECK_EQ_INT(anim_len_total_ms(6000, 0), 0);
  CHECK_EQ_INT(anim_len_t(0, 6000, 0), 6000);                        // already finished
  // Exact 2.0 s for a 6 s natural change: 3x faster.
  CHECK_EQ_INT(anim_len_total_ms(6000, 20), 2000);
  CHECK_EQ_INT(anim_len_t(1000, 6000, 20), 3000);
  CHECK_EQ_INT(anim_len_t(2000, 6000, 20), 6000);
  CHECK_EQ_INT(anim_len_t(5000, 6000, 20), 6000);                    // never past natural
  // Exact 20 s: 0.3x.
  CHECK_EQ_INT(anim_len_total_ms(6000, 200), 20000);
  CHECK_EQ_INT(anim_len_t(10000, 6000, 200), 3000);
  // 201..254 behave as 200.
  CHECK_EQ_INT(anim_len_total_ms(6000, 230), 20000);
  // Shortest exact: 0.1 s.
  CHECK_EQ_INT(anim_len_total_ms(5200, 1), 100);
  CHECK_EQ_INT(anim_len_t(50, 5200, 1), 2600);
}
```

  Add `../src/c/anim_len.c` to the `PURE` list in `tests/Makefile`.

- [ ] **Step 2: Run it and check that it fails.**
  - Run: `make -C tests 2>&1 | tail -5`
  - Expected: a compile error, because `anim_len.h` is not found.

- [ ] **Step 3: Write the module.** `src/c/anim_len.h`:

```c
#ifndef ANIM_LEN_H
#define ANIM_LEN_H

#include <stdint.h>

#define ANIM_LEN_NATURAL 255   // Settings.anim_len: 0 instant, 1..200 tenths of a second, 255 natural

uint32_t anim_len_total_ms(uint16_t natural_ms, uint8_t len);
// Timeline ms for real elapsed ms: scaled so the change fills the chosen length, never past natural_ms.
uint16_t anim_len_t(uint32_t elapsed_ms, uint16_t natural_ms, uint8_t len);

#endif
```

  `src/c/anim_len.c`:

```c
#include "anim_len.h"

uint32_t anim_len_total_ms(uint16_t natural_ms, uint8_t len) {
  if (len == ANIM_LEN_NATURAL) return natural_ms;
  return (uint32_t)(len > 200 ? 200 : len) * 100;
}

uint16_t anim_len_t(uint32_t elapsed_ms, uint16_t natural_ms, uint8_t len) {
  if (len == ANIM_LEN_NATURAL) return elapsed_ms < 65534 ? (uint16_t)elapsed_ms : 65534;
  uint32_t total = anim_len_total_ms(natural_ms, len);
  if (total == 0 || elapsed_ms >= total) return natural_ms;
  return (uint16_t)(elapsed_ms * natural_ms / total);
}
```

- [ ] **Step 4: Run it and check that it passes.**
  - Run: `make -C tests 2>&1 | tail -2`
  - Expected: `all host tests passed`.

- [ ] **Step 5: Add the setting.**
  - **`src/c/settings.h`:** add `uint8_t  anim_len;                 // 0 instant, 1..200 tenths of a second, 255 natural` after `wx_fixed`.
  - **`src/c/settings.c`:**
    - Add `#define PHASE4_SETTINGS_SIZE 20`.
    - Change the `_Static_assert` to `sizeof(Settings) == 22`. The struct pads to an even size; if the compiler says otherwise, use the size it reports and say so in the report.
    - Add `.anim_len = 255,` to `DEFAULTS`.
    - Accept `PHASE4_SETTINGS_SIZE` in the size check in `settings_load`. The 20 B layout is an exact prefix, and the default 255 stays because `s = DEFAULTS` is set before the read.
    - Add `changed |= set_u8(it, MESSAGE_KEY_ANIM_LEN, &s.anim_len);`.
  - **`package.json`:** append `"ANIM_LEN"` after `"WX_FIXED"`.

- [ ] **Step 6: Wire it into `src/c/main.c`.** Add `#include "anim_len.h"`.
  - **`anim_timer_cb`:** change `if (t >= skiers_duration(s_new))` to `if (t >= anim_len_total_ms(skiers_duration(s_new), settings_get()->anim_len))`.
  - **`update()`, non-mock branch:** the `Frame` `t` becomes `s_anim ? anim_len_t(el, skiers_duration(s_new), settings_get()->anim_len) : SKI_REST_MS`. Keep `s_smoke_ms = s_smoke_base + el;` unchanged (real time).
  - **`tick_handler`:** change `if (s_saver) rebuild_current(); else anim_start();` to `if (s_saver || settings_get()->anim_len == 0) rebuild_current(); else anim_start();`.
  - **`intro_cb`:** change the condition to `if (!s_saver && !s_anim && settings_get()->anim_len != 0) anim_start();`.
  - **Check the mock build:** `MOCK_ANIM_MS` builds still freeze at the given timeline ms, unscaled.

- [ ] **Step 7: Build and run the tests.**
  - Run: `make -C tests 2>&1 | tail -2`, then `pebble clean`, then `pebble build 2>&1 | tail -3`.
  - Expected: both pass, for both platforms.

- [ ] **Step 8: Commit.**

```bash
git add src/c/anim_len.h src/c/anim_len.c src/c/settings.h src/c/settings.c src/c/main.c package.json tests/Makefile tests/host_tests.c
git commit -m "Scale the minute change to a chosen length"
```

---

### Task 2: Page: the "Change length" slider, the message key and the preview

**Files:**
- Modify: `src/pkjs/config.js`, `tests/pkjs/config_test.js`

**Interfaces:**
- Consumes: the `ANIM_LEN` encoding from Task 1, and the page's existing `frame`/`redraw`/`SkiPreview.draw`. `draw` returns `{durationMs}`, the natural length.
- Produces, exported from `config.js`:
  - `animScaledMs(elapsed, natural, len)`: the JS twin of `anim_len_t`. It returns the timeline ms.
  - `animTotalMs(natural, len)`
  - `lenFromSlider(v)`: `v` is the slider value 0..200; returns 0..200, or 255 when `|v-60| <= 2`.
  - `lenFromTyped(str, prev)`: `parseFloat`. Returns `prev` when the input is NaN; otherwise rounds to tenths, clamps to 0..200, and never snaps.
  - `lenLabel(len)`
  - `DEFAULTS.animLen = 255`
  - `toMessage` sends `ANIM_LEN`

- [ ] **Step 1: Write the failing tests.** Append to `tests/pkjs/config_test.js`, before the final `console.log`:

```js
// Change length helpers (JS twin of src/c/anim_len.c)
assert.strictEqual(config.DEFAULTS.animLen, 255);
assert.strictEqual(config.toMessage(cfg()).ANIM_LEN, 255);
assert.strictEqual(config.toMessage(cfg({ animLen: 0 })).ANIM_LEN, 0);
assert.strictEqual(config.toMessage(cfg({ animLen: 125 })).ANIM_LEN, 125);
var oldCfg = cfg(); delete oldCfg.animLen;
assert.strictEqual(config.toMessage(oldCfg).ANIM_LEN, 255, 'old config without animLen sends natural');
assert.strictEqual(config.animTotalMs(6000, 255), 6000);
assert.strictEqual(config.animTotalMs(6000, 20), 2000);
assert.strictEqual(config.animTotalMs(6000, 0), 0);
assert.strictEqual(config.animScaledMs(1000, 6000, 20), 3000);
assert.strictEqual(config.animScaledMs(5000, 6000, 20), 6000);
assert.strictEqual(config.animScaledMs(1234, 6000, 255), 1234);
assert.strictEqual(config.lenFromSlider(60), 255);
assert.strictEqual(config.lenFromSlider(62), 255);
assert.strictEqual(config.lenFromSlider(63), 63);
assert.strictEqual(config.lenFromSlider(0), 0);
assert.strictEqual(config.lenFromSlider(200), 200);
assert.strictEqual(config.lenFromTyped('', 80), 80);
assert.strictEqual(config.lenFromTyped('abc', 80), 80);
assert.strictEqual(config.lenFromTyped('-3', 80), 0);
assert.strictEqual(config.lenFromTyped('25', 80), 200);
assert.strictEqual(config.lenFromTyped('6.04', 80), 60);   // typed values never snap
assert.strictEqual(config.lenFromTyped('12.35', 80), 124);
assert.strictEqual(config.lenLabel(255), 'About 6 s (recommended)');
assert.strictEqual(config.lenLabel(0), 'Instant');
assert.strictEqual(config.lenLabel(125), '12.5 s');

// Page wiring: slider <-> number box, label, disabled in Battery saver
var lp2 = runPage(cfg(), envE);
assert(lp2.page.indexOf('id="animLenR"') > 0 && lp2.page.indexOf('id="animLenN"') > 0, 'slider and number box');
assert(lp2.page.indexOf('Recommended') > 0, 'notch label');
assert.strictEqual(+lp2.els.animLenR.value, 60);
lp2.els.animLenR.value = '120'; lp2.els.animLenR.oninput();
assert.strictEqual(lp2.win.CFG.animLen, 120);
assert.strictEqual(lp2.els.animLenN.value, '12.0');
lp2.els.animLenN.value = '3.3'; lp2.els.animLenN.onchange();
assert.strictEqual(lp2.win.CFG.animLen, 33);
assert.strictEqual(+lp2.els.animLenR.value, 33);
lp2.els.animLenR.value = '59'; lp2.els.animLenR.oninput();
assert.strictEqual(lp2.win.CFG.animLen, 255, 'slider snaps to recommended');
var lp3 = runPage(cfg({ animMode: 2 }), envE);
assert.strictEqual(lp3.els.animLenR.disabled, true);
assert.strictEqual(lp3.els.animLenN.disabled, true);
```

  The `runPage` stub elements are plain objects. If the harness needs `value`, `disabled`, `oninput` or `textContent` on them, extend it there. Do not change the assertions.

- [ ] **Step 2: Run it and check that it fails.**
  - Run: `node tests/pkjs/config_test.js`
  - Expected: an AssertionError at `DEFAULTS.animLen`.

- [ ] **Step 3: Write the helpers in `config.js`.** Put them above `toMessage`, and add them to `module.exports`:

```js
// JS twins of src/c/anim_len.c: len is tenths of a second (0 instant, 1..200 exact), 255 = natural.
function animTotalMs(natural, len) { return len === 255 ? natural : Math.min(len, 200) * 100; }
function animScaledMs(elapsed, natural, len) {
  if (len === 255) return elapsed;
  var total = animTotalMs(natural, len);
  return total === 0 || elapsed >= total ? natural : Math.floor(elapsed * natural / total);
}
function lenFromSlider(v) { v = Math.round(+v); return Math.abs(v - 60) <= 2 ? 255 : Math.max(0, Math.min(200, v)); }
function lenFromTyped(str, prev) {
  var s = parseFloat(str);
  if (isNaN(s)) return prev;
  return Math.max(0, Math.min(200, Math.round(s * 10)));
}
function lenLabel(len) { return len === 255 ? 'About 6 s (recommended)' : len === 0 ? 'Instant' : (len / 10).toFixed(1) + ' s'; }
```

  - Add `animLen: 255,` to `DEFAULTS`, next to `animMode`, with a comment.
  - Add `ANIM_LEN: cfg.animLen === undefined ? 255 : cfg.animLen` to `toMessage`.

- [ ] **Step 4: Add the markup and the page wiring.**
  - **Markup:** add this row immediately after the Animations `selRow`:

```js
'<div class="row" id="animLenRow"><label>Change length<br><span id="animLenLbl" style="color:var(--muted);font-size:12px"></span></label>' +
'<div class="len"><div class="lenTrack"><input type="range" id="animLenR" min="0" max="200" step="1">' +
'<div class="notch" style="left:30%"><span>Recommended</span></div></div>' +
'<input type="number" id="animLenN" min="0" max="20" step="0.1" inputmode="decimal"><span>s</span></div></div>' +
```

  - **CSS:**

```js
'.len{display:flex;align-items:center;gap:8px;}.lenTrack{position:relative;padding-bottom:16px;}' +
'.lenTrack input{width:150px;}.notch{position:absolute;top:22px;width:1px;height:6px;background:var(--muted);}' +
'.notch span{position:absolute;top:6px;left:-30px;width:60px;text-align:center;font-size:10px;color:var(--muted);}' +
'#animLenN{width:64px;font-size:14px;color:var(--ink);background:var(--panel2);border:1px solid var(--hair2);border-radius:8px;padding:6px;}' +
```

  - **Inline the helpers:** prepend them to the page script. In the `<script>` string, after `PREVIEW_JS + '\n'`, insert `[animTotalMs, animScaledMs, lenFromSlider, lenFromTyped, lenLabel].map(String).join('\n') + '\n' +`.
  - **Page JS:** add this to `PAGE_JS`, after the `bindSel`/`bindToggle` lines:

```js
"if(CFG.animLen===undefined)CFG.animLen=255;" +
"(function(){var R=document.getElementById('animLenR'),N=document.getElementById('animLenN'),L=document.getElementById('animLenLbl');" +
"window.syncLen=function(){var v=CFG.animLen;R.value=String(v===255?60:v);N.value=(v===255?6:v/10).toFixed(1);" +
"var saver=CFG.animMode===2;R.disabled=N.disabled=saver;L.textContent=saver?'Battery saver changes instantly':lenLabel(v);};" +
"R.oninput=function(){CFG.animLen=lenFromSlider(R.value);syncLen();};" +
"N.onchange=function(){CFG.animLen=lenFromTyped(N.value,CFG.animLen);syncLen();};syncLen();})();",
```

  - **Animations select:** make it re-run `syncLen()` when it changes, for the disabled state. Simplest: call `if(window.syncLen)syncLen();` inside `bindSel`'s `onchange`.
  - **Preview scaling in `frame(ts)`:**
    - Keep the natural length from the latest draw: declare `var natMs=6000;` with the other driver vars, and after each draw set `natMs=r.durationMs;`.
    - Compute `var el=animT0===null?null:ts-animT0;`.
    - Pass `animMs: el===null?null:animScaledMs(el,natMs,CFG.animLen)`.
    - End the animation when `el>=animTotalMs(natMs,CFG.animLen)`.
    - If `CFG.animLen===0`, Run draws the resting frame and ends at once.
- [ ] **Step 5: Run it and check that it passes.**
  - Run: `node tests/pkjs/config_test.js && node tests/pkjs/preview_test.js`
  - Expected: both pass. The existing animation-loop test must still pass at the default 255.
- [ ] **Step 6: Commit.**

```bash
git add src/pkjs/config.js tests/pkjs/config_test.js
git commit -m "Add a Change length slider to the settings page"
```

---

### Task 3: Location: the page search and the phone fetch

**Files:**
- Modify: `src/pkjs/config.js`, `src/pkjs/index.js`, `tests/pkjs/config_test.js`
- Create: `tests/pkjs/index_test.js`

**Interfaces:**
- Consumes: `loadCfg()` and `fetchWeather()` in `index.js`, and the page harness `runPage` in `config_test.js`.
- Produces, exported from `config.js`:
  - `geoUrl(text)`
  - `placeLabel(result)`
  - `DEFAULTS` gains `locAuto: true, locName: '', locLat: null, locLon: null`

  In `index.js`, `weatherCoords(cb)` is internal.

- [ ] **Step 1: Write the failing page tests.** Append to `tests/pkjs/config_test.js`, before the final `console.log`:

```js
assert.strictEqual(config.geoUrl('Park City'), 'https://geocoding-api.open-meteo.com/v1/search?name=Park%20City&count=5&language=en&format=json');
assert.strictEqual(config.geoUrl('a&b=c'), 'https://geocoding-api.open-meteo.com/v1/search?name=a%26b%3Dc&count=5&language=en&format=json');
assert.strictEqual(config.placeLabel({ name: 'Park City', admin1: 'Utah', country: 'United States' }), 'Park City, Utah, United States');
assert.strictEqual(config.placeLabel({ name: 'Monaco', country: 'Monaco' }), 'Monaco, Monaco');
assert.strictEqual(config.DEFAULTS.locAuto, true);
assert.strictEqual(config.DEFAULTS.locLat, null);

// Location UI: hidden when auto, shown when off; search results render as text; picking sets cfg; Save fixes up.
function xhrStub(respond) {   // respond(url) -> {status, body} | 'error'
  return function() {
    var x = this; x.open = function(m, u) { x.url = u; };
    x.send = function() { var r = respond(x.url); if (r === 'error') { x.onerror && x.onerror(); return; }
      x.status = r.status; x.responseText = r.body; x.onload && x.onload(); };
  };
}
var lp4 = runPage(cfg(), envE, { XMLHttpRequest: xhrStub(function() { return { status: 200, body: JSON.stringify({ results: [
  { name: 'Park <City>', admin1: 'Utah', country: 'United States', latitude: 40.646, longitude: -111.498 },
  { name: 'Park City', country: 'Canada', latitude: 50.1, longitude: -100.2 }] }) }; }) });
assert.strictEqual(lp4.els.locManual.style.display, 'none', 'manual location hidden while auto');
lp4.els.locAuto_off.onclick();
assert.notStrictEqual(lp4.els.locManual.style.display, 'none', 'manual location shown when off');
assert(/Pick a place/.test(lp4.els.locNote.textContent), 'warns when no place chosen');
lp4.els.locQ.value = 'Park City'; lp4.els.locFind.onclick();
var items = lp4.els.locList.children;
assert.strictEqual(items.length, 2);
assert.strictEqual(items[0].textContent, 'Park <City>, Utah, United States', 'rendered as text');
assert(!items[0].innerHTML || items[0].innerHTML.indexOf('<City>') < 0, 'not injected as HTML');
items[0].onclick();
assert.strictEqual(lp4.win.CFG.locName, 'Park <City>, Utah, United States');
assert.strictEqual(lp4.win.CFG.locLat, 40.646);
assert.strictEqual(lp4.win.CFG.locLon, -111.498);
assert(/Weather for: Park <City>/.test(lp4.els.locSel.textContent));

var lp5 = runPage(cfg(), envE, { XMLHttpRequest: xhrStub(function() { return { status: 200, body: '{}' }; }) });
lp5.els.locAuto_off.onclick(); lp5.els.locQ.value = 'zzz'; lp5.els.locFind.onclick();
assert.strictEqual(lp5.els.locStatus.textContent, 'No matches');
lp5.els.save.onclick();
var saved = JSON.parse(decodeURIComponent(lp5.win.location.href.split('#')[1]));
assert.strictEqual(saved.locAuto, true, 'off with no place saves as auto');

var lp6 = runPage(cfg(), envE, { XMLHttpRequest: xhrStub(function() { return 'error'; }) });
lp6.els.locAuto_off.onclick(); lp6.els.locQ.value = 'x'; lp6.els.locFind.onclick();
assert.strictEqual(lp6.els.locStatus.textContent, "Couldn't search — check your connection");
```

  Extend `runPage(c, env, opts)`:
  - `opts.XMLHttpRequest` is installed as a global for the page script.
  - The stub DOM must support `document.createElement(tag)`, returning `{tagName, style:{}, children:[], textContent:'', appendChild(c){this.children.push(c)}}`.
  - Container elements (`locList`) need `children` and `appendChild`, and clearing via `textContent = ''` must reset `children`.
  - Stub elements need a `textContent` field.

  Keep every existing assertion unchanged.

- [ ] **Step 2: Write the failing phone test.** Create `tests/pkjs/index_test.js`:

```js
var assert = require('assert');

function load(savedCfg, geo) {
  var handlers = {}, opened = [], sent = [], store = { cfg: savedCfg ? JSON.stringify(savedCfg) : null };
  global.localStorage = { getItem: function(k) { return k in store ? store[k] : null; }, setItem: function(k, v) { store[k] = v; } };
  global.Pebble = {
    addEventListener: function(n, f) { handlers[n] = f; },
    sendAppMessage: function(m, ok) { sent.push(m); ok && ok(); },
    openURL: function() {}, getActiveWatchInfo: function() { return { platform: 'emery' }; }
  };
  global.navigator = { geolocation: { getCurrentPosition: function(ok) { geo.calls++; ok({ coords: { latitude: 10.5, longitude: 20.25 } }); } } };
  global.XMLHttpRequest = function() { var x = this; x.open = function(m, u) { opened.push(u); }; x.send = function() {}; };
  global.setInterval = function() {};
  delete require.cache[require.resolve('../../src/pkjs/index.js')];
  require('../../src/pkjs/index.js');
  return { handlers: handlers, opened: opened, sent: sent, store: store };
}

// No saved config: geolocation.
var geo = { calls: 0 }, h = load(null, geo);
h.handlers.ready();
assert.strictEqual(geo.calls, 1);
assert(/latitude=10\.5000&longitude=20\.2500/.test(h.opened[0]), h.opened[0]);

// Old config without location fields: geolocation.
geo = { calls: 0 }; h = load({ comp1: 1, animMode: 1 }, geo);
h.handlers.ready();
assert.strictEqual(geo.calls, 1);

// Manual place: no geolocation, its coordinates in the URL.
geo = { calls: 0 }; h = load({ locAuto: false, locName: 'Park City', locLat: 40.646, locLon: -111.498 }, geo);
h.handlers.ready();
assert.strictEqual(geo.calls, 0);
assert(/latitude=40\.6460&longitude=-111\.4980/.test(h.opened[0]), h.opened[0]);

// Saving a changed location refetches at once; saving without a change does not.
geo = { calls: 0 }; h = load({ locAuto: true }, geo);
var before = h.opened.length;
h.handlers.webviewclosed({ response: encodeURIComponent(JSON.stringify({ locAuto: false, locName: 'X', locLat: 1, locLon: 2 })) });
assert.strictEqual(h.opened.length, before + 1, 'refetch after location change');
assert(/latitude=1\.0000&longitude=2\.0000/.test(h.opened[h.opened.length - 1]));
before = h.opened.length;
h.handlers.webviewclosed({ response: encodeURIComponent(JSON.stringify({ locAuto: false, locName: 'X', locLat: 1, locLon: 2 })) });
assert.strictEqual(h.opened.length, before, 'no refetch when location unchanged');

console.log('index tests passed');
```

- [ ] **Step 3: Run both and check that they fail.**
  - Run: `node tests/pkjs/config_test.js; node tests/pkjs/index_test.js`
  - Expected: both fail, the first with `config.geoUrl is not a function` and the second at the manual-place assert.

- [ ] **Step 4: Implement it in `config.js`.**
  - **Helpers:** add these and export them:

```js
function geoUrl(text) {
  return 'https://geocoding-api.open-meteo.com/v1/search?name=' + encodeURIComponent(text) + '&count=5&language=en&format=json';
}
function placeLabel(r) { return [r.name, r.admin1, r.country].filter(function(s) { return s; }).join(', '); }
```

  - **`DEFAULTS`:** add `locAuto: true, locName: '', locLat: null, locLon: null`.
  - **Markup:** add a section immediately after the Display section's `</section>`:

```js
'<section><h2>Location</h2>' + toggleRow('Use my location', 'locAuto') +
'<p id="locNote" style="color:var(--muted);font-size:12px;margin:0"></p>' +
'<div id="locManual"><div class="row"><input type="text" id="locQ" placeholder="Town or city" ' +
'style="flex:1;font-size:14px;color:var(--ink);background:var(--panel2);border:1px solid var(--hair2);border-radius:8px;padding:8px 10px">' +
'<button class="runbtn" id="locFind">Find</button></div>' +
'<p id="locStatus" style="color:var(--muted);font-size:13px;margin:4px 0"></p><div id="locList"></div>' +
'<p id="locSel" style="font-size:13px;margin:8px 0 0"></p></div></section>' +
```

  - **CSS:** `'#locList button{display:block;width:100%;text-align:left;margin:4px 0;padding:9px 10px;border-radius:8px;border:1px solid var(--hair2);background:var(--panel2);color:var(--ink);font-size:14px;}'`
  - **Inline the helpers:** add `geoUrl` and `placeLabel` to the list of functions inlined with `.map(String)`.
  - **Page JS:**
    - Add `show('locManual',!CFG.locAuto)` to `syncRows`, plus a call to `syncLoc()` guarded by `if(window.syncLoc)`.
    - Add `bindToggle('locAuto')` to the toggle list.
    - Add:

```js
"if(CFG.locAuto===undefined)CFG.locAuto=true;" +
"window.syncLoc=function(){var has=typeof CFG.locLat==='number';" +
"document.getElementById('locSel').textContent=has?'Weather for: '+CFG.locName:'No place chosen yet';" +
"document.getElementById('locNote').textContent=!CFG.locAuto&&!has?'Pick a place, or your phone\\'s location will be used.':'';};" +
"(function(){var Q=document.getElementById('locQ'),F=document.getElementById('locFind'),S=document.getElementById('locStatus'),Li=document.getElementById('locList');" +
"function find(){var q=(Q.value||'').trim();if(!q)return;S.textContent='Searching\\u2026';Li.textContent='';" +
"var x=new XMLHttpRequest();x.open('GET',geoUrl(q),true);" +
"x.onload=function(){var rs=[];try{rs=(JSON.parse(x.responseText).results)||[];}catch(e){}" +
"if(x.status<200||x.status>=300){S.textContent='Couldn\\'t search \\u2014 check your connection';return;}" +
"S.textContent=rs.length?'':'No matches';rs.slice(0,5).forEach(function(r){var b=document.createElement('button');" +
"b.textContent=placeLabel(r);b.onclick=function(){CFG.locName=placeLabel(r);CFG.locLat=r.latitude;CFG.locLon=r.longitude;syncLoc();};Li.appendChild(b);});};" +
"x.onerror=function(){S.textContent='Couldn\\'t search \\u2014 check your connection';};x.send();}" +
"F.onclick=find;Q.onkeydown=function(e){if(e&&e.key==='Enter')find();};syncLoc();})();",
```

  - **Save handler:** the first line of `PAGE_JS` serialises `CFG`. Before it serialises, add `if(!CFG.locAuto&&typeof CFG.locLat!=='number')CFG.locAuto=true;`.

- [ ] **Step 5: Implement it in `index.js`.**
  - Move the URL building and the request out of the geolocation callback into `requestWeather(lat, lon)`. It keeps the existing body, with `var lat = (+lat).toFixed(4), lon = (+lon).toFixed(4);`.
  - Add:

```js
// Weather location: a place chosen on the settings page, else the phone's own position.
function weatherCoords(cb) {
  var cfg = loadCfg();
  if (!cfg.locAuto && typeof cfg.locLat === 'number' && typeof cfg.locLon === 'number') { cb(cfg.locLat, cfg.locLon); return; }
  navigator.geolocation.getCurrentPosition(
    function(pos) { cb(pos.coords.latitude, pos.coords.longitude); },
    function(err) { logErr('geo', err); },
    { timeout: REQUEST_TIMEOUT_MS, maximumAge: 10 * 60 * 1000 });
}
function fetchWeather() { weatherCoords(requestWeather); }
```

  - **`webviewclosed`:** before `localStorage.setItem('cfg', ...)`, read the previous config with `loadCfg()`. After saving and sending, call `fetchWeather()` if `prev.locAuto !== cfg.locAuto || prev.locLat !== cfg.locLat || prev.locLon !== cfg.locLon`.
- [ ] **Step 6: Run the tests and check that they pass.**
  - Run: `node tests/pkjs/config_test.js && node tests/pkjs/preview_test.js && node tests/pkjs/index_test.js`, then `pebble build 2>&1 | tail -3`.
  - Expected: all pass, and the page length stays under 120 000 (printed by `config_test`).
- [ ] **Step 7: Commit.**

```bash
git add src/pkjs/config.js src/pkjs/index.js tests/pkjs/config_test.js tests/pkjs/index_test.js
git commit -m "Let the weather follow a chosen place"
```

---

### Task 4: Live check and README

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update the README.** Describe the two settings in its settings section:
  - **Location:** "Use my location", or search for a town and pick it. The weather and sun times follow the chosen place.
  - **Change length:** 0–20 s; Recommended keeps the natural length; 0 is instant.
- [ ] **Step 2: Live timing on Emery.** These builds are not committed.
  1. For each of `anim_len` 20 (2.0 s), 120 (12.0 s) and 0 (instant), temporarily change `.anim_len = 255` in `settings.c` `DEFAULTS` to that value.
  2. Run `pebble kill`, `pebble wipe`, `pebble clean`, then `pebble build`.
  3. Run `pebble install --emulator emery`, then start `pebble logs --emulator emery > <log> &` in the background.
  4. Wait for one natural minute change, plus 25 s.
  5. The log line `heap free at rest` prints when a change ends. Its timestamp minus the minute boundary (hh:mm:00) gives the length.
     - Expected: about 2–3 s for 20, and about 12–13 s for 120. At 0 there is no change animation; the time swaps at the minute and the rest line may not print, so take a screenshot at hh:mm:01 instead.
  6. Revert `settings.c` with `git checkout src/c/settings.c`.
  7. Also check the replay when the face opens at 120: the install launches the face, and a screenshot at about 7 s should still show the avalanche.
- [ ] **Step 3: Basalt check.** Run a plain build on basalt, then check one live minute change and the `heap free at rest` value, which must be at least 2720.
- [ ] **Step 4: Rebuild and run the tests.** Run `pebble clean; pebble build` so `build/` is a plain build. Run all tests: `make -C tests`, then the three node tests.
- [ ] **Step 5: Commit.**

```bash
git add README.md
git commit -m "Document the location and change length settings"
```
