# Ski Time

A Pebble watch face for basalt (Pebble Time) and emery (Pebble Time 2). A ski slope falls from the
top right to the bottom left, and small skiers carve the time into the snow as curly script digits.
Each minute the old skiers ski out, an avalanche clears the slope, and new skiers draw the new time.
Design spec: `docs/superpowers/specs/2026-09-29-ski-time-design.md`.

## Features

- Three complication slots below the slope: seconds, weather, date, steps, heart rate or battery.
- 12-hour or 24-hour time, with an optional leading zero.
- Track style: twin grooves or a solid line.
- Times of day: sunrise, midday, sunset and night each have their own sky, with a moon and stars at night. Emery also gets window glow, floodlights, a campfire and slope trees that skiers pass behind.
- Battery saver: during a chosen time window the face stops animating.
- Weather comes from the phone (PebbleKit JS) and shows `--` until the first sync.

## Time of day

The **Time of day** setting ("Sky follows") has three modes: follow the real sunrise and sunset, fixed
sunrise and sunset times, or always midday. Follow mode uses sunrise and sunset from Open-Meteo,
fetched by the phone with the weather and kept on the watch. Until the first sync it assumes 07:00 and 19:00.
Basalt keeps the themes, moon and stars; the night extras and slope trees are Emery only.
Spec: `docs/superpowers/specs/2026-09-29-times-of-day-design.md`.

## Build & run

```sh
pebble build                                  # basalt + emery
pebble install --emulator basalt              # or emery
pebble emu-app-config --emulator basalt       # open the settings page
make -C tests                                 # host unit tests for the pure modules
```

## Screenshots

Pin the time and freeze the minute change at a given point (in ms) so it can be captured:

```sh
pebble kill; pebble wipe                      # clear saved settings so defaults apply
MOCK_TIME=20:47 MOCK_DAYPART=3 MOCK_ANIM_MS=1200 pebble build
pebble install --emulator basalt
pebble screenshot --emulator basalt shot.png
```

Add `MOCK_DAYPART=0..3` (sunrise, midday, sunset, night) to force a time of day.
Leave out `MOCK_ANIM_MS` for the resting scene. Run a plain `pebble build` afterwards.

## Emulator tips

If the emulator hangs or misbehaves, run `pebble kill` and `pebble wipe`, then install again.
