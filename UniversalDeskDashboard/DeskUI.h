#pragma once
#include "DeskAssets.h"
#include "DeskTheme.h"
#include "DashboardData.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

struct DeskView {
  const char* clock="--:--"; const char* date="Waiting for time"; const char* city="Brno";
  const char* countdown="25:00"; const char* ip="";
  bool connected=false, daily=false, running=false, finished=false, oldData=false;
  uint32_t left=1500,preset=1500; int brightness=70;
};

// Shared production renderer: native previews and the ESP32 use these same pixels.
// Coordinates are 320 x 240. Text and icons are native-size alpha masks, never enlarged.
template<class Surface> class DeskRenderer {
  Surface& s;
  static uint16_t mix(uint8_t a,uint16_t f,uint16_t b){
    return ((((f>>11)*a+(b>>11)*(255-a)+127)/255)<<11)|
      (((((f>>5)&63)*a+((b>>5)&63)*(255-a)+127)/255)<<5)|
      (((f&31)*a+(b&31)*(255-a)+127)/255);
  }
  uint16_t paper=DeskTheme::background,panel=DeskTheme::panel,ink=DeskTheme::text,muted=DeskTheme::muted,blue=DeskTheme::accent,line=DeskTheme::line;
  static unsigned codepoint(const char*& p){
    unsigned c=(uint8_t)*p++;
    if(c>=0xC0 && c<0xE0 && *p){c=((c&31)<<6)|((uint8_t)*p++&63);}
    if(c>=0xE0 && c<0xF0 && p[0] && p[1]){c=((c&15)<<12)|(((uint8_t)p[0]&63)<<6)|((uint8_t)p[1]&63);p+=2;}
    return c;
  }
  static const AAGlyph* glyph(const AAFont& f,unsigned c){
    for(unsigned i=0;i<f.count;i++){if(f.glyphs[i].code==c)return &f.glyphs[i];}
    return nullptr;
  }
  void mask(const uint8_t* data,unsigned size,int width,int x,int y,uint16_t fg,uint16_t bg){
    auto alphaAt=[data](unsigned p){return (uint8_t)(((p&1)?data[p/2]&15:data[p/2]>>4)*17);};
    unsigned pixel=0;
    while(pixel<size*2){
      unsigned row=pixel/width,col=pixel%width;uint8_t alpha=alphaAt(pixel);
      unsigned run=1,maxRun=width-col;
      while(run<maxRun && pixel+run<size*2 && alphaAt(pixel+run)==alpha)++run;
      if(alpha)s.fillRect(x+col,y+row,run,1,mix(alpha,fg,bg));
      pixel+=run;
    }
  }
  void icon(const AAIcon& i,int x,int y,uint16_t fg,uint16_t bg){mask(i.data,i.size,i.width,x,y,fg,bg);}
  int width(const char* p,const AAFont& f){int w=0;while(*p){auto g=glyph(f,codepoint(p));if(g)w+=g->advance;}return w;}
  void text(const char* p,int x,int baseline,const AAFont& f,uint16_t fg,uint16_t bg,int limit=320){
    int cursor=x;while(*p){auto g=glyph(f,codepoint(p));if(!g)continue;if(cursor+g->advance>limit)break;
      if(g->w && g->h)mask(f.data+g->offset,g->size,g->w,cursor+g->left,baseline+g->top,fg,bg);
      cursor+=g->advance;
    }
  }
  void center(const char* p,int cx,int baseline,const AAFont& f,uint16_t fg,uint16_t bg){text(p,cx-width(p,f)/2,baseline,f,fg,bg);}
  void round(int x,int y,int w,int h,int r,uint16_t fg,uint16_t bg){
    s.fillRect(x+r,y,w-2*r,h,fg);
    for(int row=0;row<h;row++)for(int col=0;col<r;col++){
      float dx=r-col-.5f,dy=row<r?r-row-.5f:(row>=h-r?row-(h-r)+.5f:0);
      float coverage=r+.5f-sqrtf(dx*dx+dy*dy);if(coverage<=0)continue;
      uint16_t color=coverage>=1?fg:mix((uint8_t)(coverage*255),fg,bg);
      s.fillRect(x+col,y+row,1,1,color);s.fillRect(x+w-col-1,y+row,1,1,color);
    }
  }
  void dots(int page){for(int i=0;i<6;i++)round(132+i*10,231,i==page?6:4,4,2,i==page?blue:line,paper);}
  void header(const char* title){icon(IconBack24,12,10,muted,paper);center(title,160,28,FontTitle,ink,paper);}
  const AAIcon& weatherIcon(int code){
    if(code==0)return IconSun24;
    if(code>=1 && code<=3)return IconWeather24;
    if(code==45||code==48)return IconFog24;
    if((code>=71&&code<=77)||code==85||code==86)return IconSnow24;
    if(code>=95)return IconStorm24;
    if((code>=51&&code<=67)||(code>=80&&code<=82))return IconRain24;
    return IconCloud24;
  }
  const AAIcon& weatherSmall(int code){
    if(code==0)return IconSun16;
    if(code>=1 && code<=3)return IconWeather16;
    if(code==45||code==48)return IconFog16;
    if((code>=71&&code<=77)||code==85||code==86)return IconSnow16;
    if(code>=95)return IconStorm16;
    if((code>=51&&code<=67)||(code>=80&&code<=82))return IconRain16;
    return IconCloud16;
  }
  static void temp(char* out,size_t n,bool valid,float value){if(valid)snprintf(out,n,"%d°",(int)roundf(value));else snprintf(out,n,"--");}
  static void number(char* out,size_t n,int64_t value){
    if(value<0)snprintf(out,n,"--");else if(value>=1000000)snprintf(out,n,"%.1fM",value/1000000.0);
    else if(value>=100000)snprintf(out,n,"%.0fk",value/1000.0);else snprintf(out,n,"%lld",(long long)value);
  }
public:
  explicit DeskRenderer(Surface& surface):s(surface){}
  void render(int page,const DeskView& v,const WeatherData& w,const MakerData& m){
    s.fillRect(0,0,320,240,paper);
    char b[64],a[32];
    if(page==0){
      text(v.city,20,30,FontBody,muted,paper,240);
      if(v.connected)icon(IconWifi24,254,12,muted,paper);
      icon(IconSettings24,286,12,muted,paper);
      center(v.clock,160,148,FontClock,ink,paper);
      center(v.date,160,180,FontBody,muted,paper);
      if(w.valid){temp(b,sizeof(b),true,w.temp);int tw=width(b,FontBody);int x=160-(tw+36)/2;
        round(x-9,193,tw+54,27,13,panel,paper);icon(weatherIcon(w.code),x,194,blue,panel);text(b,x+34,212,FontBody,ink,panel);
      }
    }else if(page==1){
      header("");const AAIcon* icons[]={&IconWeather44,&IconFocus44,&IconStats44,&IconSettings44};
      for(int i=0;i<4;i++){int x=16+(i%2)*152,y=43+(i/2)*91;round(x,y,136,82,18,panel,paper);icon(*icons[i],x+46,y+19,blue,panel);}
    }else if(page==2){
      header(v.city);temp(b,sizeof(b),w.valid,w.temp);text(b,18,78,FontMetric,ink,paper);
      if(w.valid){temp(a,sizeof(a),true,w.feels);snprintf(b,sizeof(b),"Feels %s  ·  %d%% humidity",a,w.humidity);icon(weatherIcon(w.code),280,50,blue,paper);}
      else snprintf(b,sizeof(b),"Waiting for weather");
      text(b,18,98,FontSmall,muted,paper);
      if(!v.daily){
        round(12,110,296,94,16,panel,paper);
        for(int i=0;i<6;i++){auto h=w.hours[i];int x=37+i*49;snprintf(b,sizeof(b),h.valid?"%.2s":"--",h.hour);center(b,x,129,FontSmall,muted,panel);
          if(h.valid)icon(weatherIcon(h.code),x-12,136,blue,panel);
          temp(b,sizeof(b),h.valid,h.temp);center(b,x,180,FontBody,ink,panel);
          if(h.rain<0)snprintf(b,sizeof(b),"--");else snprintf(b,sizeof(b),"%d%%",h.rain);center(b,x,196,FontSmall,muted,panel);
        }
        snprintf(b,sizeof(b),"%s / %s",w.sunrise,w.sunset);text(b,18,222,FontSmall,muted,paper);icon(IconWind16,132,209,muted,paper);snprintf(b,sizeof(b),"%.0f km/h",w.wind);text(b,153,222,FontSmall,muted,paper);
      }else{
        round(12,110,296,94,16,panel,paper);
        for(int i=0;i<5;i++){auto d=w.days[i];int y=125+i*18;
          if(i==0)snprintf(b,sizeof(b),"Today");else snprintf(b,sizeof(b),"%.2s / %.2s",d.date+8,d.date+5);
          text(d.valid?b:"--",24,y,FontSmall,muted,panel);
          if(d.valid)icon(weatherSmall(d.code),99,y-13,blue,panel);
          temp(b,sizeof(b),d.valid,d.high);temp(a,sizeof(a),d.valid,d.low);
          text(b,145,y,FontSmall,ink,panel);text(a,189,y,FontSmall,muted,panel);
          if(d.rain<0)snprintf(b,sizeof(b),"--");else snprintf(b,sizeof(b),"%d%%",d.rain);text(b,258,y,FontSmall,muted,panel);
        }
      }
      text(v.daily?"Hourly  ›":"5 days  ›",253,222,FontSmall,blue,paper);
      if(w.failed)text("Update failed",18,222,FontSmall,muted,paper);
    }else if(page==3){
      header("Focus");center(v.countdown,160,121,FontFocus,ink,paper);
      center(v.running?"Focus time":(v.finished?"Complete":(v.left==v.preset?"Ready":"Paused")),160,146,FontBody,muted,paper);
      round(70,151,180,3,1,line,paper);if(v.preset && v.left){int n=(int)(180.0*v.left/v.preset);if(n>180)n=180;if(n<3)n=3;round(70,151,n,3,1,blue,line);}
      round(54,161,135,34,16,blue,paper);round(199,161,67,34,16,panel,paper);
      icon(v.running?IconPause24:IconPlay24,109,166,ink,blue);icon(IconReset24,220,166,muted,panel);
      const char* labels[]={"25 min","5 min","15 min"};int durations[]={1500,300,900};
      for(int i=0;i<3;i++){int x=32+i*90;round(x,202,76,25,12,panel,paper);center(labels[i],x+38,219,FontSmall,v.preset==(uint32_t)durations[i]?blue:muted,panel);}
    }else if(page==4){
      header("MakerWorld");text("Nasik",18,50,FontSmall,muted,paper);
      text(!m.valid?(m.failed?"Source unavailable":"Waiting for data"):(v.oldData?"Old data":(m.failed?"Update failed":"Latest measurement")),194,50,FontSmall,muted,paper);
      const AAIcon* icons[]={&IconPrinter24,&IconDownload24,&IconHeart24,&IconPeople24};int64_t values[]={m.latest.prints,m.latest.downloads,m.latest.likes,m.latest.followers};
      for(int i=0;i<4;i++){int x=12+(i%2)*152,y=60+(i/2)*56;round(x,y,144,50,14,panel,paper);icon(*icons[i],x+12,y+13,muted,panel);number(b,sizeof(b),values[i]);text(b,x+47,y+34,FontMetric,ink,panel,x+136);}
      text("Recent print totals",18,185,FontSmall,muted,paper);
      if(m.count>=2){int64_t lo=m.samples[0].prints,hi=lo;
        for(int i=1;i<m.count;i++){if(m.samples[i].prints<lo)lo=m.samples[i].prints;if(m.samples[i].prints>hi)hi=m.samples[i].prints;}
        int px=0,py=0;int64_t span=m.samples[m.count-1].stamp-m.samples[0].stamp;
        for(int i=0;i<m.count;i++){int x=18+(span?(int)((m.samples[i].stamp-m.samples[0].stamp)*284/span):0),y=hi==lo?204:213-(int)((m.samples[i].prints-lo)*19/(hi-lo));
          if(i)s.drawLine(px,py,x,y,blue);
          round(x-2,y-2,4,4,2,blue,paper);px=x;py=y;}
      }else text("More measurements needed",18,207,FontSmall,muted,paper);
      if(m.valid){snprintf(b,sizeof(b),"%.10s  %.5s UTC",m.measured,m.measured+11);text(b,18,225,FontSmall,muted,paper);}
    }else if(page==5){
      header("");const AAIcon* icons[]={&IconWifi24,&IconSun24,&IconStats24,&IconWeb24};
      const char* labels[]={"Wi-Fi",b,"MakerWorld","Web setup"};snprintf(b,sizeof(b),"%d%%",v.brightness);
      for(int i=0;i<4;i++){int x=16+(i%2)*152,y=43+(i/2)*75;round(x,y,136,66,16,panel,paper);icon(*icons[i],x+56,y+9,blue,panel);center(labels[i],x+68,y+56,FontSmall,muted,panel);}
      center(v.ip,160,209,FontSmall,muted,paper);
    }
    if(page==6){header("Web setup");center("Open on your phone",160,100,FontBody,muted,paper);center(v.ip,160,138,FontTitle,ink,paper);center("Tap to return",160,192,FontSmall,muted,paper);}
    if(page<6)dots(page);
  }
};
