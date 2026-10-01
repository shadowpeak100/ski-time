#ifndef WX_DRAW_H
#define WX_DRAW_H

#include <pebble.h>
#include "layout.h"
#include "daypart.h"
#include "weather.h"

#ifdef PBL_PLATFORM_EMERY
// Rain, snow and lightning over the finished frame. still: Balanced / Battery saver (frozen, denser, bolt held).
void wx_draw(GContext *ctx, const Layout *L, WxKind w, Daypart d, bool still, int8_t wind, uint32_t ms);
#endif

#endif
