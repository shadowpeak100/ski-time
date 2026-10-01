# Settings Preview and Ko-fi Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Ko-fi links (a header pill and a bottom Support card) and a live canvas preview with a Run-animation button to the Ski Time settings page.

**Architecture:**
- The page is still one generated HTML string (`config.buildHtml`). It now takes an `env` object (platform, last weather, sun times) that `index.js` caches in localStorage.
- A new `src/pkjs/preview.js` exports `PREVIEW_JS`, a string of in-page drawing code ported from `docs/prototypes/weather.html` and brought in line with the current C. The page inlines it and calls `SkiPreview.draw(canvas, cfg, env, opts)`.
- Node tests run `buildHtml` output against a stub DOM and canvas. Visual checks render the same code with `@napi-rs/canvas` in the scratchpad and compare it with emulator screenshots.

**Tech Stack:** PebbleKit JS (CommonJS `require` inside `src/pkjs`), in-page ES2015 plus canvas 2D, node for tests, `@napi-rs/canvas` (scratchpad only, not a project dependency), the Pebble SDK emulator for reference screenshots.

**Spec:** `docs/superpowers/specs/2026-10-01-config-preview-design.md`

## Global Constraints

- Ko-fi link: `https://ko-fi.com/shadowpeak100`, opened with `target="_blank" rel="noopener"`.
- The header pill copies pebble-fit's `.kofi` style: background `#ff5e5b`, white text, the cup SVG, the label "Ko-fi".
- The Support card has the button "☕ Buy me a coffee on Ko-fi" and the text "Ski Time is free, with no ads. If it's useful to you, you can support development. Thank you!"
- The preview canvas shows the watch's real pixel size, scaled 2× with `image-rendering: pixelated`. Emery is 200×228 and basalt is 144×168.
- Basalt previews draw no weather.
- Fallbacks when nothing is cached: weather clear (WMO 0), wind 0, sunrise 420 (07:00), sunset 1140 (19:00), platform emery.
- Sample complication values: 61°, the real date, the seconds `:38`, 8,412 steps, 72 bpm, 80% battery.
- Animation timings must match the C:
  - crack at 450 ms, drawn over 500 ms with ease-out across the on-slope part of the line
  - flow 1000–2800 ms, fade 450 ms
  - skiers start drawing at 2600 ms
  - stagger 200 ms, pair offset 150 ms, stop 300 ms
- Where the prototype and the current C differ, the C wins. The C is what the watch shows.
- The generated page (`buildHtml(...)` length) must stay under 120 000 characters, before URI encoding.
- No external scripts or stylesheets in the page.
- Settings message keys, `toMessage` output and the watch C code do not change.

## Review Focus

1. **Settings saved by an older version.** A saved cfg with no `dayCycle`, `wxFixed` or `dayFixed`, or with `dayMode: 2`, must still render without throwing. Task 4's stub-DOM test feeds such a cfg.
2. **Cached weather that is junk.** A `wx` cache holding a non-number or an out-of-range WMO, such as 150 or "abc", must render as clear, not throw. Task 2's test covers this.
3. **Pressing Run twice, or changing a setting mid-animation.** There must be no second animation loop: one `requestAnimationFrame` chain only. The next frame must use the new settings. Task 4's test counts the RAF registrations.
4. **12-hour time at midnight and noon, with and without a leading zero.** The preview digits must match the watch: `12:05` or `0:05`, following the same rules as `src/c/` (see `digits`/`skier` slot code). Task 3's test covers `fmtTime`.
5. **Fixed times where sunset comes before sunrise.** The time of day must still be picked as the watch does, using wrap-aware `daypart_at`. Task 3's test covers this.

---

## File Structure

- **Create `src/pkjs/preview.js`:** `module.exports = { PREVIEW_JS: '<string>' }`. The string defines a global `SkiPreview` with `draw(canvas, cfg, env, opts)`, `daypartFor(cfg, env, minuteOfDay)`, `weatherFor(cfg, env)` and `fmtTime(cfg, h, m)`.
- **Modify `src/pkjs/config.js`:**
  - add Ko-fi markup and CSS
  - add the preview canvas and the Run button
  - `buildHtml(cfg, env)` inlines `PREVIEW_JS` and the preview wiring
- **Modify `src/pkjs/index.js`:**
  - cache `wx`, `wind`, `sunrise` and `sunset` in localStorage when they are sent
  - build `env` (adding the platform from `Pebble.getActiveWatchInfo()`)
  - pass `env` to `buildHtml`
