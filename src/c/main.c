#include <pebble.h>
#include "layout.h"
#include "scene.h"
#include "skier.h"
#include "glyphs.h"
#include "avalanche.h"
#include "sprites.h"
#include "aval_draw.h"
#include "settings.h"
#include "comps.h"
#include "saver.h"
#include "daypart.h"
#include "theme.h"
#include "wx_draw.h"

#define PERSIST_KEY_TEMP_F 0
#define PERSIST_KEY_SUN 2   // key 1 is Settings
#define PERSIST_KEY_WX 3

typedef struct { const SkierSet *old; const SkierSet *neu; uint16_t t; } Frame;   // old == NULL -> resting

static Window *s_window;
static Layer *s_layer;
static Layout s_L;
static bool s_scene_ok;
static TrackStyle s_track_style = TRACK_TWIN;
static uint32_t s_smoke_ms, s_smoke_base;   // base advances only while animating
static bool s_saver;
static uint8_t s_mode = ANIM_BALANCED;   // effective AnimMode
static int s_last_min = -1;
static SkierSet s_set_a, s_set_b;
static SkPt *s_pool_a, *s_pool_b;
static SkierSet *s_old, *s_new;
static bool s_anim;
static uint32_t s_anim_start_ms;
static bool s_anim_first_frame;   // the first frame may include a slow background rebuild
static AppTimer *s_anim_timer;
static uint16_t s_sun_rise = 420, s_sun_set = 1140;   // phone data, minutes of day
static float s_moon;                                  // phase for the cached date
static int s_moon_yday = -1;
#define ANIM_FRAME_MS 40
#ifdef PBL_PLATFORM_EMERY   // basalt draws no weather, so it neither stores it nor links weather.c
typedef struct { int16_t wmo; int8_t wind; } WxSaved;   // persisted under PERSIST_KEY_WX
static WxSaved s_wx;                  // phone data, applied at the next apply_theme(); clear until some arrives
#define REST_FRAME_MS 100
#define REST_BOLT_FRAME_MS 25   // samples the bolt double flicker near the start of its 5 s cycle
static AppTimer *s_rest_timer;        // Full mode only, never alongside s_anim_timer
#endif

static uint32_t now_ms(void);

static bool render_frame(GContext *ctx, const Frame *f) {   // true if the background was rebuilt
  const Layout *L = &s_L;
  TrackStyle ts = s_track_style;
  bool rebuilt = scene_blit(ctx, L);
  if (f->old && f->t < AV_FLOW_END_MS + AV_FADE_MS)
    for (uint8_t i = 0; i < f->old->count; i++) sprites_track(ctx, &f->old->sk[i], f->old->sk[i].s_idx, f->old->sk[i].n - 1, ts);
  if (f->old) aval_draw(ctx, L, f->t);
  for (uint8_t i = 0; i < f->neu->count; i++) {
    const Skier *s = &f->neu->sk[i];
    SkierState st = skier_state(s, f->t);
    if (st.visible && st.idx > s->s_idx) sprites_track(ctx, s, s->s_idx, st.idx, ts);
  }
  scene_draw_chalet(ctx, L);
#ifdef PBL_PLATFORM_EMERY
  if (scene_theme()->night) scene_draw_campfire(ctx, L, s_smoke_ms);
#endif
  scene_draw_smoke(ctx, L, s_smoke_ms);
  comps_draw(ctx, L);
  if (f->old && f->t < 1500)
    for (uint8_t i = 0; i < f->old->count; i++) {
      SkierState st = skier_exit_state(&f->old->sk[i], i, f->t, L->k);
      if (st.visible) sprites_skier(ctx, st.x, st.y, st.h, f->old->sk[i].color, L->k);
    }
  for (uint8_t i = 0; i < f->neu->count; i++) {
    SkierState st = skier_state(&f->neu->sk[i], f->t);
    if (!st.visible) continue;
    sprites_skier(ctx, st.x, st.y, st.h, f->neu->sk[i].color, L->k);
    if (st.spray) sprites_spray(ctx, st.x, st.y, st.h, st.spray, L->k);
  }
#ifdef PBL_PLATFORM_EMERY
  scene_draw_slope_trees(ctx, L);
  bool still = s_mode != ANIM_FULL;
  wx_draw(ctx, L, scene_weather(), theme_daypart(scene_theme()), still, s_wx.wind, still ? s_smoke_ms : now_ms());
#endif
  return rebuilt;
}

