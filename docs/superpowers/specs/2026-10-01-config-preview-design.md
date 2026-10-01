# Ski Time: settings page preview and Ko-fi links

Date: 2026-10-01
Status: draft, for user review.
Reference: `docs/prototypes/weather.html`. Its scene, skier, avalanche, weather and complications drawing code is the starting point for the preview renderer. Where this spec and the prototype differ, the spec wins.
Patterns:
- **Preview:** constellation's `src/pkjs/config.js`, which has a sticky canvas preview, a Run button and in-page ES5 drawing code.
- **Ko-fi links:** pebble-fit's `src/pkjs/index.js`, with the header pill at about line 2961 and the Support card at about line 4006.

## 1. What the user gets

- **Ko-fi pill.** At the top of the settings page, a header with "Ski Time" on the left and a red Ko-fi pill on the right. The pill uses pebble-fit's `.kofi` style, its cup icon, the label "Ko-fi" and the link `https://ko-fi.com/shadowpeak100`. It opens in a new tab.
- **Preview.** Under the header, a sticky preview: a canvas showing the face at the watch's real size, scaled up 2× with crisp pixels.
  - It redraws whenever a setting changes.
  - A **"▶ Run animation"** button below it plays one full minute change: ski out, crack, avalanche, then the new skiers draw the time.
- **Support card.** At the bottom, above the Save/Cancel bar, a "Support" card. It holds pebble-fit's text, adapted ("Ski Time is free, with no ads…"), and the "☕ Buy me a coffee on Ko-fi" button with the same link.

## 2. What the preview reflects

| Setting | Preview behaviour |
|---|---|
| Daylight cycle on | The time of day for the phone's current clock. With "Sunrise and sunset" it uses the last sun times the phone saved; with "Fixed times" it uses the times on the page. |
| Daylight cycle off | The chosen time of day. |
| Weather on | The last weather the phone fetched, or clear if there is none yet. |
| Weather off | The chosen weather. |
| Animations | Full: rain and snow fall and the bolt flickers on a live loop. Balanced and Battery saver: weather is held still. In Battery saver, Run swaps the time instantly, as on the watch. |
| Track style, 24-hour, Leading zero | The digits use the chosen style and format. |
| Complication slots | The three slots show sample values: 61°, the real date, :38, 8,412 steps, 72 bpm, 80%. |

- The preview shows the phone's current time.
- **Basalt:** when `Pebble.getActiveWatchInfo()` reports basalt, the preview uses the 144×168 layout and draws no weather, as on the watch. Otherwise it uses Emery, 200×228.

## 3. Staying in step with the watch

The prototype is older than these watch changes, so the port must include them:

- **Avalanche timing:** crack at 450 ms, drawn across the visible slope over 500 ms with ease-out. Flow 1000–2800 ms, fade 450 ms. Skiers start drawing at 2600 ms.
- **Animation look:** snow look D, flat partly-cloudy clouds, and the FireWatchDynamic-style bolt between the two left peaks.
- **Layout:**
  - the Lowest hillside, with tree clusters placed as on the watch
  - the floodlights at night
  - the time block centred on the screen.

Small pixel differences from the watch are acceptable, because canvas drawing is not gpath. It must not be missing anything visible on the watch, or show anything extra.

## 4. Data the page needs

`index.js` passes these to `buildHtml` alongside `CFG`:
- the platform (`basalt` or `emery`)
- the last weather WMO code and wind
- the saved sunrise and sunset

It caches these values in `localStorage` when it sends them to the watch. A fresh install uses the fallbacks: clear weather, and sun times of 07:00 and 19:00.

## 5. Size and structure

- **Renderer location:** the renderer lives in a new file, `src/pkjs/preview.js`. It exports the drawing code as one string that `config.js` inlines into the page. The renderer may use ES2015, which is what the prototype is written in and what current phone webviews run. It loads no external scripts.
- **Size budget:** the generated page must stay under 120 KB. The prototype's UI, grid and storyboard code is dropped.
- **Settings untouched:** the existing settings, their message keys and what they send do not change.

## 6. Testing

- A node check that `buildHtml` output:
  - contains the Ko-fi pill, the Support card and the canvas
  - runs its page script without errors against a stub DOM and canvas, for every time of day × weather × platform combination
- Render the preview with `@napi-rs/canvas` at a few settings: Emery midday clear, night storm, sunset snow, and a basalt frame. Compare each side by side with an emulator screenshot of the same settings.
- Render a Run-animation sequence at a few timestamps.
- Check the generated page size.
