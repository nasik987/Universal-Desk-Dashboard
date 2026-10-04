#include "DashboardData.h"
#include "Stopwatch.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
static JsonDocument json(const char* value) {JsonDocument d; assert(!deserializeJson(d,value));return d;}
int main(int argc,char** argv){
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
  std::cout<<"Data parsing, missing/stale data, hourly/timer rollover, stopwatch pause/resume/reset and rollover: PASS\n";
}