static void now_hm(int *h, int *m) {
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  *h = lt->tm_hour;
  *m = lt->tm_min;
#ifdef MOCK_H
  *h = MOCK_H;
  *m = MOCK_M;
#endif
}

static uint32_t now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (uint32_t)s * 1000u + ms;
}

static bool build_set(SkierSet *set, int h24, int m) {
  DigitSlot slots[4];
  uint8_t n = time_slots(h24, m, settings_get()->time_24h, settings_get()->lead_zero, slots);
  skiers_build(set, &s_L, slots, n);
  if (set->count == 0) APP_LOG(APP_LOG_LEVEL_WARNING, "skier build failed, keeping previous time");
  return set->count > 0;
}

static Daypart current_daypart(int h, int m) {
#ifdef MOCK_DAYPART
  (void)h; (void)m;
  return MOCK_DAYPART;
#else
  const Settings *st = settings_get();
  return daypart_for(st->daypart_mode, h * 60 + m, s_sun_rise, s_sun_set, st->fixed_rise, st->fixed_set,
                      st->daypart_fixed);
#endif
}

// Moon is only recomputed when the date changes, and non-night themes get phase 0 so the
// background is not rebuilt on every minute.
static void apply_theme(void) {
  int h, m;
  now_hm(&h, &m);
  Daypart dp = current_daypart(h, m);
  float moon = 0;
  if (dp == DAY_NIGHT) {
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (lt->tm_yday != s_moon_yday) {
      s_moon = moon_phase(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, h, m);
      s_moon_yday = lt->tm_yday;
    }
    moon = s_moon;
  }
  scene_set_theme(theme_for(dp), moon);
#ifdef MOCK_WEATHER
  scene_set_weather(MOCK_WEATHER);
#elif defined(PBL_PLATFORM_EMERY)
  const Settings *st = settings_get();
  scene_set_weather(st->weather_on ? wx_from_wmo(s_wx.wmo) : (WxKind)(st->wx_fixed <= WX_STORM ? st->wx_fixed : WX_CLEAR));
#endif
}

#ifdef PBL_PLATFORM_EMERY
static void rest_timer_update(void);

static void rest_timer_cancel(void) {
  if (s_rest_timer) { app_timer_cancel(s_rest_timer); s_rest_timer = NULL; }
}

static void rest_timer_cb(void *data) {
  (void)data;
  s_rest_timer = NULL;
  if (s_layer) layer_mark_dirty(s_layer);
  rest_timer_update();
}

// The only place the rest timer is registered: Full mode, moving weather, no minute animation.
static void rest_timer_update(void) {
  bool want = s_layer && s_mode == ANIM_FULL && !s_anim && wx_moves(scene_weather());
  if (!want) rest_timer_cancel();
  else if (!s_rest_timer) {
    const uint32_t c = now_ms() % 5000;
    const bool flicker = wx_def(scene_weather())->lightning && (c >= 4900 || c < 340);
    s_rest_timer = app_timer_register(flicker ? REST_BOLT_FRAME_MS : REST_FRAME_MS, rest_timer_cb, NULL);
  }
}
#else
static inline void rest_timer_update(void) {}
#endif

static void anim_stop(void) {
  if (s_anim) s_smoke_base += now_ms() - s_anim_start_ms;
  s_smoke_ms = s_smoke_base;
  if (s_anim_timer) { app_timer_cancel(s_anim_timer); s_anim_timer = NULL; }
  s_anim = false;
  s_old = NULL;
  if (s_layer) layer_mark_dirty(s_layer);
  rest_timer_update();
}

static void anim_timer_cb(void *data) {
  (void)data;
  s_anim_timer = NULL;
  uint32_t t = now_ms() - s_anim_start_ms;
  if (t >= skiers_duration(s_new)) {
    anim_stop();
    APP_LOG(APP_LOG_LEVEL_INFO, "heap free at rest %d", (int)heap_bytes_free());
    return;
  }
  layer_mark_dirty(s_layer);
  s_anim_timer = app_timer_register(ANIM_FRAME_MS, anim_timer_cb, NULL);
}

static void rebuild_current(void) {
  if (!s_pool_a) return;
  int h, m;
  now_hm(&h, &m);
  anim_stop();
  apply_theme();
  SkierSet *target = s_new == &s_set_a ? &s_set_b : &s_set_a;
  if (build_set(target, h, m)) s_new = target;
  rest_timer_update();   // the weather may have changed
}

