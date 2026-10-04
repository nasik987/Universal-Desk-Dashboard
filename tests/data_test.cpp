#include "DashboardData.h"
#include "Stopwatch.h"
#include "DisplaySettings.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
static JsonDocument json(const char* value) {JsonDocument d; assert(!deserializeJson(d,value));return d;}
int main(int argc,char** argv){
  DisplayConfig cfg;
  assert(displayBrightness(cfg,23)==180);
  cfg.nightDim=true;
  assert(displayBrightness(cfg,22)==40&&displayBrightness(cfg,0)==40&&displayBrightness(cfg,6)==40);
  assert(displayBrightness(cfg,7)==180&&displayBrightness(cfg,21)==180&&displayBrightness(cfg,-1)==180);
  cfg.nightStart=8;cfg.nightEnd=18;
  assert(displayBrightness(cfg,8)==40&&displayBrightness(cfg,17)==40&&displayBrightness(cfg,18)==180);
  cfg.nightStart=cfg.nightEnd=8;assert(displayBrightness(cfg,8)==180);
  cfg.nightStart=22;cfg.nightEnd=7;cfg.brightness=30;assert(displayBrightness(cfg,23)==30);
  assert(brightnessAt(-20)==20&&brightnessAt(300)==255&&brightnessAt(400)==255);
  assert(!visualSettingTap(cfg,7,73,150)); // gap between color buttons
  assert(visualSettingTap(cfg,7,240,70)&&cfg.lightTheme);
  assert(visualSettingTap(cfg,7,170,70)&&!cfg.lightTheme);
  for(int i=0;i<5;i++){assert(visualSettingTap(cfg,7,20+i*58+24,150)&&cfg.accentIndex==i);}
  assert(visualSettingTap(cfg,8,300,70)&&cfg.brightness==255);
  assert(visualSettingTap(cfg,8,20,190)&&cfg.nightBrightness==20);
  cfg.nightStart=23;cfg.nightEnd=23;
  assert(visualSettingTap(cfg,8,190,145)&&cfg.nightStart==0);
  assert(visualSettingTap(cfg,8,260,145)&&cfg.nightEnd==0);
  bool wasNight=cfg.nightDim;assert(visualSettingTap(cfg,8,270,104)&&cfg.nightDim!=wasNight);
  assert(visualSettingTap(cfg,9,200,70)&&cfg.twelveHour);
  assert(visualSettingTap(cfg,9,260,70)&&!cfg.twelveHour);
  assert(!visualSettingTap(cfg,8,10,145)&&!visualSettingTap(cfg,9,260,105));
  cfg.accentIndex=99;cfg.nightStart=99;cfg.nightEnd=99;cfg.brightness=0;cfg.nightBrightness=0;
  sanitizeDisplay(cfg);assert(cfg.accentIndex==0&&cfg.nightStart==22&&cfg.nightEnd==7&&cfg.brightness==20&&cfg.nightBrightness==20);
  char clock[6],period[3];
  formatDisplayClock(0,4,true,clock,sizeof(clock),period,sizeof(period));assert(!strcmp(clock,"12:04")&&!strcmp(period,"AM"));
  formatDisplayClock(12,30,true,clock,sizeof(clock),period,sizeof(period));assert(!strcmp(clock,"12:30")&&!strcmp(period,"PM"));
  formatDisplayClock(23,59,true,clock,sizeof(clock),period,sizeof(period));assert(!strcmp(clock,"11:59")&&!strcmp(period,"PM"));
  formatDisplayClock(23,59,false,clock,sizeof(clock),period,sizeof(period));assert(!strcmp(clock,"23:59")&&!*period);
  assert(cityPresetIndex("Brno")==0&&cityPresetIndex("Custom city")==-1);
  assert(zonePresetIndex("UTC0")==1&&zonePresetIndex("custom")==-1);
  Stopwatch watch;
  assert(!watch.running() && watch.elapsed(500)==0);
  watch.start(1000);watch.start(1200);assert(watch.elapsed(2500)==1500);
  watch.pause(2500);assert(!watch.running() && watch.elapsed(9000)==1500);
  watch.start(10000);watch.tick(11000);assert(watch.elapsed(11500)==3000);
  watch.reset(12000);assert(!watch.running() && watch.elapsed(13000)==0);
  watch.start(0xfffffff0);watch.tick(0x10);assert(watch.elapsed(0x20)==48);
  watch.tick(0xfffffff0);watch.tick(0x10);assert(watch.elapsed(0x10)==uint64_t(0x100000020));
  watch.pause(0x20);assert(watch.elapsed(0x100)==uint64_t(0x100000030));
  assert(utcEpoch("2026-10-04T01:39:00+02:00")==utcEpoch("2026-10-03T23:39:00Z"));
  assert(utcEpoch("1970-01-01T00:00:01Z")==1);
  assert(!utcEpoch("2026-02-30T01:00:00Z"));assert(!utcEpoch("2026-10-03T23:39:00"));
  assert(remainingSeconds(true,1000,99,999)==1);
  assert(remainingSeconds(true,1000,99,1000)==0);
  assert(remainingSeconds(true,500,99,0xfffffff0)==1);
  assert(remainingSeconds(false,1000,120,2000)==120);
  auto bad=json("{}");MakerData m;assert(!parseMaker(bad,m,12));assert(m.latest.prints==-1);
  auto one=json(R"({"timestamp":"2026-10-03T23:00:00Z","prints":123,"followers":0,"likes":null})");
  assert(parseMaker(one,m,12));assert(m.latest.prints==123 && m.latest.followers==0 && m.latest.likes==-1);
  assert(!parseMaker(bad,m,13));assert(m.latest.prints==123 && m.updatedAt==12);
  auto shuffled=json(R"([{"time_utc":"2026-10-03T03:00:00Z","metrics":{"printCount":33}},{"time_utc":"2026-10-03T01:00:00Z","metrics":{"printCount":11}},{"time_utc":"2026-10-03T02:00:00Z","metrics":{"printCount":22}},{"time_utc":"2026-10-03T02:00:00Z","metrics":{"printCount":22}}])");
  assert(parseMaker(shuffled,m,14));assert(m.count==3 && m.latest.prints==33 && m.samples[0].prints==11);
  auto w=json(R"({"current":{"temperature_2m":13,"time":"2026-10-04T23:40","weather_code":0},"hourly":{"time":["2026-10-04T23:00","2026-10-05T00:00","2026-10-05T01:00"],"temperature_2m":[13,12,null],"precipitation_probability":[0,20,10]},"daily":{"time":["2026-10-04"],"temperature_2m_max":[21],"temperature_2m_min":[10],"sunrise":["2026-10-04T07:01"],"sunset":["2026-10-04T18:28"]}})");
  WeatherData weather;assert(parseWeather(w,weather,100));assert(weather.temp==13 && weather.hours[0].temp==12);
  assert(!strcmp(weather.hours[0].hour,"00:00") && !weather.hours[1].valid && weather.hours[0].rain==20);
  assert(!strcmp(weather.sunrise,"07:01") && weather.days[0].valid && !weather.days[1].valid);
  assert(!parseWeather(bad,weather,101) && weather.updatedAt==100);
  if(argc>1){std::ifstream f(argv[1]);std::stringstream body;body<<f.rdbuf();JsonDocument real;assert(!deserializeJson(real,body.str()));assert(parseMaker(real,m,200));assert(m.count==7 && m.latest.stamp==utcEpoch("2026-09-23T18:21:51+00:00"));for(int i=1;i<m.count;i++)assert(m.samples[i].stamp>m.samples[i-1].stamp);}
  std::cout<<"Data/timers/stopwatch, settings touch controls, brightness/night boundaries and clock format: PASS\n";
}