- **Create `tests/pkjs/config_test.js`:** plain node assertions, run with `node tests/pkjs/config_test.js`.
- **Create `tests/pkjs/stub_dom.js`:** a minimal DOM and canvas stub, shared by the tests.
- **Modify `README.md`:** one line under settings.

---

### Task 1: Ko-fi pill and Support card

**Files:**
- Modify: `src/pkjs/config.js` (`buildHtml` CSS and markup)
- Create: `tests/pkjs/config_test.js`

**Interfaces:**
- Consumes: the existing `config.buildHtml(cfg)`.
- Produces: `buildHtml(cfg, env)`. In this task `env` is accepted and ignored. The test file `tests/pkjs/config_test.js` exports nothing, prints `config tests passed`, and exits non-zero on failure. Later tasks add checks to it.

- [ ] **Step 1: Write the failing test.** Create `tests/pkjs/config_test.js`:

```js
var assert = require('assert');
var config = require('../../src/pkjs/config.js');

function cfg() { var c = {}, k; for (k in config.DEFAULTS) c[k] = config.DEFAULTS[k]; return c; }

var html = config.buildHtml(cfg(), {});
assert(html.indexOf('class="kofi"') > 0, 'header Ko-fi pill');
assert(html.indexOf('href="https://ko-fi.com/shadowpeak100"') > 0, 'Ko-fi link');
assert(html.split('https://ko-fi.com/shadowpeak100').length - 1 === 2, 'Ko-fi link top and bottom');
assert(html.indexOf('Buy me a coffee on Ko-fi') > 0, 'Support card button');
assert(html.indexOf('Ski Time is free, with no ads.') > 0, 'Support card text');
assert(html.indexOf('<h2>Support</h2>') < html.indexOf('id="save"'), 'Support card above the Save bar');

console.log('config tests passed');
```

- [ ] **Step 2: Run it and check that it fails.**
  - Run: `node tests/pkjs/config_test.js`
  - Expected: an AssertionError "header Ko-fi pill".

- [ ] **Step 3: Implement it in `src/pkjs/config.js`.**
  - Change the signature to `function buildHtml(cfg, env) {`.
  - Add to the CSS string, before `</style>`:

```js
'.hdr{display:flex;align-items:center;justify-content:space-between;margin:6px 0 14px;}.hdr h1{margin:0;}' +
'.kofi{display:flex;align-items:center;gap:5px;background:#ff5e5b;color:#fff;text-decoration:none;' +
'padding:6px 10px;border-radius:14px;font-size:13px;font-weight:bold;white-space:nowrap;}' +
'.kofibtn{display:block;text-align:center;padding:12px;border-radius:10px;background:#ff5e5b;color:#fff;' +
'text-decoration:none;font-weight:600;font-size:15px;}' +
```

  - Replace `'<h1>Ski Time</h1>'` with:

```js
'<div class="hdr"><h1>Ski Time</h1>' +
'<a class="kofi" href="https://ko-fi.com/shadowpeak100" target="_blank" rel="noopener" title="Support Ski Time on Ko-fi">' +
'<svg width="17" height="17" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">' +
'<path d="M4 8h11v6a4 4 0 0 1-4 4H8a4 4 0 0 1-4-4V8z"/><path d="M15 9h2.5a2.5 2.5 0 0 1 0 5H15"/><path d="M5 21h11"/></svg>Ko-fi</a></div>' +
```

  - Immediately before `'</div><div class="bar">'`, after the Battery saver section's closing `</section>`, insert:

```js
'<section><h2>Support</h2><p style="color:var(--muted);font-size:13px;margin:0 0 10px">' +
'Ski Time is free, with no ads. If it\'s useful to you, you can support development. Thank you!</p>' +
'<a class="kofibtn" href="https://ko-fi.com/shadowpeak100" target="_blank" rel="noopener">&#9749; Buy me a coffee on Ko-fi</a></section>' +
```

  - **Note:** `☕` is written as `&#9749;`, so the test's "Buy me a coffee on Ko-fi" check still matches.

- [ ] **Step 4: Run it and check that it passes.**
  - Run: `node tests/pkjs/config_test.js`
  - Expected: `config tests passed`.

- [ ] **Step 5: Commit.**

```bash
git add src/pkjs/config.js tests/pkjs/config_test.js
git commit -m "Add Ko-fi links to the settings page"
```

---

### Task 2: Cache the preview data on the phone

**Files:**
- Modify: `src/pkjs/index.js`
- Modify: `tests/pkjs/config_test.js`

