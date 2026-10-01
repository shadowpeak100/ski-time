#ifndef SKIER_H
#define SKIER_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "geom.h"
#include "layout.h"
#include "glyphs.h"

#define SKI_DRAW_START_MS 2600
#define SKI_STAGGER_MS 200
#define SKI_PAIR_MS 150
#define SKI_STOP_MS 300
#define SKI_EXIT_GAP_MS 30
#define SKI_MAX 8
#define SKI_EXIT_SEGS 16
#define SKI_SCRATCH_FOR(W) ((W) * 7 / 4 + 24)   // sampled points; measured peak need is 238 (basalt) / 324 (emery), >=15% margin
typedef struct { int16_t x, y; uint16_t t; } SkPt;     // x,y in 1/16 px; t ms since the minute change began
typedef struct {
  SkPt *pts; uint16_t n, s_idx;                          // [0,s_idx) ski-in, [s_idx,n) digit stroke (tracks)
  uint16_t t0, t_end;
  Vec2 end, d_end, rest_pos;
  int32_t end_h, rest_h;                                 // trig angles (0 = +x, clockwise on screen)
  float stop_len;
  Vec2 ex[4]; float ex_len[SKI_EXIT_SEGS + 1];           // ski-out cubic + cumulative length
  uint8_t color;                                         // jacket index 0..7
} Skier;
typedef struct { Skier sk[SKI_MAX]; uint8_t count; SkPt *pool; uint16_t pool_cap, pool_used; bool truncated; } SkierSet;
typedef struct { bool visible; int16_t x, y; int32_t h; uint16_t idx; uint8_t spray; } SkierState;  // x,y Q4; spray 0 = none
void skiers_init(SkierSet *set, SkPt *pool, uint16_t cap);
void skiers_build(SkierSet *set, const Layout *L, const DigitSlot *slots, uint8_t nslots);
SkierState skier_state(const Skier *s, uint16_t t_ms);
SkierState skier_exit_state(const Skier *s, uint8_t order, uint16_t t_ms, float k);
uint16_t skiers_duration(const SkierSet *set);
int32_t skier_pt_heading(const Skier *s, uint16_t i);
#define SKI_POOL_CAP(W) ((uint16_t)((W) * 9))    // worst case over every time and setting: 1255 pts at W=144, 1700 at W=200
#define SKI_REST_MS 0xFFFF   // "long after the change": everyone resting
#endif
