#ifndef BOLT_H
#define BOLT_H

#include <stdint.h>
#include <stdbool.h>
#include "geom.h"
#include "layout.h"

#define BOLT_MAIN_PTS 33
#define BOLT_BRANCHES 3
#define BOLT_BRANCH_PTS 17

typedef struct { Vec2 main[BOLT_MAIN_PTS]; uint8_t n_main; Vec2 br[BOLT_BRANCHES][BOLT_BRANCH_PTS]; uint8_t n_br[BOLT_BRANCHES]; } Bolt;

void bolt_build(Bolt *b, const Layout *L);                 // the fixed bolt: mulberry(307), (0.2W, 6k) -> (0.1W, ridge - 2)
bool bolt_visible(uint32_t ms, bool still, bool *flash);   // double flicker every 5 s; still: always visible, no flash

#endif
