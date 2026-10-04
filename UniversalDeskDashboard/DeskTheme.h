#pragma once
#include <stdint.h>

// Logical RGB565 colors. DeviceSurface and direct TFT drawing apply the
// verified panel's color complement exactly once, at the display boundary.
namespace DeskTheme {
constexpr uint16_t rgb(unsigned r,unsigned g,unsigned b){return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
constexpr uint16_t background=rgb(16,17,20);
constexpr uint16_t panel=rgb(32,34,40);
constexpr uint16_t outline=rgb(48,83,130);
constexpr uint16_t text=rgb(244,245,247);
constexpr uint16_t muted=rgb(149,154,165);
constexpr uint16_t accent=rgb(75,145,255);
constexpr uint16_t line=rgb(52,56,65);
constexpr uint16_t success=rgb(72,207,151);
constexpr uint16_t warning=rgb(255,198,104);
constexpr uint16_t error=rgb(255,107,124);
constexpr uint16_t native(uint16_t logical){return static_cast<uint16_t>(~logical);}
constexpr uint16_t blend(uint16_t f,uint16_t b,unsigned a){
  return ((((f>>11)*a+(b>>11)*(255-a))/255)<<11)|
    (((((f>>5)&63)*a+((b>>5)&63)*(255-a))/255)<<5)|
    (((f&31)*a+(b&31)*(255-a))/255);
}
constexpr uint16_t accentColor(unsigned index,bool light=false){
  return index==1?rgb(72,207,151):index==2?rgb(169,124,255):index==3?rgb(255,178,75):index==4?(light?rgb(80,90,108):text):accent;
}
struct Palette {uint16_t background,panel,text,muted,accent,line,outline,onAccent;};
inline Palette palette(bool light,unsigned index){
  uint16_t bg=light?rgb(249,250,252):background,ac=accentColor(index,light);
  return {bg,light?rgb(236,239,244):panel,light?rgb(25,29,36):text,light?rgb(108,118,132):muted,ac,
    light?rgb(207,215,226):line,blend(ac,bg,light?105:140),index==1||index==3||(index==4&&!light)?background:text};
}
}
