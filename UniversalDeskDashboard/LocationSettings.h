#pragma once
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
struct AutoLocation {
  char city[64]={},zone[64]={},rule[64]={};
  float lat=0,lon=0;
  int offset=0;bool valid=false;
};
inline bool validCoordinates(float lat,float lon){return isfinite(lat)&&isfinite(lon)&&lat>=-90&&lat<=90&&lon>=-180&&lon<=180;}
inline void locationTimeRule(const char* zone,int offset,char* out,size_t size){
  const char* central[]={"Europe/Prague","Europe/Bratislava","Europe/Berlin","Europe/Paris","Europe/Rome","Europe/Warsaw","Europe/Vienna","Europe/Budapest","Europe/Madrid","Europe/Amsterdam"};
  for(auto name:central)if(!strcmp(zone,name)){snprintf(out,size,"CET-1CEST,M3.5.0,M10.5.0/3");return;}
  if(!strcmp(zone,"Europe/London")){snprintf(out,size,"GMT0BST,M3.5.0/1,M10.5.0");return;}
  const char* us[]={"America/New_York","America/Chicago","America/Denver","America/Los_Angeles"};
  for(int i=0;i<4;i++)if(!strcmp(zone,us[i])){snprintf(out,size,"STD%dDST,M3.2.0,M11.1.0",5+i);return;}
  // POSIX signs are opposite to UTC offsets. Other zones refresh every 15 minutes.
  int seconds=offset<0?-offset:offset;
  snprintf(out,size,"LOC%s%d:%02d:%02d",offset<0?"":"-",seconds/3600,seconds/60%60,seconds%60);
}
inline bool parseAutoLocation(JsonVariantConst doc,AutoLocation& out){
  if(!doc["success"].is<bool>()||!doc["success"].as<bool>()||!doc["latitude"].is<float>()||!doc["longitude"].is<float>()||!doc["timezone"]["offset"].is<int>())return false;
  const char* city=doc["city"]|"";const char* zone=doc["timezone"]["id"]|"";
  float lat=doc["latitude"],lon=doc["longitude"];int offset=doc["timezone"]["offset"];
  if(!*city||strlen(city)>=sizeof(out.city)||!*zone||strlen(zone)>=sizeof(out.zone)||!validCoordinates(lat,lon)||offset< -43200||offset>50400)return false;
  AutoLocation value;snprintf(value.city,sizeof(value.city),"%s",city);snprintf(value.zone,sizeof(value.zone),"%s",zone);
  value.lat=lat;value.lon=lon;value.offset=offset;value.valid=true;locationTimeRule(zone,offset,value.rule,sizeof(value.rule));out=value;return true;
}
