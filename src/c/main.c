#include <pebble.h>
#include "layout.h"
#include "scene.h"

static Window *s_window;
static Layer *s_layer;
static Layout s_L;

static void update(Layer *layer, GContext *ctx) {
  (void)layer;
  scene_blit(ctx, &s_L);
  scene_draw_chalet(ctx, &s_L);
  scene_draw_smoke(ctx, &s_L, 0);
}

static void window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect bounds = layer_get_bounds(root);
  layout_init(&s_L, bounds.size.w, bounds.size.h);
  scene_init(&s_L);
  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, update);
  layer_add_child(root, s_layer);
}

static void window_unload(Window *w) {
  (void)w;
  layer_destroy(s_layer);
  scene_deinit();
}

int main(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){ .load = window_load, .unload = window_unload });
  window_stack_push(s_window, true);
  app_event_loop();
  window_destroy(s_window);
}
