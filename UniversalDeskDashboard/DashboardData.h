#pragma once
#include <ArduinoJson.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

struct HourForecast { bool valid=false; char hour[6]="--:--"; float temp=0; int rain=-1, code=-1; };
struct DayForecast { bool valid=false; char date[11]=""; float high=0, low=0; int code=-1, rain=-1; };
struct WeatherData {
  bool valid=false, failed=false;
  float temp=0, feels=0, wind=0, tmax=0, tmin=0;
  int humidity=-1, code=-1;
  uint32_t updatedAt=0;
  char sunrise[6]="--:--", sunset[6]="--:--";
  HourForecast hours[6]; DayForecast days[5];
};
struct MakerSample {
  int64_t stamp=0, followers=-1, prints=-1, downloads=-1, likes=-1;
};
struct MakerData {
  bool valid=false, failed=false;
  uint32_t updatedAt=0;
  MakerSample latest, samples[7]; int count=0;
  char measured[21]="";
};

inline bool jsonNumber(JsonVariantConst v) { return v.is<double>() && isfinite(v.as<double>()); }
inline int64_t metric(JsonVariantConst v) { return v.is<int64_t>() && v.as<int64_t>()>=0 ? v.as<int64_t>() : -1; }
inline void copyClock(char* dest, const char* iso) {
  if(iso && strlen(iso)>=16 && iso[10]=='T') { memcpy(dest,iso+11,5); dest[5]=0; }
}
// Convert UTC ISO timestamps without depending on the device's configured timezone.
inline int64_t utcEpoch(const char* s) {
  if(!s || strlen(s)<20) return 0;
  int y,m,d,h,n,sec;
  if(sscanf(s,"%4d-%2d-%2dT%2d:%2d:%2d",&y,&m,&d,&h,&n,&sec)!=6) return 0;
  static const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
  if(y<1970 || y>2100 || m<1 || m>12 || d<1 || h<0 || h>23 || n<0 || n>59 || sec<0 || sec>59) return 0;
  int maxDay=lengths[m-1]+(m==2 && y%4==0 && (y%100!=0 || y%400==0));
  if(d>maxDay) return 0;
  const char* zone=s+19;
  if(*zone=='.') { ++zone; while(*zone>='0' && *zone<='9') ++zone; }
  int offset=0;
  if(*zone=='Z' && zone[1]==0) {}
  else if((*zone=='+' || *zone=='-') && strlen(zone)==6 && zone[3]==':') {
    int zh,zm; if(sscanf(zone+1,"%2d:%2d",&zh,&zm)!=2 || zh>23 || zm>59) return 0;
    offset=(zh*60+zm)*60*(*zone=='+'?1:-1);
  } else return 0;
  y-=m<=2; int era=y/400; unsigned yo=y-era*400;
  unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
  unsigned doe=yo*365+yo/4-yo/100+doy;
  int64_t days=(int64_t)era*146097+doe-719468;
  return days*86400+h*3600+n*60+sec-offset;
}

inline bool parseWeather(JsonVariantConst root, WeatherData& result, uint32_t now) {
  auto c=root["current"]; auto daily=root["daily"];
  if(!jsonNumber(c["temperature_2m"]) || !jsonNumber(daily["temperature_2m_max"][0]) || !jsonNumber(daily["temperature_2m_min"][0])) return false;
  WeatherData next;
  next.temp=c["temperature_2m"]; next.feels=c["apparent_temperature"] | next.temp;
  next.wind=c["wind_speed_10m"] | 0.0f; next.humidity=c["relative_humidity_2m"] | -1;
  next.code=c["weather_code"] | -1;
  next.tmax=daily["temperature_2m_max"][0]; next.tmin=daily["temperature_2m_min"][0];
  copyClock(next.sunrise,daily["sunrise"][0]); copyClock(next.sunset,daily["sunset"][0]);
  auto hourly=root["hourly"]; const char* current=c["time"] | "";
  int out=0;
  for(size_t i=0;i<hourly["time"].size() && out<6;i++) {
    const char* at=hourly["time"][i] | "";
    if(!*current || strcmp(at,current)<0 || !jsonNumber(hourly["temperature_2m"][i])) continue;
    auto& hour=next.hours[out++]; hour.valid=true; copyClock(hour.hour,at);
    hour.temp=hourly["temperature_2m"][i]; hour.rain=hourly["precipitation_probability"][i] | -1;
    hour.code=hourly["weather_code"][i] | -1;
  }
  for(int i=0;i<5;i++) {
    if(!jsonNumber(daily["temperature_2m_max"][i]) || !jsonNumber(daily["temperature_2m_min"][i])) continue;
    auto& day=next.days[i]; day.valid=true;
    snprintf(day.date,sizeof(day.date),"%.10s",daily["time"][i] | "");
    day.high=daily["temperature_2m_max"][i]; day.low=daily["temperature_2m_min"][i];
    day.code=daily["weather_code"][i] | -1; day.rain=daily["precipitation_probability_max"][i] | -1;
  }
  next.valid=true; next.updatedAt=now; result=next; return true;
}

inline void addMakerSample(MakerData& out, JsonVariantConst row) {
  const char* stamp=row["time_utc"] | row["updated_utc"] | row["timestamp"] | "";
  auto m=row["metrics"].is<JsonObjectConst>()?row["metrics"]:row;
  MakerSample s; s.stamp=utcEpoch(stamp);
  s.followers=metric(m["followerCount"].isNull()?m["followers"]:m["followerCount"]);
  s.prints=metric(m["printCount"].isNull()?m["prints"]:m["printCount"]);
  s.downloads=metric(m["downloadCount"].isNull()?m["downloads"]:m["downloadCount"]);
  s.likes=metric(m["likeCount"].isNull()?m["likes"]:m["likeCount"]);
  if(!s.stamp || (s.followers<0 && s.prints<0 && s.downloads<0 && s.likes<0)) return;
  if(!out.valid || s.stamp>out.latest.stamp) {
    out.latest=s; snprintf(out.measured,sizeof(out.measured),"%.19s",stamp); out.valid=true;
  }
  if(s.prints<0) return;
  for(int i=0;i<out.count;i++) if(out.samples[i].stamp==s.stamp) return;
  int pos=0; while(pos<out.count && out.samples[pos].stamp<s.stamp) ++pos;
  if(out.count==7) { if(pos==0) return; for(int i=1;i<7;i++) out.samples[i-1]=out.samples[i]; --out.count; --pos; }
  for(int i=out.count;i>pos;i--) out.samples[i]=out.samples[i-1];
  out.samples[pos]=s; ++out.count;
}
inline bool parseMaker(JsonVariantConst root, MakerData& result, uint32_t now) {
  MakerData next;
  if(root.is<JsonArrayConst>()) for(auto row:root.as<JsonArrayConst>()) addMakerSample(next,row);
  else if(root["history"].is<JsonArrayConst>()) for(auto row:root["history"].as<JsonArrayConst>()) addMakerSample(next,row);
  else addMakerSample(next,root);
  if(!next.valid) return false;
  next.updatedAt=now; result=next; return true;
}

inline uint32_t remainingSeconds(bool running, uint32_t deadline, uint32_t paused, uint32_t now) {
  if(!running) return paused;
  int32_t ms=(int32_t)(deadline-now);
  return ms>0?((uint32_t)ms+999)/1000:0;
}