static void anim_start(void) {
  if (!s_scene_ok || !s_pool_a) return;
  if (s_anim) anim_stop();
  SkierSet *target = s_new == &s_set_a ? &s_set_b : &s_set_a;
  int h, m;
  now_hm(&h, &m);
  apply_theme();   // before the first animated frame
  if (!build_set(target, h, m)) { rest_timer_update(); return; }
  s_old = s_new;
  s_new = target;
  s_anim = true;
  rest_timer_update();   // cancels the rest timer: the 40 ms frame timer takes over
  s_anim_start_ms = now_ms();
  s_anim_first_frame = true;
  s_smoke_ms = s_smoke_base;
#ifndef MOCK_ANIM_MS
  s_anim_timer = app_timer_register(ANIM_FRAME_MS, anim_timer_cb, NULL);
#endif
}

static void tick_handler(struct tm *tm, TimeUnits changed);

static void apply_mode(uint8_t mode) {
  bool on = mode == ANIM_SAVER;
  s_mode = mode;
  s_saver = on;
  comps_set_saver(on);
  if (on) anim_stop();
  tick_timer_service_subscribe(!on && comps_needs_seconds() ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
  if (s_layer) layer_mark_dirty(s_layer);
  rest_timer_update();
}

static uint8_t mode_now(void) {
#ifdef MOCK_ANIM_MODE
  return MOCK_ANIM_MODE;
#else
  int h, m;
  now_hm(&h, &m);
  const Settings *st = settings_get();
  return effective_anim_mode(st->anim_mode, st->saver_enabled, st->saver_start, st->saver_end, h, m);
#endif
}

static void tick_handler(struct tm *tm, TimeUnits changed) {
  (void)changed;
  uint8_t mode = mode_now();
  if (mode != s_mode) apply_mode(mode);
  if (s_layer) layer_mark_dirty(s_layer);
  int hm = tm->tm_hour * 60 + tm->tm_min;
  if (hm == s_last_min) return;
  s_last_min = hm;
#ifndef MOCK_ANIM_MS
  if (s_saver) rebuild_current();
  else anim_start();
#endif
}

// Request heart-rate sampling only while a slot actually shows heart rate.
static void update_health(void) {
  const Settings *st = settings_get();
  bool hr = st->comp[0] == COMP_HEART || st->comp[1] == COMP_HEART || st->comp[2] == COMP_HEART;
  health_service_set_heart_rate_sample_period(hr ? 10 : 0);
}

static void inbox_received(DictionaryIterator *it, void *ctx) {
  (void)ctx;
  Tuple *t = dict_find(it, MESSAGE_KEY_TEMP_F);
  if (t) {
    comps_set_temp((int)t->value->int32, true);
    persist_write_int(PERSIST_KEY_TEMP_F, (int)t->value->int32);
  }
  Tuple *rise = dict_find(it, MESSAGE_KEY_SUNRISE), *set = dict_find(it, MESSAGE_KEY_SUNSET);
  if (rise || set) {   // takes effect at the next minute change
    if (rise && rise->value->int32 >= 0 && rise->value->int32 < 1440) s_sun_rise = (uint16_t)rise->value->int32;
    if (set && set->value->int32 >= 0 && set->value->int32 < 1440) s_sun_set = (uint16_t)set->value->int32;
    uint16_t sun[2] = { s_sun_rise, s_sun_set }, saved[2] = { 0, 0 };
    if (persist_read_data(PERSIST_KEY_SUN, saved, sizeof(saved)) != (int)sizeof(saved) || saved[0] != sun[0] || saved[1] != sun[1])
      persist_write_data(PERSIST_KEY_SUN, sun, sizeof(sun));
  }
#ifdef PBL_PLATFORM_EMERY
  Tuple *code = dict_find(it, MESSAGE_KEY_WX_CODE), *wind = dict_find(it, MESSAGE_KEY_WIND_X);
  if (code || wind) {   // takes effect at the next minute change
    WxSaved wx = s_wx;
    if (code && code->value->int32 >= 0 && code->value->int32 <= 99) wx.wmo = (int16_t)code->value->int32;
    if (wind && wind->value->int32 >= -1 && wind->value->int32 <= 1) wx.wind = (int8_t)wind->value->int32;
    if (wx.wmo != s_wx.wmo || wx.wind != s_wx.wind) persist_write_data(PERSIST_KEY_WX, &wx, sizeof(wx));
    s_wx = wx;
  }
#endif
  if (settings_apply_message(it)) {
    s_track_style = settings_get()->track_style == TRACK_SOLID ? TRACK_SOLID : TRACK_TWIN;
    anim_stop();
    rebuild_current();
    update_health();
    apply_mode(mode_now());
  }
  if (s_layer) layer_mark_dirty(s_layer);
}

static void update(Layer *layer, GContext *ctx) {
  if (!s_scene_ok || !s_pool_a) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
    return;
  }
#ifdef MOCK_ANIM_MS
  uint32_t el = MOCK_ANIM_MS;
  Frame f = { s_old, s_new, MOCK_ANIM_MS };
#else
  uint32_t el = s_anim ? now_ms() - s_anim_start_ms : 0;
  s_smoke_ms = s_smoke_base + el;
  Frame f = { s_anim ? s_old : NULL, s_new, s_anim ? (uint16_t)(el < 65534 ? el : 65534) : SKI_REST_MS };
#endif
  bool rebuilt = render_frame(ctx, &f);
#ifndef MOCK_ANIM_MS
  if (s_anim && s_anim_first_frame && rebuilt) s_anim_start_ms = now_ms();   // do not skip the start of the animation
  s_anim_first_frame = false;
#endif
}