**Interfaces:**
- Consumes: `buildHtml(cfg, env)` from Task 1.
- Produces: `index.js` exports nothing new. A new pure helper goes in `config.js`:
  - `config.envFrom(stored, platform)` takes the parsed localStorage object `{wx, wind, rise, set}` (any field may be missing or junk) and a platform string.
  - It returns `{platform: 'emery'|'basalt', wmo: int 0..99, wind: -1|0|1, rise: 0..1439, set: 0..1439}`, with fallbacks wmo 0, wind 0, rise 420, set 1140.
  - Any platform other than `'basalt'` gives `'emery'`.

- [ ] **Step 1: Write the failing test.** Append to `tests/pkjs/config_test.js`, before the final `console.log`:

```js
var e = config.envFrom({}, undefined);
assert.deepStrictEqual(e, { platform: 'emery', wmo: 0, wind: 0, rise: 420, set: 1140 });
e = config.envFrom({ wx: 61, wind: -1, rise: 400, set: 1200 }, 'basalt');
assert.deepStrictEqual(e, { platform: 'basalt', wmo: 61, wind: -1, rise: 400, set: 1200 });
e = config.envFrom({ wx: 150, wind: 7, rise: 'x', set: 2000 }, 'chalk');
assert.deepStrictEqual(e, { platform: 'emery', wmo: 0, wind: 0, rise: 420, set: 1140 });
e = config.envFrom({ wx: 'abc', wind: null }, 'emery');
assert.strictEqual(e.wmo, 0); assert.strictEqual(e.wind, 0);
```

- [ ] **Step 2: Run it and check that it fails.**
  - Run: `node tests/pkjs/config_test.js`
  - Expected: a TypeError, because `config.envFrom` is not a function.

- [ ] **Step 3: Implement `envFrom` in `src/pkjs/config.js`.** Add it above `module.exports`, and add `envFrom: envFrom` to the exports:

```js
// Preview inputs from the phone's cache; junk or missing values fall back to clear, calm, 07:00-19:00.
function envFrom(stored, platform) {
  function int(v, lo, hi, dflt) { return typeof v === 'number' && v % 1 === 0 && v >= lo && v <= hi ? v : dflt; }
  stored = stored || {};
  return {
    platform: platform === 'basalt' ? 'basalt' : 'emery',
    wmo: int(stored.wx, 0, 99, 0),
    wind: int(stored.wind, -1, 1, 0),
    rise: int(stored.rise, 0, 1439, 420),
    set: int(stored.set, 0, 1439, 1140)
  };
}
```

- [ ] **Step 4: Wire up `index.js`.**
  - In `fetchWeather`'s `onload`, immediately before `sendWeather(msg);`, insert:

```js
            cachePreview(msg);
```

  - Above `fetchWeather`, add:

```js
// The settings preview shows the last weather and sun times the watch was sent.
function cachePreview(msg) {
  try {
    var p = JSON.parse(localStorage.getItem('preview') || '{}');
    if ('WX_CODE' in msg) p.wx = msg.WX_CODE;
    if ('WIND_X' in msg) p.wind = msg.WIND_X;
    if ('SUNRISE' in msg) { p.rise = msg.SUNRISE; p.set = msg.SUNSET; }
    localStorage.setItem('preview', JSON.stringify(p));
  } catch (e) { logErr('cachePreview', e); }
}

function previewEnv() {
  var stored = {}, platform;
  try { stored = JSON.parse(localStorage.getItem('preview') || '{}'); } catch (e) { logErr('previewEnv', e); }
  try { var info = Pebble.getActiveWatchInfo && Pebble.getActiveWatchInfo(); platform = info && info.platform; }
  catch (e) { logErr('watchInfo', e); }
  return config.envFrom(stored, platform);
}
```

  - In the `showConfiguration` handler, change `config.buildHtml(loadCfg())` to `config.buildHtml(loadCfg(), previewEnv())`.

- [ ] **Step 5: Run it and check that it passes.**
  - Run: `node tests/pkjs/config_test.js`
  - Expected: `config tests passed`.
  - Run: `pebble build 2>&1 | tail -5`
  - Expected: `'build' finished successfully`.

- [ ] **Step 6: Commit.**

```bash
git add src/pkjs/index.js src/pkjs/config.js tests/pkjs/config_test.js
git commit -m "Cache weather and sun times for the settings preview"
```

---

### Task 3: Preview renderer (`preview.js`)

This is the port. It is the largest task, and it needs judgment: compare the prototype with the C and pick the C where they differ.

