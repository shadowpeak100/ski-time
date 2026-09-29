#include "settings.h"

#define PERSIST_KEY_SETTINGS 1   // key 0 is the weather temperature (main.c)

static Settings s;

static const Settings DEFAULTS = {
  .comp = { COMP_WEATHER, COMP_DATE, COMP_SECONDS },
  .time_24h = 1,
  .lead_zero = 1,
  .track_style = 0,
  .saver_enabled = 0,
  .saver_start = 1320,   // 22:00
  .saver_end = 420,      // 07:00
};

void settings_load(void) {
  if (persist_exists(PERSIST_KEY_SETTINGS) &&
      persist_get_size(PERSIST_KEY_SETTINGS) == (int)sizeof(Settings)) {
    persist_read_data(PERSIST_KEY_SETTINGS, &s, sizeof(Settings));
  } else {
    s = DEFAULTS;
  }
}

static bool set_u8(DictionaryIterator *it, uint32_t key, uint8_t *dst) {
  Tuple *t = dict_find(it, key);
  if (!t) return false;
  *dst = (uint8_t)t->value->int32;
  return true;
}
static bool set_u16(DictionaryIterator *it, uint32_t key, uint16_t *dst) {
  Tuple *t = dict_find(it, key);
  if (!t) return false;
  *dst = (uint16_t)t->value->int32;
  return true;
}

bool settings_apply_message(DictionaryIterator *it) {
  bool changed = false;
  changed |= set_u8(it, MESSAGE_KEY_COMP_1, &s.comp[0]);
  changed |= set_u8(it, MESSAGE_KEY_COMP_2, &s.comp[1]);
  changed |= set_u8(it, MESSAGE_KEY_COMP_3, &s.comp[2]);
  changed |= set_u8(it, MESSAGE_KEY_TIME_24H, &s.time_24h);
  changed |= set_u8(it, MESSAGE_KEY_LEAD_ZERO, &s.lead_zero);
  changed |= set_u8(it, MESSAGE_KEY_TRACK_STYLE, &s.track_style);
  changed |= set_u8(it, MESSAGE_KEY_SAVER_ON, &s.saver_enabled);
  changed |= set_u16(it, MESSAGE_KEY_SAVER_START, &s.saver_start);
  changed |= set_u16(it, MESSAGE_KEY_SAVER_END, &s.saver_end);
  if (changed) persist_write_data(PERSIST_KEY_SETTINGS, &s, sizeof(Settings));
  return changed;
}

const Settings *settings_get(void) { return &s; }
