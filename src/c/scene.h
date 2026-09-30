#ifndef SCENE_H
#define SCENE_H

#include <pebble.h>
#include "layout.h"
#include "theme.h"

// Colour of a theme role (powder, snow_shade, ...) as a GColor.
#define SCENE_ROLE(field) ((GColor){ .argb = scene_theme()->field })

bool scene_init(const Layout *L);                 // allocates the background buffer + ridge/berm row tables
void scene_deinit(void);
void scene_set_theme(const Theme *t, float moon_phase);   // rebuilds the background if either changed
const Theme *scene_theme(void);
void scene_invalidate(void);                      // rebuild the background on next draw
bool scene_blit(GContext *ctx, const Layout *L);  // builds the background if needed, else copies it to the framebuffer; true if it had to build
bool scene_ready(void);                           // background built and available
bool scene_bg_row(int y, uint8_t *out, int W);    // unpacks background row y (W GColor8 bytes); false if not built
const int16_t *scene_ridge_rows(void);            // first slope row per column (ceil(ridge+0.5))
const int16_t *scene_berm_rows(void);             // first strip row per column (floor(berm))
void scene_draw_chalet(GContext *ctx, const Layout *L);
void scene_draw_smoke(GContext *ctx, const Layout *L, uint32_t ms);
void scene_draw_campfire(GContext *ctx, const Layout *L, uint32_t ms);   // night only; same clock as the smoke
#ifdef PBL_PLATFORM_EMERY
void scene_draw_slope_trees(GContext *ctx, const Layout *L);   // over the skiers, who pass behind them
#endif
void scene_restore_outside_slope(GContext *ctx, const Layout *L);   // copy bg over sky + strip rows

#endif
