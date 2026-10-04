#pragma once
#include <stdint.h>

// Logical RGB565 colors. DeviceSurface and direct TFT drawing apply the
// verified panel's color complement exactly once, at the display boundary.
namespace DeskTheme {
constexpr uint16_t rgb(unsigned r,unsigned g,unsigned b){return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
constexpr uint16_t background=rgb(16,17,20);
constexpr uint16_t panel=rgb(32,34,40);
constexpr uint16_t text=rgb(244,245,247);
constexpr uint16_t muted=rgb(149,154,165);
constexpr uint16_t accent=rgb(75,145,255);
constexpr uint16_t line=rgb(52,56,65);
constexpr uint16_t success=rgb(72,207,151);
constexpr uint16_t warning=rgb(255,198,104);
constexpr uint16_t error=rgb(255,107,124);
constexpr uint16_t native(uint16_t logical){return static_cast<uint16_t>(~logical);}
}
