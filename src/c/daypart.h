#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum { DAY_SUNRISE = 0, DAY_MIDDAY = 1, DAY_SUNSET = 2, DAY_NIGHT = 3 } Daypart;
typedef enum { DPM_FOLLOW = 0, DPM_FIXED = 1, DPM_ALWAYS_DAY = 2 } DaypartMode;

Daypart daypart_at(int minute_of_day, uint16_t sunrise, uint16_t sunset);
Daypart daypart_for(uint8_t mode, int minute_of_day, uint16_t phone_rise, uint16_t phone_set,
                    uint16_t fixed_rise, uint16_t fixed_set);
float moon_phase(int year, int month, int day, int hour, int minute);   // 0..1, 0 = new, 0.5 = full
