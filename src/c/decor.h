#pragma once

#include <stdint.h>

// behind = 1: some skier's feet pass under the canopy, so the tree is drawn each frame over the skiers;
// behind = 0: the tree is painted once into the stored background.
typedef struct { int16_t x, base, h; uint8_t behind; } SlopeTree;
#define SLOPE_TREE_COUNT 44
extern const SlopeTree SLOPE_TREES[SLOPE_TREE_COUNT];   // Emery pixel coordinates, back to front by base
