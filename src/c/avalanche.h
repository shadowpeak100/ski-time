#ifndef AVALANCHE_H
#define AVALANCHE_H

#include <stdint.h>
#include <stdbool.h>
#include "layout.h"

#define AV_CRACK_MS 550
#define AV_FLOW_START_MS 700
#define AV_FLOW_END_MS 1650
#define AV_FADE_MS 300
#define AV_MAX_S 320

typedef enum { AV_NONE = 0, AV_SHADOW, AV_RESTORE, AV_WHITE, AV_CELESTE, AV_BLUE } AvPix;
typedef struct { bool active; float a, fade, grow, uf; int16_t body5; int32_t sh0, sh1, wake5, grow256, fade256; int16_t front5[AV_MAX_S]; } AvalFrame;

float   aval_noise(float x);
uint8_t aval_bayer(int x, int y);                         // 0..15
void    aval_streaks(uint8_t streak[AV_MAX_S]);
void    aval_frame(AvalFrame *A, const Layout *L, uint16_t t_ms);
float   aval_front(const AvalFrame *A, float s);          // front position along FALL (px) at across-slope s
AvPix   aval_pixel(const AvalFrame *A, const uint8_t *streak, float k, int x, int y, uint16_t t_ms);

#endif