**Files:**
- Create: `src/pkjs/preview.js`
- Create: `tests/pkjs/stub_dom.js`
- Create: `tests/pkjs/preview_test.js`
- Reference (read only):
  - `docs/prototypes/weather.html`, script at lines 123–687
  - `src/c/layout.c`, `scene.c`, `theme.c`, `weather.c`, `bolt.c`, `wx_draw.c`, `avalanche.c`/`.h`, `aval_draw.c`, `skier.c`/`.h`, `digits*`, `comps.c`, `daypart.c`

**Interfaces:**
- Consumes: nothing from Tasks 1–2 at runtime. The `env` shape from Task 2 is `{platform, wmo, wind, rise, set}`. The `cfg` shape is `config.DEFAULTS`: `comp1..3`, `use24`, `lead0`, `tracks`, `dayCycle`, `dayMode`, `fixedRise`, `fixedSet`, `dayFixed`, `animMode`, `weatherOn`, `wxFixed`, saver fields.
- Produces: `require('./preview').PREVIEW_JS`, a string. Evaluated in a page, it defines the global `SkiPreview` with:
  - `SkiPreview.daypartFor(cfg, env, minuteOfDay)` returns 0 sunrise, 1 midday, 2 sunset or 3 night. It is the same logic as `daypart_for` in `src/c/daypart.c`:
    - `dayCycle` false gives `dayFixed`, with values outside 0..3 giving 1
    - `dayMode` 1 uses `fixedRise`/`fixedSet`
    - otherwise it uses `env.rise`/`env.set`
    - a legacy `dayMode === 2` is treated as static midday
  - `SkiPreview.weatherFor(cfg, env)` returns a WxKind 0..7:
    - basalt gives 0
    - `weatherOn` gives the result of the port of `wx_from_wmo(env.wmo)`
    - otherwise `wxFixed`, with values outside 0..7 giving 0
  - `SkiPreview.fmtTime(cfg, h, m)` returns the four digit characters the watch draws, as a string such as `'1205'` or `' 905'`. It must match the C's rule for `time_24h` and `lead_zero` exactly; read it from the C.
  - `SkiPreview.draw(canvas, cfg, env, opts)`:
    - `opts = { now: Date, animMs: number|null, wxClock: seconds }`
    - It sets `canvas.width`/`height` to the platform size and draws one full frame.
    - `animMs` null means the resting frame, with the time drawn and the skiers stopped.
    - A number means the minute-change frame at that elapsed ms. Old time = `now` minus 1 minute, new time = `now`.
    - In Battery saver (`animMode === 2`), any `animMs` draws the resting frame.
    - `wxClock` drives falling precipitation and the bolt flicker when `animMode === 0`. Otherwise the weather is drawn still, using the C's still counts and positions.
    - It returns `{ durationMs }`, the length of the minute change for this frame's skier set (the port of `skiers_duration`).

- [ ] **Step 1: Write the stub DOM and canvas.** Create `tests/pkjs/stub_dom.js`:

```js
// Minimal DOM + canvas 2D stub: every context method is a no-op recorder, so page code can run in node.
function makeCtx() {
  var calls = 0;
  var target = { canvas: null, calls: function() { return calls; } };
  return new Proxy(target, {
    get: function(t, k) {
      if (k in t) return t[k];
      if (k === 'getImageData' || k === 'createImageData')
        return function(x, y, w, h) { calls++; return { width: w || 1, height: h || 1, data: new Uint8ClampedArray(4 * (w || 1) * (h || 1)) }; };
      if (k === 'measureText') return function() { calls++; return { width: 10 }; };
      return function() { calls++; };
    },
    set: function(t, k, v) { t[k] = v; return true; }
  });
}
function makeCanvas() {
  var c = { width: 300, height: 150, style: {}, attrs: {} };
  var ctx = makeCtx(); ctx.canvas = c;
  c.getContext = function() { return ctx; };
  c.setAttribute = function(k, v) { this.attrs[k] = v; };
  return c;
}
module.exports = { makeCanvas: makeCanvas };
```

- [ ] **Step 2: Write the failing test.** Create `tests/pkjs/preview_test.js`:

