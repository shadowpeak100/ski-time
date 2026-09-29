#ifndef GEOM_H
#define GEOM_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct { float x, y; } Vec2;
typedef Vec2 (*GeomMap)(float u, float v, const void *ctx);

#define GEOM_STEPS 60   // dense samples per cubic segment before arc-length resampling

float geom_sqrt(float v);
float geom_sin(float rad);
float geom_cos(float rad);
Vec2  geom_norm(float x, float y);

// Samples a cubic chain {x0,y0, c1x,c1y,c2x,c2y,x1,y1, ...} (n floats) through `map`,
// emitting points spaced `step` apart along the curve plus the exact end point. Returns count.
int geom_sample_chain(const float *chain, int n, GeomMap map, const void *ctx, float step, Vec2 *out, int max);

// Unit direction from p[i-w] to p[i+w], clamped to the array.
Vec2 geom_tangent(const Vec2 *p, int n, int i, int w);

// mulberry32, bit-identical to the prototype's `mulberry`, for deterministic scenery.
typedef struct { uint32_t a; } Mulberry;
float mulberry_next(Mulberry *m);

#endif
