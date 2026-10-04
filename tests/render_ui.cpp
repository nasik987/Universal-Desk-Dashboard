#include "DeskUI.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cassert>
#include <algorithm>
struct Raster {
  std::vector<uint16_t> pixels=std::vector<uint16_t>(320*240);int overflow=0;
  void fillRect(int x,int y,int w,int h,uint16_t c){for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++){if(xx<0||xx>=320||yy<0||yy>=240){overflow++;continue;}pixels[yy*320+xx]=c;}}
  void drawLine(int x,int y,int x2,int y2,uint16_t c){int dx=abs(x2-x),sx=x<x2?1:-1,dy=-abs(y2-y),sy=y<y2?1:-1,e=dx+dy;for(;;){fillRect(x,y,1,1,c);if(x==x2&&y==y2)break;int e2=e*2;if(e2>=dy){e+=dy;x+=sx;}if(e2<=dx){e+=dx;y+=sy;}}}
  void save(const char* path){std::ofstream f(path,std::ios::binary);f<<"P6\n320 240\n255\n";for(uint16_t p:pixels){unsigned char rgb[]={static_cast<unsigned char>((p>>11)*255/31),static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};f.write((char*)rgb,3);}}
};
static JsonDocument fromFile(const char* path){std::ifstream f(path);std::stringstream b;b<<f.rdbuf();JsonDocument d;assert(!deserializeJson(d,b.str()));return d;}
int main(int argc,char** argv){
  WeatherData w;MakerData m;if(argc>=3){auto weather=fromFile(argv[1]);auto maker=fromFile(argv[2]);assert(parseWeather(weather,w,1));assert(parseMaker(maker,m,1));}
  DeskView v;v.clock="02:14";v.date="Sunday, 4 October";v.connected=true;v.oldData=true;v.ip="192.168.0.188";
  Raster raster;DeskRenderer<Raster> render(raster);char name[80];
  for(int i=0;i<6;i++){raster.overflow=0;render.render(i,v,w,m);assert(!raster.overflow);snprintf(name,sizeof(name),"tests/previews/page-%d.ppm",i);raster.save(name);}
  // Compare the production 80-row rendering strategy against the full raster.
  struct Stripes:Raster {int top=0;void fillRect(int x,int y,int width,int height,uint16_t color){int lo=std::max(y,top),hi=std::min(y+height,top+80);if(hi>lo)Raster::fillRect(x,lo,width,hi-lo,color);}
    void drawLine(int x,int y,int x2,int y2,uint16_t c){int dx=abs(x2-x),sx=x<x2?1:-1,dy=-abs(y2-y),sy=y<y2?1:-1,e=dx+dy;for(;;){fillRect(x,y,1,1,c);if(x==x2&&y==y2)break;int e2=2*e;if(e2>=dy){e+=dy;x+=sx;}if(e2<=dx){e+=dx;y+=sy;}}}
  };
  Stripes strips;DeskRenderer<Stripes> striped(strips);
  for(int page=0;page<6;page++){render.render(page,v,w,m);for(int top=0;top<240;top+=80){strips.top=top;striped.render(page,v,w,m);}assert(strips.pixels==raster.pixels);}
  v.stopwatchMode=true;v.stopwatchRunning=true;v.stopwatchElapsed=83420;
  render.render(3,v,w,m);assert(!raster.overflow);raster.save("tests/previews/stopwatch.ppm");
  for(uint64_t elapsed:{uint64_t(0),uint64_t(3599999),uint64_t(3600000),uint64_t(36000000000000)}){
    v.stopwatchElapsed=elapsed;
    for(bool running:{false,true}){v.stopwatchRunning=running;raster.overflow=0;render.render(3,v,w,m);assert(!raster.overflow);
      for(int top=0;top<240;top+=80){strips.top=top;striped.render(3,v,w,m);}assert(strips.pixels==raster.pixels);}
  }
  v.stopwatchMode=false;
  v.daily=true;render.render(2,v,w,m);assert(!raster.overflow);raster.save("tests/previews/daily.ppm");
  w=WeatherData();m=MakerData();v.clock="--:--";v.date="Waiting for time";for(int i=0;i<6;i++){raster.overflow=0;render.render(i,v,w,m);assert(!raster.overflow);}
  v.clock="23:59";v.city="A really long city name that must be clipped";render.render(0,v,w,m);assert(!raster.overflow);
  std::cout<<"Production UI renderer: dashboard/stopwatch layouts, empty data, long text and striped rendering: PASS\n";
}
