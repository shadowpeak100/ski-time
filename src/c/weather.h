#ifndef WEATHER_H
#define WEATHER_H

#include <stdint.h>
#include <stdbool.h>
#include "daypart.h"
#include "theme.h"

typedef enum { WX_CLEAR = 0, WX_PARTLY, WX_OVERCAST, WX_FOG, WX_DRIZZLE, WX_RAIN, WX_SNOW, WX_STORM, WX_COUNT } WxKind;
typedef enum { PRECIP_NONE = 0, PRECIP_RAIN, PRECIP_SNOW } Precip;
typedef enum { WXSKY_CLEAR = 0, WXSKY_OVERCAST, WXSKY_STORM } WxSky;
typedef struct { uint8_t clouds; bool deck; WxSky sky; uint8_t fog256; Precip precip; uint8_t rate, len, speed; bool lightning; } WxDef;

const WxDef *wx_def(WxKind w);
WxKind       wx_from_wmo(int code);
bool         wx_moves(WxKind w);                       // precipitation or lightning
uint8_t      wx_sky(Daypart d, WxKind w, Col8 out[4]); // returns stop count
const Col8  *wx_cloud_roles(Daypart d, WxKind w);      // lit, mid, shade
Col8         wx_rain_colour(Daypart d);
Col8         wx_fog_colour(Daypart d);
uint8_t      wx_bg_palette(Daypart d, WxKind w, Col8 out[32]);

#endif
