#pragma once
#include <stdint.h>

// Accumulate unsigned tick deltas so elapsed time survives millis() rollover.
class Stopwatch {
  uint64_t total=0;
  uint32_t lastTick=0;
  bool active=false;
public:
  bool running() const {return active;}
  uint64_t elapsed(uint32_t now) const {return total+(active?uint32_t(now-lastTick):0);}
  void tick(uint32_t now){if(active)total+=uint32_t(now-lastTick);lastTick=now;}
  void start(uint32_t now){if(!active){lastTick=now;active=true;}}
  void pause(uint32_t now){tick(now);active=false;}
  void reset(uint32_t now){total=0;lastTick=now;active=false;}
};