```js
var assert = require('assert');
var config = require('../../src/pkjs/config.js');
var PREVIEW_JS = require('../../src/pkjs/preview.js').PREVIEW_JS;
var makeCanvas = require('./stub_dom.js').makeCanvas;

var sandbox = {};
new Function('window', PREVIEW_JS + ';window.SkiPreview=SkiPreview;')(sandbox);
var P = sandbox.SkiPreview;
function cfg(over) { var c = {}, k; for (k in config.DEFAULTS) c[k] = config.DEFAULTS[k]; for (k in over || {}) c[k] = over[k]; return c; }
var env = { platform: 'emery', wmo: 0, wind: 0, rise: 420, set: 1140 };

// daypartFor mirrors src/c/daypart.c
assert.strictEqual(P.daypartFor(cfg({ dayCycle: false, dayFixed: 3 }), env, 720), 3);
assert.strictEqual(P.daypartFor(cfg({ dayCycle: false, dayFixed: 9 }), env, 720), 1);
assert.strictEqual(P.daypartFor(cfg({ dayMode: 2 }), env, 0), 1);                         // legacy "Always midday"
assert.strictEqual(P.daypartFor(cfg(), env, 720), 1);
assert.strictEqual(P.daypartFor(cfg(), env, 0), 3);
assert.strictEqual(P.daypartFor(cfg({ dayMode: 1, fixedRise: 1200, fixedSet: 300 }), env, 0), 1);   // sunset before sunrise: wrap-aware

// weatherFor
assert.strictEqual(P.weatherFor(cfg({ weatherOn: true }), { platform: 'emery', wmo: 63 }), 5);    // rain
assert.strictEqual(P.weatherFor(cfg({ weatherOn: false, wxFixed: 6 }), env), 6);
assert.strictEqual(P.weatherFor(cfg({ weatherOn: false, wxFixed: 6 }), { platform: 'basalt' }), 0);
assert.strictEqual(P.weatherFor(cfg({ weatherOn: false, wxFixed: 42 }), env), 0);

// fmtTime: noon, midnight, morning, with and without 24h / leading zero
[[true, true], [true, false], [false, true], [false, false]].forEach(function(f) {
  var c = cfg({ use24: f[0], lead0: f[1] });
  [[0, 5], [12, 5], [9, 5], [23, 59]].forEach(function(t) {
    var s = P.fmtTime(c, t[0], t[1]);
    assert.strictEqual(s.length, 4, 'four digit cells ' + JSON.stringify([f, t, s]));
  });
});
assert.strictEqual(P.fmtTime(cfg({ use24: true, lead0: true }), 9, 5), '0905');
assert.strictEqual(P.fmtTime(cfg({ use24: false, lead0: true }), 0, 5), '1205');

// draw: every daypart x weather x platform x anim mode, resting and mid-animation, without throwing
var now = new Date(2026, 9, 1, 12, 30, 38);
['emery', 'basalt'].forEach(function(platform) {
  for (var dp = 0; dp < 4; dp++) for (var wx = 0; wx < 8; wx++) for (var am = 0; am < 3; am++) {
    var c = cfg({ dayCycle: false, dayFixed: dp, weatherOn: false, wxFixed: wx, animMode: am });
    var e = { platform: platform, wmo: 0, wind: 1, rise: 420, set: 1140 };
    [null, 0, 450, 700, 1500, 2600, 4000, 9000].forEach(function(ms) {
      var cv = makeCanvas();
      var r = P.draw(cv, c, e, { now: now, animMs: ms, wxClock: 3.7 });
      assert.strictEqual(cv.width, platform === 'basalt' ? 144 : 200);
      assert.strictEqual(cv.height, platform === 'basalt' ? 168 : 228);
      assert(r && r.durationMs >= 4500 && r.durationMs <= 7500, 'duration ' + (r && r.durationMs));
      assert(cv.getContext('2d').calls() > 100, 'drew something');
    });
  }
});

console.log('preview tests passed');
```

  - Change the expected `fmtTime` strings, the duration bounds (they come from `tests/host_tests.c` `check_set`, which allows 4500–7500) and the wrap case, **only** if the C gives a different answer. In that case put the C's answer in the test and add a comment naming the C line.

- [ ] **Step 3: Run it and check that it fails.**
  - Run: `node tests/pkjs/preview_test.js`
  - Expected: `Cannot find module '../../src/pkjs/preview.js'`.

- [ ] **Step 4: Port the renderer into `src/pkjs/preview.js`.**
  - **File shape:** `var PREVIEW_JS = [ ...lines... ].join('\n'); module.exports = { PREVIEW_JS: PREVIEW_JS };`. Keep it as a joined array of string lines, matching `config.js` `PAGE_JS`. Alternatively, write one function and export it as `'(' + fn.toString() + ')();'`; both are fine. The **`fn.toString()` form is recommended**: it keeps the code readable and lets node syntax-check it.
  - **What to take from `docs/prototypes/weather.html`:** the colour tables (`C`, `THEMES`, `WEATHER`, `WX_*`), geometry, `makeLayout`, the background build (sky, peaks, clouds, deck, fog, trees, chalet, campfire, floods), skiers, the avalanche, `drawWeather`, `boltPath`/`drawBolt`, `drawComps` and `render`.
  - **What to drop:** controls, the grid, the storyboard, `tick`/RAF boot, `PLATFORMS[].label`/`z`, every `$()` DOM helper, and the alternative `SNOW_STYLE` branches (keep look D, `'mixed'`).
  - **Wrap the result** as `var SkiPreview = (function(){ ...; return { draw, daypartFor, weatherFor, fmtTime }; })();`.
