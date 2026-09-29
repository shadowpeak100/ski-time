#ifndef GLYPHS_H
#define GLYPHS_H
#include <stdint.h>
#include <stdbool.h>

// One skier's stroke: a cubic chain in the unit glyph box (x right, y down), n floats.
typedef struct { const float *pts; uint8_t n; } GlyphStroke;
// A digit placed in a slot: 0 = hour tens, 1 = hour ones, 2 = minute tens, 3 = minute ones.
typedef struct { uint8_t digit, slot; } DigitSlot;

uint8_t     glyph_stroke_count(uint8_t digit);
GlyphStroke glyph_stroke(uint8_t digit, uint8_t j);

// Digits to draw for a time. Without a leading zero, a one-digit hour sits in slot 1
// and slot 0 gets no skier. Returns the number of entries written (3 or 4).
uint8_t time_slots(int hour24, int minute, bool use24, bool lead_zero, DigitSlot out[4]);

#endif
