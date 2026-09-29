#ifndef SETTINGS_H
#define SETTINGS_H

#include <pebble.h>

typedef enum {
  COMP_SECONDS = 0,
  COMP_WEATHER = 1,
  COMP_DATE    = 2,
  COMP_STEPS   = 3,
  COMP_HEART   = 4,
  COMP_BATTERY = 5,
} ComplicationType;

// Persisted verbatim as one blob.
typedef struct {
  uint8_t  comp[3];        // left / center / right
  uint8_t  time_24h, lead_zero, track_style;
  uint8_t  saver_enabled;
  uint16_t saver_start, saver_end;   // minutes-of-day
} Settings;

void settings_load(void);
bool settings_apply_message(DictionaryIterator *it);   // true if anything changed
const Settings *settings_get(void);

#endif
