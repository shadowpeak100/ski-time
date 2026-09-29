#include <pebble.h>
#include "layout.h"
#include "scene.h"
#include "skier.h"
#include "glyphs.h"
#include "avalanche.h"
#include "sprites.h"
#include "aval_draw.h"

typedef struct { const SkierSet *old; const SkierSet *neu; uint16_t t; } Frame;   // old == NULL -> resting

static Window *s_window;
static Layer *s_layer;
static Layout s_L;
static bool s_scene_ok;
static TrackStyle s_track_style = TRACK_TWIN;
static uint32_t s_smoke_ms;
static bool s_use24 = true, s_lead0 = true;
static SkierSet s_cur, s_prev;
static SkPt *s_pool_cur, *s_pool_prev;

static void comps_draw(GContext *ctx, const Layout *L) { (void)ctx; (void)L; }

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
  comps_draw(ctx, L);                       // stub: empty function until Task 9
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

static void build_set(SkierSet *set, int h24, int m) {
  DigitSlot slots[4];
  uint8_t n = time_slots(h24, m, s_use24, s_lead0, slots);
  skiers_build(set, &s_L, slots, n);
}

static void update(Layer *layer, GContext *ctx) {
  if (!s_scene_ok || !s_pool_cur) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
    return;
  }
#ifdef MOCK_ANIM_MS
  Frame f = { &s_prev, &s_cur, MOCK_ANIM_MS };
#else
  Frame f = { NULL, &s_cur, SKI_REST_MS };
#endif
  render_frame(ctx, &f);
}

static void build_skiers(void) {
  uint16_t cap = SKI_POOL_CAP(s_L.W);
  s_pool_cur = malloc(cap * sizeof(SkPt));
  s_pool_prev = malloc(cap * sizeof(SkPt));
  APP_LOG(APP_LOG_LEVEL_INFO, "heap free %d", (int)heap_bytes_free());
  if (!s_pool_cur || !s_pool_prev) {
    free(s_pool_cur); free(s_pool_prev);
    s_pool_cur = s_pool_prev = NULL;
    return;
  }
  skiers_init(&s_cur, s_pool_cur, cap);
  skiers_init(&s_prev, s_pool_prev, cap);
#ifdef MOCK_H
  int h = MOCK_H, m = MOCK_M;
#else
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  int h = tm->tm_hour, m = tm->tm_min;
#endif
  build_set(&s_cur, h, m);
  int pm = m == 0 ? 59 : m - 1, ph = m == 0 ? (h + 23) % 24 : h;
  build_set(&s_prev, ph, pm);
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
}

static void window_unload(Window *w) {
  (void)w;
  layer_destroy(s_layer);
  free(s_pool_cur); free(s_pool_prev);
  s_pool_cur = s_pool_prev = NULL;
  scene_deinit();
}

int main(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){ .load = window_load, .unload = window_unload });
  window_stack_push(s_window, true);
  app_event_loop();
  window_destroy(s_window);
}
