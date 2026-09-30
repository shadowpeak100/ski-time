#include "comps.h"
#include "settings.h"
#include "scene.h"

static int s_temp_f;
static bool s_temp_valid, s_saver;

static const char *const MO[12] = { "JAN","FEB","MAR","APR","MAY","JUN",
                                    "JUL","AUG","SEP","OCT","NOV","DEC" };

void comps_set_temp(int temp_f, bool valid) { s_temp_f = temp_f; s_temp_valid = valid; }
void comps_set_saver(bool saver) { s_saver = saver; }

bool comps_needs_seconds(void) {
  const Settings *st = settings_get();
  for (int i = 0; i < 3; i++) if (st->comp[i] == COMP_SECONDS) return true;
  return false;
}

static void format_complication(char *buf, size_t n, uint8_t type, const struct tm *now) {
  switch (type) {
    case COMP_SECONDS:
      if (s_saver) snprintf(buf, n, "-");
      else snprintf(buf, n, ":%02d", (int)now->tm_sec);
      break;
    case COMP_WEATHER:
      if (s_temp_valid) snprintf(buf, n, "%d\xC2\xB0", s_temp_f);
      else snprintf(buf, n, "--");
      break;
    case COMP_DATE:
      snprintf(buf, n, "%s %d", MO[now->tm_mon % 12], (int)now->tm_mday);
      break;
    case COMP_STEPS:
      snprintf(buf, n, "%d", (int)health_service_sum_today(HealthMetricStepCount));
      break;
    case COMP_HEART: {
      int bpm = (int)health_service_peek_current_value(HealthMetricHeartRateBPM);
      if (bpm > 0) snprintf(buf, n, "%d", bpm);
      else snprintf(buf, n, "--");
      break;
    }
    case COMP_BATTERY:
      snprintf(buf, n, "%d%%", battery_state_service_peek().charge_percent);
      break;
    default:
      buf[0] = '\0';
  }
}

void comps_draw(GContext *ctx, const Layout *L) {
  time_t t = time(NULL);
  struct tm now = *localtime(&t);
  bool big = L->W >= 200;
  GFont font = fonts_get_system_font(big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  int16_t k4 = (int16_t)(4 * L->k + 0.5f);
  int16_t text_h = big ? 30 : 24;
  // Glyphs sit high in their bounds rect; bias up 4 px to centre visually.
  int16_t y = (int16_t)(L->comp_top + (L->comp_h - text_h) / 2 - 4);
  GRect strip = GRect(k4, y, L->W - 2 * k4, text_h);
  graphics_context_set_text_color(ctx, SCENE_ROLE(text));
  const GTextAlignment aligns[3] = { GTextAlignmentLeft, GTextAlignmentCenter, GTextAlignmentRight };
  const Settings *st = settings_get();
  for (int i = 0; i < 3; i++) {
    char buf[12];
    format_complication(buf, sizeof(buf), st->comp[i], &now);
    if (buf[0]) graphics_draw_text(ctx, buf, font, strip, GTextOverflowModeWordWrap, aligns[i], NULL);
  }
}
