#include "glyphs.h"

static const float G0A[] = {.50f,.02f, .15f,.02f,.02f,.55f,.22f,.86f, .36f,1.06f,.66f,1.00f,.88f,.93f};
static const float G0B[] = {.56f,.02f, .92f,.04f,1.00f,.55f,.78f,.86f, .64f,1.06f,.34f,1.00f,.12f,.93f};
static const float G1[]  = {.84f,.02f, .72f,.02f,.64f,.12f,.60f,.30f, .55f,.55f,.50f,.78f,.46f,.90f, .42f,1.00f,.32f,1.02f,.22f,.96f};
static const float G2[]  = {.16f,.20f, .20f,.03f,.46f,-.02f,.64f,.04f, .86f,.12f,.86f,.36f,.66f,.52f, .46f,.68f,.24f,.80f,.14f,.90f, .06f,.98f,.14f,1.04f,.30f,.96f, .44f,.90f,.62f,.98f,.92f,.96f};
static const float G3[]  = {.18f,.12f, .30f,-.02f,.72f,-.04f,.76f,.16f, .80f,.34f,.60f,.44f,.42f,.46f, .34f,.47f,.34f,.52f,.46f,.50f, .70f,.48f,.88f,.60f,.84f,.76f, .80f,.96f,.46f,1.04f,.10f,.88f};
static const float G4A[] = {.54f,.00f, .40f,.22f,.22f,.44f,.10f,.58f, .04f,.66f,.08f,.70f,.20f,.66f, .44f,.62f,.70f,.60f,.94f,.62f};
static const float G4B[] = {.76f,.08f, .72f,.40f,.66f,.72f,.60f,.90f, .56f,1.00f,.46f,1.02f,.36f,.96f};
static const float G5[]  = {.90f,.02f, .70f,.05f,.50f,.02f,.36f,.04f, .31f,.18f,.29f,.32f,.27f,.44f, .46f,.34f,.86f,.40f,.84f,.68f, .82f,.96f,.46f,1.04f,.10f,.88f};
static const float G6[]  = {.80f,.04f, .46f,.10f,.18f,.44f,.20f,.74f, .22f,1.00f,.70f,1.04f,.80f,.78f, .88f,.56f,.60f,.46f,.42f,.54f, .26f,.62f,.16f,.80f,.04f,.98f};
static const float G7[]  = {.10f,.08f, .30f,.00f,.60f,.06f,.84f,.03f, .97f,.02f,.94f,.12f,.86f,.20f, .70f,.38f,.50f,.62f,.40f,.86f, .36f,.98f,.26f,1.02f,.16f,.95f};
static const float G8A[] = {.50f,.02f, .16f,.02f,.14f,.38f,.50f,.50f, .86f,.62f,.90f,1.00f,.50f,1.00f, .30f,1.00f,.18f,.94f,.12f,.86f};
static const float G8B[] = {.54f,.02f, .88f,.02f,.86f,.38f,.50f,.50f, .14f,.62f,.10f,1.00f,.50f,1.00f, .70f,1.00f,.82f,.94f,.88f,.86f};
static const float G9[]  = {.80f,.06f, .62f,-.04f,.24f,-.02f,.18f,.22f, .12f,.46f,.58f,.54f,.78f,.30f, .84f,.20f,.94f,.14f,.90f,.30f, .84f,.56f,.72f,.82f,.64f,.94f, .56f,1.04f,.44f,1.02f,.34f,.95f};

#define S(a) { a, (uint8_t)(sizeof(a) / sizeof(float)) }
static const GlyphStroke STROKES[10][2] = {
  { S(G0A), S(G0B) }, { S(G1), {0, 0} }, { S(G2), {0, 0} }, { S(G3), {0, 0} }, { S(G4A), S(G4B) },
  { S(G5), {0, 0} }, { S(G6), {0, 0} }, { S(G7), {0, 0} }, { S(G8A), S(G8B) }, { S(G9), {0, 0} },
};

uint8_t glyph_stroke_count(uint8_t d) { return d > 9 ? 0 : (STROKES[d][1].n ? 2 : 1); }
GlyphStroke glyph_stroke(uint8_t d, uint8_t j) { return STROKES[d][j]; }

uint8_t time_slots(int h24, int m, bool use24, bool lead0, DigitSlot out[4]) {
  int hh = use24 ? h24 : (h24 % 12 == 0 ? 12 : h24 % 12);
  uint8_t n = 0;
  if (hh >= 10 || lead0) out[n++] = (DigitSlot){ (uint8_t)(hh / 10), 0 };
  out[n++] = (DigitSlot){ (uint8_t)(hh % 10), 1 };
  out[n++] = (DigitSlot){ (uint8_t)(m / 10), 2 };
  out[n++] = (DigitSlot){ (uint8_t)(m % 10), 3 };
  return n;
}
