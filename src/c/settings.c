#include "settings.h"

#define PERSIST_KEY_SETTINGS 1   // key 0 is the weather temperature (main.c)

#define PHASE1_SETTINGS_SIZE 12
#define PHASE2_SETTINGS_SIZE 16
#define PHASE3_SETTINGS_SIZE 18

_Static_assert(sizeof(Settings) == 20, "persisted layout changed: add a migration size");

static Settings s;

static const Settings DEFAULTS = {
  .comp = { COMP_WEATHER, COMP_DATE, COMP_SECONDS },
  .time_24h = 1,
  .lead_zero = 1,
  .track_style = 0,
  .saver_enabled = 0,
  .saver_start = 1320,   // 22:00
  .saver_end = 420,      // 07:00
  .daypart_mode = 0,
  .fixed_rise = 420,
  .fixed_set = 1140,
  .anim_mode = 1,   // ANIM_BALANCED
  .weather_on = 1,
  .daypart_fixed = 1,   // DAY_MIDDAY
  .wx_fixed = 0,        // WX_CLEAR
};

void settings_load(void) {
  int n = persist_exists(PERSIST_KEY_SETTINGS) ? persist_get_size(PERSIST_KEY_SETTINGS) : 0;
  s = DEFAULTS;
  if (n != (int)sizeof(Settings) && n != PHASE3_SETTINGS_SIZE && n != PHASE2_SETTINGS_SIZE && n != PHASE1_SETTINGS_SIZE) return;
  persist_read_data(PERSIST_KEY_SETTINGS, &s, n);   // the phase 1 to 3 layouts are exact prefixes of this one
  if (n == PHASE1_SETTINGS_SIZE) s.daypart_mode = 0;   // was the pad byte before saver_start
  if (n != (int)sizeof(Settings)) persist_write_data(PERSIST_KEY_SETTINGS, &s, sizeof(Settings));
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
  changed |= set_u8(it, MESSAGE_KEY_DAYPART_MODE, &s.daypart_mode);
  changed |= set_u16(it, MESSAGE_KEY_SUN_FIXED_RISE, &s.fixed_rise);
  changed |= set_u16(it, MESSAGE_KEY_SUN_FIXED_SET, &s.fixed_set);
  changed |= set_u8(it, MESSAGE_KEY_ANIM_MODE, &s.anim_mode);
  changed |= set_u8(it, MESSAGE_KEY_WEATHER_ON, &s.weather_on);
  changed |= set_u8(it, MESSAGE_KEY_DAYPART_FIXED, &s.daypart_fixed);
  changed |= set_u8(it, MESSAGE_KEY_WX_FIXED, &s.wx_fixed);
  if (changed) persist_write_data(PERSIST_KEY_SETTINGS, &s, sizeof(Settings));
  return changed;
}

const Settings *settings_get(void) { return &s; }