- [ ] **Step 5: Bring the port in line with the current C.** Go through this checklist. For each item, read the C named, compare it with the prototype code and change the port to match the C. Record each change in the task report as `item: prototype X -> C Y`.
  1. **Timings, `avalanche.h` and `skier.h`:** `AV_CRACK_MS 450`, `AV_CRACK_DRAW_MS 500`, `AV_FLOW_START_MS 1000`, `AV_FLOW_END_MS 2800`, `AV_FADE_MS 450`, `SKI_DRAW_START_MS 2600`, `SKI_STAGGER_MS 200`, `SKI_PAIR_MS 150`, `SKI_STOP_MS 300`, `SKI_EXIT_GAP_MS 30`.
  2. **The crack, `aval_draw.c` `draw_crack`:** it sweeps from the low-`s` end over the on-slope span `[lo, hi]` with `grow = 1-(1-g)^2`. It is visible from `AV_CRACK_MS` until `AV_FLOW_START_MS + 150`.
  3. **Slope trees, `scene.c` (the tree table near `SLOPE_TREES`/`pine` call sites at about lines 309, 422, 455 and 536):** use the C's tree list and order, including clusters, ridge-line trees and the chalet tree. Replace the prototype's `SLOPE_TREES` with the C table.
  4. **Hillside, `layout.c`:** the ridge, the berm, the time block centred on the screen, the chalet rect and `u0`/`s0`/`u1`. Check every constant.
  5. **Peaks and far forest, `scene.c` (about lines 400–417):** the `far`/`near` peak tables, the `split` at `hw*0.12`, the cap polygons and the forest-triangle loop (`Mulberry r = {7}`).
  6. **Themes, `theme.c`:** every role colour for all four dayparts.
  7. **Weather, `weather.c`, `wx_draw.c` and `bolt.c`:**
     - the WMO mapping and per-weather counts
     - the still multipliers: rain ×1.7, drizzle ×2, snow ×1.5
     - the integer hash positions (the C uses `hsh`/`mix`, not the prototype's sin-hash, so port `mix`/`hsh` exactly so still drops land where the watch puts them)
     - the bolt `mulberry(307)` path and the flicker windows
     - the flat partly-cloudy cloud positions
     - the deck rows and fog
  8. **Night, `scene.c`:** flood heads and reach, the campfire, the stars and the moon phase (use `now` for the phase).
  9. **Digits and tracks, `skier.c`/`digits*`:** the twin vs solid track style, `fmtTime` rules and the ski-out paths.
  10. **Complications, `comps.c`:**
      - the layout and fonts (a canvas text substitute is fine)
      - the sample values from Global Constraints
      - in Battery saver the seconds slot shows `-`.

  Basalt: no weather layers, as `PBL_PLATFORM_EMERY` guards in the C.
- [ ] **Step 6: Run the test and check that it passes.**
  - Run: `node tests/pkjs/preview_test.js`
  - Expected: `preview tests passed`.
- [ ] **Step 7: Run a visual check against the emulator.** This is not committed.
  1. In the scratchpad, `npm install @napi-rs/canvas` if it is not present.
  2. Write a script that `require`s `preview.js`, evaluates `PREVIEW_JS` the same way the test does, draws onto `createCanvas(200,228)` and saves PNGs.
  3. Render these cases, each with `MOCK_TIME=12:30`, a resting frame and `wxClock` 3.7:
     - midday clear
     - night storm (Balanced)
     - sunset snow
     - basalt midday
  4. Build the same four cases on the emulator with `pebble clean` before each build:
     - `MOCK_TIME=12:30 MOCK_DAYPART=1 MOCK_WEATHER=0 pebble build`, installed with `pebble install --emulator emery`, then `pebble screenshot --emulator emery --no-open <file>`
     - the same for daypart 3 / weather 7, daypart 2 / weather 6, and basalt daypart 1
  5. Put the preview and the emulator shot side by side for each case, and look at them.
  6. Fix anything missing or extra. Small pixel offsets are acceptable.
  7. If the emulator hangs, run `pebble kill` then `pebble wipe`.
  8. List the image paths in the report.
- [ ] **Step 8: Commit.**

```bash
git add src/pkjs/preview.js tests/pkjs/stub_dom.js tests/pkjs/preview_test.js
git commit -m "Port the face renderer for the settings preview"
```

---

### Task 4: Wire the preview into the settings page

**Files:**
- Modify: `src/pkjs/config.js`
- Modify: `tests/pkjs/config_test.js`
- Modify: `tests/pkjs/stub_dom.js` (if more DOM is needed)

**Interfaces:**
- Consumes:
  - `PREVIEW_JS` and `SkiPreview.draw` from Task 3
  - `buildHtml(cfg, env)` and `envFrom` from Tasks 1–2
- Produces: the page has
  - `<canvas id="pv">` inside `<div class="preview">`, placed after the header
  - `<button id="run">▶ Run animation</button>`
  - a global `redraw()`, called by every `bindSel`/`bindToggle`/time-input change

- [ ] **Step 1: Write the failing test.** Append to `tests/pkjs/config_test.js`, before the final `console.log`:

```js
var makeCanvas = require('./stub_dom.js').makeCanvas;

function runPage(c, env) {
  var page = config.buildHtml(c, env);
  var els = {}, raf = [], ids = page.match(/id="([^"]+)"/g).map(function(s) { return s.slice(4, -1); });
  ids.forEach(function(id) {
    els[id] = id === 'pv' ? makeCanvas() : { id: id, style: {}, value: '', setAttribute: function() {}, addEventListener: function() {} };
  });
  var win = {
    document: { getElementById: function(id) { if (!els[id]) throw new Error('missing #' + id); return els[id]; } },
    location: {},
    requestAnimationFrame: function(f) { raf.push(f); return raf.length; },
    cancelAnimationFrame: function() {},
    performance: { now: function() { return 0; } },
    Date: Date
  };
  var js = page.match(/<script>([\s\S]*)<\/script>/)[1];
  new Function('window', 'document', 'location', 'requestAnimationFrame', 'cancelAnimationFrame', 'performance',
    js + ';window.CFG=CFG;')(win, win.document, win.location, win.requestAnimationFrame, win.cancelAnimationFrame, win.performance);
  return { els: els, raf: raf, win: win, page: page };
}

var envE = config.envFrom({}, 'emery');
var r = runPage(cfg(), envE);
assert(r.page.indexOf('<canvas id="pv"') > 0, 'preview canvas');
assert(r.page.indexOf('class="preview"') > r.page.indexOf('class="hdr"'), 'preview under the header');
assert(r.page.length < 120000, 'page size ' + r.page.length);
assert(r.els.pv.width === 200, 'emery preview size');
assert(r.els.pv.getContext('2d').calls() > 100, 'preview drawn on load');

// Run twice: still only one RAF chain pending.
r.els.run.onclick(); r.els.run.onclick();
assert.strictEqual(r.raf.length, 1, 'one animation loop after double Run, got ' + r.raf.length);

// Old saved settings (no new fields, legacy dayMode 2) still render.
var old = { comp1: 1, comp2: 2, comp3: 0, use24: true, lead0: true, tracks: 0, saverOn: false, saverStart: 1320,
            saverEnd: 420, dayMode: 2, fixedRise: 420, fixedSet: 1140, animMode: 1, weatherOn: false };
runPage(old, config.envFrom({ wx: 'abc' }, 'basalt'));

// Every daypart x weather on a basalt and an emery page.
['emery', 'basalt'].forEach(function(p) {
  for (var dp = 0; dp < 4; dp++) for (var wx = 0; wx < 8; wx++)
    runPage(cfg({ dayCycle: false, dayFixed: dp, weatherOn: false, wxFixed: wx }), config.envFrom({}, p));
});
```

- [ ] **Step 2: Run it and check that it fails.**
  - Run: `node tests/pkjs/config_test.js`
  - Expected: `AssertionError: preview canvas`.

- [ ] **Step 3: Implement it in `src/pkjs/config.js`.**
  - Add `var PREVIEW_JS = require('./preview').PREVIEW_JS;` at the top.
  - Add this CSS:

```js
'.preview{position:sticky;top:0;z-index:5;background:var(--bg);padding:8px 0 10px;display:flex;flex-direction:column;align-items:center;gap:8px;}' +
'#pv{image-rendering:pixelated;image-rendering:crisp-edges;border-radius:12px;background:#000;}' +
'.runbtn{padding:8px 14px;border-radius:8px;border:1px solid var(--hair2);background:var(--panel2);color:var(--ink);font-size:14px;}' +
```

  - Add this markup, immediately after the header `</div>` from Task 1:

```js
'<div class="preview"><canvas id="pv"></canvas><button class="runbtn" id="run">&#9654; Run animation</button></div>' +
```

  - The page script: change the `<script>` to `'<script>var CFG=' + JSON.stringify(cfg) + ';var ENV=' + JSON.stringify(env || envFrom({}, 'emery')) + ';' + PREVIEW_JS + '\n' + PAGE_JS + '</script>'`.
  - At the start of `PAGE_JS`, add the legacy normalisation. It must match `index.js` `loadCfg`, because old configs can still reach the page:
    `"if(CFG.dayMode===2){CFG.dayCycle=false;CFG.dayMode=0;CFG.dayFixed=1;}if(CFG.dayCycle===undefined)CFG.dayCycle=true;"`
  - Add the preview driver to `PAGE_JS`, before `syncRows();`:

```js
"var PV=document.getElementById('pv'),animT0=null,rafId=null,runPending=false;" +
"function pvSize(){var w=ENV.platform==='basalt'?144:200,h=ENV.platform==='basalt'?168:228;PV.style.width=(2*w)+'px';PV.style.height=(2*h)+'px';}" +
"function frame(ts){rafId=null;if(runPending){runPending=false;animT0=ts;}var animMs=animT0===null?null:ts-animT0;" +
"var r=SkiPreview.draw(PV,CFG,ENV,{now:new Date(),animMs:animMs,wxClock:ts/1000});" +
"if(animT0!==null&&animMs>=r.durationMs)animT0=null;" +
"var moving=CFG.animMode===0&&ENV.platform!=='basalt'&&SkiPreview.weatherFor(CFG,ENV)>=4;" +
"if(animT0!==null||moving)rafId=requestAnimationFrame(frame);}" +
"function redraw(){if(rafId===null)rafId=requestAnimationFrame(frame);}" +
"pvSize();SkiPreview.draw(PV,CFG,ENV,{now:new Date(),animMs:null,wxClock:0});" +
"document.getElementById('run').onclick=function(){runPending=true;redraw();};",
```

  - **Run-twice check:** Run sets `runPending` and calls `redraw()`, which queues a frame only when none is pending. A second click, or a Run while a settings redraw is already queued, never adds a second loop, and the next frame still starts the animation. This satisfies the test. A Run during a running animation restarts it from 0.
  - **Moving weather:** `weatherFor >= 4` (drizzle, rain, snow, storm) keeps the loop going in Full. Check this against `wx_moves` in `src/c/weather.c`; if the C says otherwise, use the C's set.
  - Make the existing `bindSel` `onchange`, `bindToggle` `set()` and the four time-input `onchange` handlers each call `redraw()` after updating `CFG`. The `bindToggle` initial call runs before `PV` exists, so guard it: `if(typeof redraw==='function')redraw();`. Alternatively, move the preview driver block above the `bindSel(...)` lines.
- [ ] **Step 4: Run it and check that it passes.**
  - Run: `node tests/pkjs/config_test.js && node tests/pkjs/preview_test.js`
  - Expected: both pass. Print the page length in the report.
- [ ] **Step 5: Commit.**

```bash
git add src/pkjs/config.js tests/pkjs/config_test.js tests/pkjs/stub_dom.js
git commit -m "Show a live preview with a Run button on the settings page"
```

---

### Task 5: Verify the build and document it

**Files:**
- Modify: `README.md`

- [ ] **Step 1:** In the README's settings description, add one line: "The settings page shows a live preview of the face (tap **Run animation** to see a minute change) and has Ko-fi links to support development."
- [ ] **Step 2:** Run the checks.
  - Run: `node tests/pkjs/config_test.js && node tests/pkjs/preview_test.js && make -C tests 2>&1 | tail -2`. Expected: all pass.
  - Then run `pebble clean` and `pebble build 2>&1 | tail -3`. Expected: success.
- [ ] **Step 3: Render the Run-animation frames.** Using the Task 3 scratchpad harness, render `SkiPreview.draw` at animMs 0, 450, 700, 1500, 2300, 3100 and 4500 for Emery midday clear, and put them side by side. They should show this order: ski-out, crack sweep, flow, fade, new skiers drawing. View the result and list the path in the report.
- [ ] **Step 4: Check that the settings page loads in the emulator, if the tooling allows.**
  - Run `pebble install --emulator emery`, then `pebble emu-app-config --emulator emery`. This opens a browser; it's fine if the environment cannot open it.
  - If it cannot be opened, say so in the report. Do not try a workaround.
- [ ] **Step 5: Commit.**

```bash
git add README.md
git commit -m "Document the settings preview and Ko-fi links"
```
