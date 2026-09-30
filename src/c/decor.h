#pragma once

#include <stdint.h>

typedef struct { int16_t x, base, h; } SlopeTree;
#define SLOPE_TREE_COUNT 9
extern const SlopeTree SLOPE_TREES[SLOPE_TREE_COUNT];   // Emery pixel coordinates
