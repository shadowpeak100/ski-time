#ifndef SCENE_H
#define SCENE_H

#include <pebble.h>
#include "layout.h"

bool scene_init(const Layout *L);                 // allocates the background buffer + ridge/berm row tables
void scene_deinit(void);
void scene_invalidate(void);                      // rebuild the background on next draw
void scene_blit(GContext *ctx, const Layout *L);  // builds the background if needed, then copies it to the framebuffer
const uint8_t *scene_bg(void);                    // W*H bytes, GColor8 argb
const int16_t *scene_ridge_rows(void);            // first slope row per column (ceil(ridge+0.5))
const int16_t *scene_berm_rows(void);             // first strip row per column (floor(berm))
void scene_draw_chalet(GContext *ctx, const Layout *L);
void scene_draw_smoke(GContext *ctx, const Layout *L, uint32_t ms);
void scene_restore_outside_slope(GContext *ctx, const Layout *L);   // copy bg over sky + strip rows

#endif
