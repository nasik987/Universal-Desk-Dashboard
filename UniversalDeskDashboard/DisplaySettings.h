#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct DisplayConfig {
  uint8_t brightness=180,nightBrightness=40,nightStart=22,nightEnd=7,accentIndex=0;
  bool nightDim=false,lightTheme=false,twelveHour=false;
};
inline void sanitizeDisplay(DisplayConfig& c){
  if(c.brightness<20)c.brightness=20;
  if(c.nightBrightness<20)c.nightBrightness=20;
  if(c.nightStart>23)c.nightStart=22;
  if(c.nightEnd>23)c.nightEnd=7;
  if(c.accentIndex>4)c.accentIndex=0;
}
inline bool nightHours(int hour,uint8_t start,uint8_t end){
  if(hour<0||hour>23||start==end)return false;
  return start<end?(hour>=start&&hour<end):(hour>=start||hour<end);
}
inline uint8_t displayBrightness(const DisplayConfig& c,int hour){
  return c.nightDim&&nightHours(hour,c.nightStart,c.nightEnd)&&c.nightBrightness<c.brightness?c.nightBrightness:c.brightness;
}
inline uint8_t brightnessAt(int x){if(x<20)x=20;if(x>300)x=300;return 20+(x-20)*235/280;}
// Shared hit boxes for the native settings screens and their interaction tests.
inline bool visualSettingTap(DisplayConfig& c,int page,int x,int y){
  if(page==7){
    if(y>=53&&y<=85&&x>=154&&x<=294){c.lightTheme=x>=226;return true;}
    if(y>=133&&y<=175&&x>=20&&x<=300){int index=(x-20)/58;if(index<5&&(x-20)%58<48){c.accentIndex=index;return true;}}
  }else if(page==8){
    if(x>=14&&x<=306&&y>=60&&y<=80){c.brightness=brightnessAt(x);return true;}
    if(x>=16&&x<=304&&y>=87&&y<=121){c.nightDim=!c.nightDim;return true;}
    if(y>=129&&y<=163){if(x>=160&&x<=224){c.nightStart=(c.nightStart+1)%24;return true;}if(x>=232&&x<=296){c.nightEnd=(c.nightEnd+1)%24;return true;}}
    if(x>=14&&x<=306&&y>=182&&y<=202){c.nightBrightness=brightnessAt(x);return true;}
  }else if(page==9&&y>=53&&y<=91){
    if(x>=178&&x<=236){c.twelveHour=true;return true;}if(x>=242&&x<=300){c.twelveHour=false;return true;}
  }
  return false;
}
inline void formatDisplayClock(int hour,int minute,bool twelve,char* clock,size_t size,char* period,size_t periodSize){
  snprintf(clock,size,"%02d:%02d",twelve?(hour%12?hour%12:12):hour,minute);
  snprintf(period,periodSize,"%s",twelve?(hour>=12?"PM":"AM"):"");
}
struct CityPreset {const char* name;float lat,lon;};
static const CityPreset CityPresets[]={{"Brno",49.1951f,16.6068f},{"Praha",50.0755f,14.4378f},{"Ostrava",49.8209f,18.2625f},{"Plzeň",49.7384f,13.3736f},{"Bratislava",48.1486f,17.1077f}};
struct ZonePreset {const char* name;const char* rule;};
static const ZonePreset ZonePresets[]={{"Central EU","CET-1CEST,M3.5.0,M10.5.0/3"},{"UTC","UTC0"},{"London","GMT0BST,M3.5.0/1,M10.5.0"},{"New York","EST5EDT,M3.2.0,M11.1.0"}};
inline int cityPresetIndex(const char* name){for(int i=0;i<5;i++)if(!strcmp(name,CityPresets[i].name))return i;return -1;}
inline int zonePresetIndex(const char* rule){for(int i=0;i<4;i++)if(!strcmp(rule,ZonePresets[i].rule))return i;return -1;}
