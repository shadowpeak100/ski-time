#pragma once

#define FLOOD_COUNT 5
typedef struct { float bx, by, hx, hy, dx, dy; } FloodHead;   // base, head, unit aim direction (px)

void  flood_heads(int W, int H, float k, FloodHead out[FLOOD_COUNT]);
float flood_at(const FloodHead *h, int n, float k, float x, float y);   // 0..1, max over the first n heads
