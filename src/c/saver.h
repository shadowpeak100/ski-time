#ifndef SAVER_H
#define SAVER_H

#include <stdint.h>
#include <stdbool.h>

bool saver_active(bool enabled, uint16_t start, uint16_t end, int hour, int minute);

#endif