static void build_skiers(void) {
  uint16_t cap = SKI_POOL_CAP(s_L.W);
  s_pool_a = malloc(cap * sizeof(SkPt));
  s_pool_b = malloc(cap * sizeof(SkPt));
  if (!s_pool_a || !s_pool_b) {
    free(s_pool_a); free(s_pool_b);
    s_pool_a = s_pool_b = NULL;
    return;
  }
  skiers_init(&s_set_a, s_pool_a, cap);
  skiers_init(&s_set_b, s_pool_b, cap);
  s_new = &s_set_a;
  rebuild_current();
#ifdef MOCK_ANIM_MS
  int h, m;
  now_hm(&h, &m);
  SkierSet *prev = s_new == &s_set_a ? &s_set_b : &s_set_a;
  build_set(prev, m == 0 ? (h + 23) % 24 : h, m == 0 ? 59 : m - 1);
  s_old = prev;
  s_anim = true;
#endif
}

static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect bounds = layer_get_bounds(root);
  layout_init(&s_L, bounds.size.w, bounds.size.h);
  s_scene_ok = scene_init(&s_L);
  apply_theme();
  build_skiers();
  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, update);
  layer_add_child(root, s_layer);
  { int h, m; now_hm(&h, &m); s_last_min = h * 60 + m; }
  apply_mode(mode_now());
}

static void window_unload(Window *w) {
  (void)w;
  tick_timer_service_unsubscribe();
  anim_stop();
#ifdef PBL_PLATFORM_EMERY
  rest_timer_cancel();
#endif
  layer_destroy(s_layer);
  s_layer = NULL;
  free(s_pool_a); free(s_pool_b);
  s_pool_a = s_pool_b = NULL;
  scene_deinit();
}

static void init(void) {
  settings_load();
  s_track_style = settings_get()->track_style == TRACK_SOLID ? TRACK_SOLID : TRACK_TWIN;
  if (persist_exists(PERSIST_KEY_TEMP_F)) comps_set_temp(persist_read_int(PERSIST_KEY_TEMP_F), true);
  uint16_t sun[2];
  if (persist_get_size(PERSIST_KEY_SUN) == (int)sizeof(sun)) {
    persist_read_data(PERSIST_KEY_SUN, sun, sizeof(sun));
    s_sun_rise = sun[0]; s_sun_set = sun[1];
  }
#ifdef PBL_PLATFORM_EMERY
  if (persist_get_size(PERSIST_KEY_WX) == (int)sizeof(s_wx)) {
    persist_read_data(PERSIST_KEY_WX, &s_wx, sizeof(s_wx));
    if (s_wx.wmo < 0 || s_wx.wmo > 99) s_wx.wmo = 0;
    if (s_wx.wind < -1 || s_wx.wind > 1) s_wx.wind = 0;
  }
#endif
  update_health();
  app_message_register_inbox_received(inbox_received);
  app_message_open(192, 16);
}

int main(void) {
  init();
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){ .load = window_load, .unload = window_unload });
  window_stack_push(s_window, true);
  app_event_loop();
  window_destroy(s_window);
  app_message_deregister_callbacks();
}
