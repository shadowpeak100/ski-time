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

#define PERSIST_KEY_TEMP_F 0

typedef struct { const SkierSet *old; const SkierSet *neu; uint16_t t; } Frame;   // old == NULL -> resting

static Window *s_window;
static Layer *s_layer;
static Layout s_L;
static bool s_scene_ok;
static TrackStyle s_track_style = TRACK_TWIN;
static uint32_t s_smoke_ms;
static bool s_saver;
static int s_last_min = -1;
static SkierSet s_set_a, s_set_b;
static SkPt *s_pool_a, *s_pool_b;
static SkierSet *s_old, *s_new;
static bool s_anim;
static uint32_t s_anim_start_ms;
static AppTimer *s_anim_timer;
#define ANIM_FRAME_MS 40

static void render_frame(GContext *ctx, const Frame *f) {
  const Layout *L = &s_L;
  TrackStyle ts = s_track_style;
  scene_blit(ctx, L);
  if (f->old && f->t < AV_FLOW_END_MS + AV_FADE_MS)
    for (uint8_t i = 0; i < f->old->count; i++) sprites_track(ctx, &f->old->sk[i], f->old->sk[i].s_idx, f->old->sk[i].n - 1, ts);
  if (f->old) aval_draw(ctx, L, f->t);
  for (uint8_t i = 0; i < f->neu->count; i++) {
    const Skier *s = &f->neu->sk[i];
    SkierState st = skier_state(s, f->t);
    if (st.visible && st.idx > s->s_idx) sprites_track(ctx, s, s->s_idx, st.idx, ts);
  }
  scene_draw_chalet(ctx, L);
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

static void anim_stop(void) {
  if (s_anim_timer) { app_timer_cancel(s_anim_timer); s_anim_timer = NULL; }
  s_anim = false;
  s_old = NULL;
  if (s_layer) layer_mark_dirty(s_layer);
}

static void anim_timer_cb(void *data) {
  (void)data;
  s_anim_timer = NULL;
  uint32_t t = now_ms() - s_anim_start_ms;
  if (t >= skiers_duration(s_new)) { anim_stop(); return; }
  layer_mark_dirty(s_layer);
  s_anim_timer = app_timer_register(ANIM_FRAME_MS, anim_timer_cb, NULL);
}

static void rebuild_current(void) {
  if (!s_pool_a) return;
  int h, m;
  now_hm(&h, &m);
  anim_stop();
  SkierSet *target = s_new == &s_set_a ? &s_set_b : &s_set_a;
  if (build_set(target, h, m)) s_new = target;
}

static void anim_start(void) {
  if (!s_scene_ok || !s_pool_a) return;
  if (s_anim) anim_stop();
  SkierSet *target = s_new == &s_set_a ? &s_set_b : &s_set_a;
  int h, m;
  now_hm(&h, &m);
  if (!build_set(target, h, m)) return;
  s_old = s_new;
  s_new = target;
  s_anim = true;
  s_anim_start_ms = now_ms();
  s_smoke_ms = 0;
#ifndef MOCK_ANIM_MS
  s_anim_timer = app_timer_register(ANIM_FRAME_MS, anim_timer_cb, NULL);
#endif
}

static void tick_handler(struct tm *tm, TimeUnits changed);

static void apply_saver(bool on) {
  s_saver = on;
  comps_set_saver(on);
  if (on) anim_stop();
  tick_timer_service_subscribe(!on && comps_needs_seconds() ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
  if (s_layer) layer_mark_dirty(s_layer);
}

static bool saver_now(void) {
  int h, m;
  now_hm(&h, &m);
  const Settings *st = settings_get();
  return saver_active(st->saver_enabled, st->saver_start, st->saver_end, h, m);
}

static void tick_handler(struct tm *tm, TimeUnits changed) {
  (void)changed;
  bool saver = saver_now();
  if (saver != s_saver) apply_saver(saver);
  if (s_layer) layer_mark_dirty(s_layer);
  if (tm->tm_min == s_last_min) return;
  s_last_min = tm->tm_min;
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
  } else if (settings_apply_message(it)) {
    s_track_style = settings_get()->track_style == TRACK_SOLID ? TRACK_SOLID : TRACK_TWIN;
    anim_stop();
    rebuild_current();
    update_health();
    apply_saver(saver_now());
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
  Frame f = { s_old, s_new, MOCK_ANIM_MS };
#else
  uint32_t el = s_anim ? now_ms() - s_anim_start_ms : 0;
  s_smoke_ms = el;
  Frame f = { s_anim ? s_old : NULL, s_new, s_anim ? (uint16_t)(el < 65534 ? el : 65534) : SKI_REST_MS };
#endif
  render_frame(ctx, &f);
}

static void build_skiers(void) {
  uint16_t cap = SKI_POOL_CAP(s_L.W);
  s_pool_a = malloc(cap * sizeof(SkPt));
  s_pool_b = malloc(cap * sizeof(SkPt));
  APP_LOG(APP_LOG_LEVEL_INFO, "heap free %d", (int)heap_bytes_free());
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
  aval_draw_init();
  build_skiers();
  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, update);
  layer_add_child(root, s_layer);
  { int h, m; now_hm(&h, &m); s_last_min = m; }
  apply_saver(saver_now());
}

static void window_unload(Window *w) {
  (void)w;
  tick_timer_service_unsubscribe();
  anim_stop();
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
  update_health();
  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 16);
}

int main(void) {
  init();
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){ .load = window_load, .unload = window_unload });
  window_stack_push(s_window, true);
  APP_LOG(APP_LOG_LEVEL_INFO, "heap free after init %d", (int)heap_bytes_free());
  app_event_loop();
  window_destroy(s_window);
  app_message_deregister_callbacks();
}
