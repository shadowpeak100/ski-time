#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "daypart.h"

typedef uint8_t Col8;
#define C8(hex) ((Col8)(0xC0 | ((((hex) >> 22) & 3) << 4) | ((((hex) >> 14) & 3) << 2) | (((hex) >> 6) & 3)))

typedef enum { BODY_SUN = 0, BODY_MOON = 1 } BodyKind;
typedef struct { BodyKind kind; float x, y; uint8_t r, halo; Col8 core, glow; } Body;   // x,y fractions of W,H

typedef struct {
  Col8 sky[4]; uint8_t nsky; Body body; bool stars, night;
  Col8 far[4], near_[4];              // lit, shade, cap lit, cap shade
  Col8 forest, pine_lit, pine_shade, pine_snow;
  Col8 snow, snow_shade, snow_shadow, powder, text;
  Col8 cloud[3];                      // reserved for phase 3
} Theme;

const Theme *theme_for(Daypart d);
uint8_t theme_bg_palette(const Theme *t, Col8 out[32]);   // distinct colours the stored background may hold
