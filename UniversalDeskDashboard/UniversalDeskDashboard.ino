/* Universal Desk Dashboard v0.3 - CYD ESP32-2432S028 */
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include "DashboardData.h"
#include "DeskUI.h"
#include "Stopwatch.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

TFT_eSPI lcd = TFT_eSPI();

#define T_CS   33
#define T_IRQ  36
#define T_CLK  25
#define T_DIN  32
#define T_DOUT 39
XPT2046_Touchscreen ts(T_CS, T_IRQ);

const int TOUCH_X_MIN = 200;
const int TOUCH_X_MAX = 3900;
const int TOUCH_Y_MIN = 200;
const int TOUCH_Y_MAX = 3900;

const int MAX_NETWORKS = 20;
String wifiSSIDs[MAX_NETWORKS];
int wifiCount = 0;
int wifiOffset = 0;
String selectedSSID;
String passwordBuffer;
bool keyboardNumbers = false;
bool keyboardShift = false;
bool showPassword = false;
Preferences prefs;
WebServer server(80);
DNSServer dns;

enum Screen { HOME, APPS, WEATHER, TIMER, MAKERWORLD, SETTINGS, WIFI_LIST, WIFI_KEYBOARD, WEB_INFO };
Screen currentScreen = HOME;

String cfgSsid, cfgPass;
String cfgCity = "Brno";
String cfgTZ = "CET-1CEST,M3.5.0,M10.5.0/3";
String cfgCustomLink = "https://example.com";
String cfgMakerURL = "https://raw.githubusercontent.com/nasik987/nasik-makerworld-monitor/main/data/history.json";
uint8_t cfgBrightness=180;
bool weatherDaily=false;
QueueHandle_t weatherQueue, makerQueue;
TaskHandle_t networkTaskHandle=nullptr;
float cfgLat = 49.1951;
float cfgLon = 16.6068;

bool setupMode = false;
const char* AP_NAME = "DeskDash-Setup";
IPAddress apIP(192,168,4,1);

WeatherData weather;
MakerData maker;
const unsigned long WEATHER_INTERVAL = 15UL * 60UL * 1000UL;
bool timerRunning = false;
unsigned long timerEndsAt = 0;
uint32_t timerDurationSec = 25 * 60;
unsigned long lastDraw = 0, lastTouch = 0;
uint32_t timerPresetSec=25*60;
bool timerFinished=false;
bool stopwatchMode=false;
Stopwatch stopwatch;
int lastMinuteDrawn = -1;

// Preserve the verified ST7789 inversion setting used on this CYD2USB.
// This panel displays the complement of RGB565; neutral cards stay neutral.
static const uint16_t C_BG=DeskTheme::native(DeskTheme::background), C_PANEL=DeskTheme::native(DeskTheme::panel), C_PANEL2=DeskTheme::native(DeskTheme::line);
static const uint16_t C_TEXT=DeskTheme::native(DeskTheme::text), C_MUTED=DeskTheme::native(DeskTheme::muted), C_CYAN=DeskTheme::native(DeskTheme::accent);
static const uint16_t C_GREEN=DeskTheme::native(DeskTheme::success), C_YELLOW=DeskTheme::native(DeskTheme::warning);
static const uint16_t C_BLUE=DeskTheme::native(DeskTheme::accent), C_RED=DeskTheme::native(DeskTheme::error);

void drawTimer();

void loadPrefs(){
  prefs.begin("desk-dash", true);
  cfgSsid=prefs.getString("ssid",""); cfgPass=prefs.getString("pass","");
  cfgCity=prefs.getString("city","Brno"); cfgTZ=prefs.getString("tz","CET-1CEST,M3.5.0,M10.5.0/3");
  cfgCustomLink=prefs.getString("link","https://example.com");
  cfgLat=prefs.getFloat("lat",49.1951f); cfgLon=prefs.getFloat("lon",16.6068f);
  cfgMakerURL=prefs.getString("makerurl",cfgMakerURL);
  cfgBrightness=prefs.getUChar("brightness",180);
  prefs.end();
}

void savePrefs(){
  prefs.begin("desk-dash",false);
  prefs.putString("ssid",cfgSsid); prefs.putString("pass",cfgPass); prefs.putString("city",cfgCity);
  prefs.putString("tz",cfgTZ); prefs.putString("link",cfgCustomLink);
  prefs.putFloat("lat",cfgLat); prefs.putFloat("lon",cfgLon);
  prefs.putString("makerurl",cfgMakerURL); prefs.putUChar("brightness",cfgBrightness); prefs.end();
}

String esc(String s){
  s.replace("&","&amp;");
  s.replace("<","&lt;");
  s.replace(">","&gt;");
  s.replace("\"","&quot;");
  return s;
}

String setupPage(){
  String ip=setupMode?WiFi.softAPIP().toString():WiFi.localIP().toString();
  String h="<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>Desk Dashboard</title>";
  h+="<style>body{color-scheme:dark;font-family:system-ui;background:#101114;color:#f4f5f7;padding:24px}.w{max-width:620px;margin:auto}.c{background:#202228;border:1px solid #343841;border-radius:18px;padding:20px;margin:0 0 16px}input{width:100%;box-sizing:border-box;padding:12px;border-radius:10px;border:1px solid #343841;background:#101114;color:#f4f5f7}label{display:block;margin:12px 0 5px}button{width:100%;padding:13px;border:0;border-radius:11px;background:#4b91ff;color:white;font-weight:700;margin-top:16px}</style></head><body><div class='w'>";
  h+="<div class='c'><h1>Universal Desk Dashboard</h1><p>Device IP: "+ip+"</p></div><form method='POST' action='/save'>";
  h+="<div class='c'><h2>Wi-Fi</h2><label>SSID</label><input name='ssid' value='"+esc(cfgSsid)+"'><label>Password</label><input type='password' name='pass' value='"+esc(cfgPass)+"'></div>";
  h+="<div class='c'><h2>Location & time</h2><label>City</label><input name='city' value='"+esc(cfgCity)+"'><label>Latitude</label><input name='lat' value='"+String(cfgLat,5)+"'><label>Longitude</label><input name='lon' value='"+String(cfgLon,5)+"'><label>POSIX timezone</label><input name='tz' value='"+esc(cfgTZ)+"'></div>";
  h+="<div class='c'><h2>MakerWorld</h2><p>Public HTTPS JSON endpoint. Old measurements are labelled on screen.</p><label>Stats URL</label><input name='makerurl' value='"+esc(cfgMakerURL)+"'><label>Brightness (20-255)</label><input type='number' min='20' max='255' name='brightness' value='"+String(cfgBrightness)+"'></div>";
  h+="<div class='c'><h2>Custom</h2><label>Custom link</label><input name='link' value='"+esc(cfgCustomLink)+"'><button type='submit'>Save & restart</button></div></form></div></body></html>";
  return h;
}

void startWebServer(){
  server.on("/",HTTP_GET,[]{server.send(200,"text/html; charset=utf-8",setupPage());});
  server.on("/save",HTTP_POST,[]{
    if(server.hasArg("ssid"))cfgSsid=server.arg("ssid"); if(server.hasArg("pass"))cfgPass=server.arg("pass");
    if(server.hasArg("city"))cfgCity=server.arg("city"); if(server.hasArg("lat"))cfgLat=server.arg("lat").toFloat();
    if(server.hasArg("lon"))cfgLon=server.arg("lon").toFloat(); if(server.hasArg("tz"))cfgTZ=server.arg("tz");
    if(server.hasArg("makerurl")) {
      String u=server.arg("makerurl"); u.trim();
      if(u.length()>256 || (u.length() && !u.startsWith("https://"))) {server.send(400,"text/plain","Use a public https:// JSON URL (max 256 characters)."); return;}
      cfgMakerURL=u;
    }
    if(server.hasArg("brightness"))cfgBrightness=constrain(server.arg("brightness").toInt(),20,255);
    if(server.hasArg("link"))cfgCustomLink=server.arg("link"); savePrefs();
    server.send(200,"text/html","<h2>Saved. Restarting...</h2>"); delay(700); ESP.restart();
  });
  server.onNotFound([](){if(setupMode){server.sendHeader("Location","http://192.168.4.1/",true);server.send(302,"text/plain","");}else server.send(404,"text/plain","Not found");});
  server.begin();
}

void connectWiFi(){
  setupMode=cfgSsid.length()==0;
  if(!setupMode){WiFi.mode(WIFI_STA);WiFi.begin(cfgSsid.c_str(),cfgPass.c_str());unsigned long s=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-s<12000)delay(250);setupMode=WiFi.status()!=WL_CONNECTED;}
  if(setupMode){WiFi.mode(WIFI_AP);WiFi.softAPConfig(apIP,apIP,IPAddress(255,255,255,0));WiFi.softAP(AP_NAME);dns.start(53,"*",apIP);} startWebServer();
}

String weatherText(int c){if(c<0)return"Unknown";if(c==0)return"Clear";if(c<=3)return"Partly cloudy";if(c==45||c==48)return"Fog";if((c>=51&&c<=67)||(c>=80&&c<=82))return"Rain";if((c>=71&&c<=77)||c==85||c==86)return"Snow";if(c>=95)return"Thunderstorm";return"Weather";}

class BoundedJSONReader {
  Stream& stream; size_t left=65536;
public:
  explicit BoundedJSONReader(Stream& s):stream(s){}
  int read(){char c;if(!left || !stream.readBytes(&c,1))return -1;--left;return (uint8_t)c;}
  size_t readBytes(char* b,size_t n){n=n<left?n:left;size_t got=stream.readBytes(b,n);left-=got;return got;}
};

bool fetchJSON(const String& url, JsonDocument& doc) {
  if(WiFi.status()!=WL_CONNECTED) return false;
  WiFiClientSecure client; client.setInsecure(); client.setHandshakeTimeout(5);
  HTTPClient http; http.setConnectTimeout(5000); http.setTimeout(5000);
  http.useHTTP10(true); // Stream a decoded response without HTTP chunk framing.
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if(!http.begin(client,url)) return false;
  bool ok=false;
  if(http.GET()==200) {
    int size=http.getSize();
    // Bound allocations on a 4 MB ESP32; oversized custom feeds are rejected.
    if(size<=65536) {
      http.getStream().setTimeout(5000);
      BoundedJSONReader body(http.getStream());
      ok=!deserializeJson(doc,body);
    }
  }
  http.end(); return ok;
}

void networkTask(void*) {
  WeatherData cachedWeather; MakerData cachedMaker;
  const String makerURL=cfgMakerURL; const float lat=cfgLat,lon=cfgLon;
  uint32_t nextWeather=0, nextMaker=0;
  bool first=true;
  for(;;) {
    uint32_t now=millis();
    if(WiFi.status()==WL_CONNECTED) {
      if(first || (int32_t)(now-nextWeather)>=0) {
        JsonDocument doc;
        String url="https://api.open-meteo.com/v1/forecast?latitude="+String(lat,5)+"&longitude="+String(lon,5)+"&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m&hourly=temperature_2m,precipitation_probability,weather_code&daily=temperature_2m_max,temperature_2m_min,weather_code,precipitation_probability_max,sunrise,sunset&timezone=auto&forecast_days=5&forecast_hours=12";
        bool ok=fetchJSON(url,doc) && parseWeather(doc.as<JsonVariantConst>(),cachedWeather,millis());
        cachedWeather.failed=!ok;
        xQueueOverwrite(weatherQueue,&cachedWeather);
        nextWeather=millis()+(ok?WEATHER_INTERVAL:120000UL);
      }
      if(makerURL.length() && (first || (int32_t)(now-nextMaker)>=0)) {
        JsonDocument doc;
        bool ok=fetchJSON(makerURL,doc) && parseMaker(doc.as<JsonVariantConst>(),cachedMaker,millis());
        cachedMaker.failed=!ok;
        xQueueOverwrite(makerQueue,&cachedMaker);
        nextMaker=millis()+(ok?1800000UL:120000UL);
      }
      first=false;
    }
    if(ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000))) first=true;
  }
}

void card(int x,int y,int w,int h,uint16_t color=C_PANEL){
  lcd.fillRoundRect(x,y,w,h,16,color);
}

void appleHeader(const String &title,bool back=false){
  lcd.fillRect(0,0,320,42,C_BG);
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(title,160,21);

  if(back){
    lcd.setTextDatum(ML_DATUM);
    lcd.setTextFont(4);
    lcd.setTextColor(C_BLUE);
    lcd.drawString("<",10,20);
  }
}

void textAt(const String& value,int x,int y,int font=2,uint16_t color=C_TEXT) {
  lcd.setTextDatum(TL_DATUM);lcd.setTextFont(font);lcd.setTextColor(color);lcd.drawString(value,x,y);
}
uint32_t timerLeft(){return remainingSeconds(timerRunning,timerEndsAt,timerDurationSec,millis());}
String timerText(){uint32_t r=timerLeft();char b[10];snprintf(b,sizeof(b),"%02lu:%02lu",(unsigned long)(r/60),(unsigned long)(r%60));return b;}
bool makerOld(){time_t now=time(nullptr);return maker.valid && (now<1700000000 || now-maker.latest.stamp>36*3600 || maker.latest.stamp>now+300);}

// Render completed 80-row strips before sending them. No white clearing flash,
// and only 50 KiB RAM is reserved, leaving space for Wi-Fi/TLS and JSON.
class DeviceSurface {
  TFT_eSprite stripe;
  bool buffered=false;
public:
  int top=0;
  DeviceSurface():stripe(&lcd){}
  void begin(){stripe.setColorDepth(16);buffered=stripe.createSprite(320,80)!=nullptr;}
  void fillRect(int x,int y,int w,int h,uint16_t logical){
    int lo=max(y,top),hi=min(y+h,top+80);if(hi<=lo)return;
    uint16_t nativeColor=DeskTheme::native(logical); // Match the verified inverted CYD2USB panel.
    if(buffered)stripe.fillRect(x,lo-top,w,hi-lo,nativeColor);
    else lcd.fillRect(x,lo,w,hi-lo,nativeColor);
  }
  void drawLine(int x,int y,int x2,int y2,uint16_t color){
    int dx=abs(x2-x),sx=x<x2?1:-1,dy=-abs(y2-y),sy=y<y2?1:-1,e=dx+dy;
    for(;;){fillRect(x,y,1,1,color);if(x==x2 && y==y2)break;int e2=2*e;if(e2>=dy){e+=dy;x+=sx;}if(e2<=dx){e+=dx;y+=sy;}}
  }
  void present(){if(buffered)stripe.pushSprite(0,top);}
} surface;

void drawPage(int page){
  DeskView v;struct tm t;char clock[6]="--:--",date[32]="Waiting for time";
  if(getLocalTime(&t,5)){strftime(clock,sizeof(clock),"%H:%M",&t);strftime(date,sizeof(date),"%A, %d %B",&t);}
  String countdown=timerText();String ip=setupMode?WiFi.softAPIP().toString():WiFi.localIP().toString();
  v.clock=clock;v.date=date;v.city=cfgCity.c_str();v.countdown=countdown.c_str();v.ip=ip.c_str();
  v.connected=WiFi.status()==WL_CONNECTED;v.daily=weatherDaily;v.running=timerRunning;v.finished=timerFinished;
  v.oldData=makerOld();v.left=timerLeft();v.preset=timerPresetSec;v.brightness=cfgBrightness*100/255;
  v.stopwatchMode=stopwatchMode;v.stopwatchRunning=stopwatch.running();v.stopwatchElapsed=stopwatch.elapsed(millis());
  DeskRenderer<DeviceSurface> renderer(surface);
  for(int top=0;top<240;top+=80){surface.top=top;renderer.render(page,v,weather,maker);surface.present();}
}
void drawHome(){drawPage(0);}
void drawApps(){drawPage(1);}
void drawWeather(){drawPage(2);}
void drawTimer(){drawPage(3);}
void drawMakerWorld(){drawPage(4);}
void drawSettings(){drawPage(5);}
void drawWebInfo(){drawPage(6);}

void scanWifiNetworks(){
  lcd.fillScreen(C_BG);
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString("Scanning Wi-Fi...",160,110);

  WiFi.mode(WIFI_STA);
  if(WiFi.status()!=WL_CONNECTED) WiFi.disconnect(false);
  delay(120);

  int n=WiFi.scanNetworks(false,true);
  wifiCount=(n>0)?min(n,MAX_NETWORKS):0;
  for(int i=0;i<wifiCount;i++) wifiSSIDs[i]=WiFi.SSID(i);
  WiFi.scanDelete();
  wifiOffset=0;
}

void drawWifiList(){
  lcd.fillScreen(C_BG);
  appleHeader("Choose Wi-Fi",true);

  if(wifiCount==0){
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextFont(2);
    lcd.setTextColor(C_MUTED);
    lcd.drawString("No networks found",160,112);
    lcd.fillRoundRect(105,151,110,34,16,C_PANEL);
    lcd.setTextColor(C_BLUE);
    lcd.drawString("Refresh",160,168);
    return;
  }

  int shown=0;
  for(int i=wifiOffset;i<wifiCount && shown<5;i++,shown++){
    int y=48+shown*36;
    lcd.fillRoundRect(10,y,300,31,12,C_PANEL);
    lcd.setTextDatum(ML_DATUM);
    lcd.setTextFont(2);
    lcd.setTextColor(C_TEXT);
    String s=wifiSSIDs[i];
    if(s.length()>22) s=s.substring(0,19)+"...";
    lcd.drawString(s,26,y+15);
    lcd.setTextDatum(MR_DATUM);
    lcd.setTextColor(C_MUTED);
    lcd.drawString(">",296,y+15);
  }

  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(1);
  if(wifiOffset>0){
    lcd.setTextColor(C_BLUE);
    lcd.drawString("UP",76,231);
  }
  lcd.setTextColor(C_BLUE);
  lcd.drawString("REFRESH",160,231);
  if(wifiOffset+5<wifiCount) lcd.drawString("DOWN",250,231);
}

void drawWifiKeyboard(){
  lcd.fillScreen(C_BG);
  appleHeader("Enter Password",true);

  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  String ss=selectedSSID;
  if(ss.length()>28) ss=ss.substring(0,25)+"...";
  lcd.drawString(ss,160,38);

  lcd.fillRoundRect(10,46,300,27,12,C_PANEL);
  lcd.setTextDatum(ML_DATUM);
  lcd.setTextColor(C_TEXT);
  String shown=showPassword?passwordBuffer:String("");
  if(!showPassword) for(size_t i=0;i<passwordBuffer.length();i++) shown+="*";
  if(shown.length()>25) shown=shown.substring(shown.length()-25);
  lcd.drawString(shown,18,59);

  lcd.setTextDatum(MC_DATUM);
  lcd.setTextColor(C_BLUE);
  lcd.drawString(showPassword?"Hide":"Show",280,59);

  const char* rowsAlpha[]={"qwertyuiop","asdfghjkl","zxcvbnm"};
  const char* rowsNum[]={"1234567890","!@#$%^&*(/",")-_+=.,?"};

  for(int r=0;r<3;r++){
    const char* row=keyboardNumbers?rowsNum[r]:rowsAlpha[r];
    int len=strlen(row);
    for(int i=0;i<len;i++){
      int bx=i*29+2, by=78+r*30;
      lcd.fillRoundRect(bx,by,26,26,6,C_PANEL);
      char ch=row[i];
      if(keyboardShift && !keyboardNumbers) ch=toupper(ch);
      lcd.setTextColor(C_TEXT);
      lcd.drawString(String(ch),bx+13,by+13);
    }
  }

  lcd.fillRoundRect(44,168,232,25,8,C_PANEL);
  lcd.setTextColor(C_TEXT);
  lcd.drawString("space",160,181);

  int by=198;
  lcd.fillRoundRect(2,by,58,35,9,C_PANEL2);
  lcd.fillRoundRect(66,by,58,35,9,C_PANEL2);
  lcd.fillRoundRect(130,by,58,35,9,C_PANEL2);
  lcd.fillRoundRect(194,by,58,35,9,C_PANEL2);
  lcd.fillRoundRect(258,by,60,35,9,C_BLUE);

  lcd.setTextFont(1);
  lcd.setTextColor(C_TEXT);
  lcd.drawString("Shift",31,216);
  lcd.drawString("123",95,216);
  lcd.drawString("Del",159,216);
  lcd.drawString("Back",223,216);
  lcd.drawString("Connect",288,216);
}

void showWifiResult(bool ok){
  lcd.fillScreen(C_BG);
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(2);
  lcd.setTextColor(ok?C_GREEN:C_YELLOW);
  lcd.drawString(ok?"Wi-Fi connected":"Connection failed",160,108);
  delay(ok?900:1500);
}

void connectSelectedWifi(){
  lcd.fillScreen(C_BG);
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString("Connecting to",160,88);
  lcd.setTextColor(C_CYAN);
  lcd.drawString(selectedSSID,160,113);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString("Please wait...",160,141);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(selectedSSID.c_str(),passwordBuffer.c_str());

  unsigned long started=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-started<15000) delay(250);

  if(WiFi.status()==WL_CONNECTED){
    cfgSsid=selectedSSID;
    cfgPass=passwordBuffer;
    savePrefs();
    setupMode=false;
    configTzTime(cfgTZ.c_str(),"pool.ntp.org","time.nist.gov");
    if(networkTaskHandle)xTaskNotifyGive(networkTaskHandle);
    showWifiResult(true);
    currentScreen=HOME;
    redraw();
  }else{
    showWifiResult(false);
    scanWifiNetworks();
    currentScreen=WIFI_LIST;
    drawWifiList();
  }
}

void redraw(){if(currentScreen==HOME)drawHome();else if(currentScreen==APPS)drawApps();else if(currentScreen==WEATHER)drawWeather();else if(currentScreen==TIMER)drawTimer();else if(currentScreen==MAKERWORLD)drawMakerWorld();else if(currentScreen==SETTINGS)drawSettings();else if(currentScreen==WIFI_LIST)drawWifiList();else if(currentScreen==WEB_INFO)drawWebInfo();else drawWifiKeyboard();lastDraw=millis();}
void setTimerMinutes(int m){timerPresetSec=m*60;timerDurationSec=timerPresetSec;timerRunning=false;timerFinished=false;drawTimer();}

bool readTouch(uint16_t &x, uint16_t &y){
  if(!ts.touched()) return false;
  TS_Point p = ts.getPoint();
  int tx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, 320);
  int ty = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 240);
  x = constrain(tx, 0, 319);
  y = constrain(ty, 0, 239);
  return true;
}

void handleTap(uint16_t x,uint16_t y){
  if(currentScreen==WEB_INFO){currentScreen=SETTINGS;redraw();return;}

  if(currentScreen==WIFI_LIST){
    if(y<42 && x<60){ currentScreen=SETTINGS; redraw(); return; }

    if(wifiCount==0){
      if(y>=140 && y<=195){ scanWifiNetworks(); drawWifiList(); }
      return;
    }

    for(int row=0;row<5;row++){
      int yy=48+row*36;
      if(y>=yy && y<=yy+31){
        int idx=wifiOffset+row;
        if(idx<wifiCount){
          selectedSSID=wifiSSIDs[idx];
          passwordBuffer="";
          keyboardNumbers=false;
          keyboardShift=false;
          showPassword=false;
          currentScreen=WIFI_KEYBOARD;
          drawWifiKeyboard();
        }
        return;
      }
    }

    if(y>=218){
      if(x<115 && wifiOffset>0){ wifiOffset=max(0,wifiOffset-5); drawWifiList(); return; }
      if(x>=115 && x<210){ scanWifiNetworks(); drawWifiList(); return; }
      if(x>=210 && wifiOffset+5<wifiCount){ wifiOffset+=5; drawWifiList(); return; }
    }
    return;
  }

  if(currentScreen==WIFI_KEYBOARD){
    if(y<44 && x<60){ currentScreen=WIFI_LIST; drawWifiList(); return; }
    if(x>=250 && y>=44 && y<=74){ showPassword=!showPassword; drawWifiKeyboard(); return; }

    const char* rowsAlpha[]={"qwertyuiop","asdfghjkl","zxcvbnm"};
    const char* rowsNum[]={"1234567890","!@#$%^&*(/",")-_+=.,?"};
    for(int r=0;r<3;r++){
      const char* row=keyboardNumbers?rowsNum[r]:rowsAlpha[r];
      int len=strlen(row);
      for(int i=0;i<len;i++){
        int bx=i*29+2, by=78+r*30;
        if(x>=bx && x<=bx+26 && y>=by && y<=by+26){
          char ch=row[i];
          if(keyboardShift && !keyboardNumbers) ch=toupper(ch);
          passwordBuffer+=ch;
          drawWifiKeyboard();
          return;
        }
      }
    }

    if(y>=168 && y<=193){ passwordBuffer+=" "; drawWifiKeyboard(); return; }

    if(y>=198){
      if(x<64){ keyboardShift=!keyboardShift; drawWifiKeyboard(); return; }
      if(x<128){ keyboardNumbers=!keyboardNumbers; drawWifiKeyboard(); return; }
      if(x<192){ if(passwordBuffer.length()) passwordBuffer.remove(passwordBuffer.length()-1); drawWifiKeyboard(); return; }
      if(x<256){ currentScreen=WIFI_LIST; drawWifiList(); return; }
      connectSelectedWifi(); return;
    }
    return;
  }

  if(currentScreen==HOME){
    if(y<48 && x>280){currentScreen=SETTINGS;redraw();return;}
    currentScreen=APPS;redraw();return;
  }
  if(currentScreen==APPS){
    if(y<42 && x<60){currentScreen=HOME;redraw();return;}
    if(y>=43 && y<216){int index=(y>=134?2:0)+(x>=165?1:0);const Screen pages[]={WEATHER,TIMER,MAKERWORLD,SETTINGS};currentScreen=pages[index];redraw();}
    return;
  }

  if(currentScreen==MAKERWORLD){if(y<42 && x<60){currentScreen=HOME;redraw();}return;}

  if(currentScreen==WEATHER){
    if(y<45 && x<60){ currentScreen=HOME; redraw(); }
    else if(y>=207 && y<230){weatherDaily=!weatherDaily;redraw();}
    return;
  }

  if(currentScreen==SETTINGS){
    if(y<42 && x<60){currentScreen=HOME;redraw();return;}
    if(y>=43 && y<=109){
      if(x<160){scanWifiNetworks();currentScreen=WIFI_LIST;drawWifiList();}
      else{cfgBrightness=cfgBrightness<80?120:(cfgBrightness<160?200:(cfgBrightness<240?255:40));analogWrite(TFT_BL,cfgBrightness);savePrefs();redraw();}
    }else if(y>=118 && y<=184){
      if(x<160){currentScreen=MAKERWORLD;redraw();}
      else{currentScreen=WEB_INFO;redraw();}
    }else redraw();
    return;
  }

  if(currentScreen==TIMER){
    if(y<45 && x<60){ currentScreen=HOME; redraw(); return; }
    if(y<45 && x>=260){stopwatchMode=!stopwatchMode;redraw();return;}
    if(stopwatchMode){
      if(y>=161 && y<=195){
        if(x>=199 && x<=266)stopwatch.reset(millis());
        else if(x>=54 && x<=189){if(stopwatch.running())stopwatch.pause(millis());else stopwatch.start(millis());}
        redraw();
      }
      return;
    }

    if(y>=161 && y<=195){
      if(x>=199 && x<=266){timerRunning=false;timerFinished=false;timerDurationSec=timerPresetSec;}
      else if(x>=54 && x<=189){
        if(timerRunning){timerDurationSec=timerLeft();timerRunning=false;}
        else {if(!timerDurationSec)timerDurationSec=timerPresetSec;timerFinished=false;timerEndsAt=millis()+timerDurationSec*1000UL;timerRunning=true;}
      }
      redraw();return;
    }
    if(y>=202 && y<=227){
      if(x>=32 && x<108)setTimerMinutes(25);
      else if(x>=122 && x<198)setTimerMinutes(5);
      else if(x>=212 && x<288)setTimerMinutes(15);
    }
  }
}

// Tap only on release. A swipe cannot start timers or open widgets on press.
void handleTouch(){
  static bool pressed=false;static uint16_t startX=0,startY=0,endX=0,endY=0;
  static uint32_t started=0,releasedAt=0;static Screen origin=HOME;
  uint16_t x,y;bool down=readTouch(x,y);
  if(down){
    releasedAt=0;
    if(!pressed){pressed=true;startX=x;startY=y;started=millis();origin=currentScreen;}
    endX=x;endY=y;return;
  }
  if(!pressed)return;
  // Resistive touch can briefly disappear during motion.
  if(!releasedAt){releasedAt=millis();return;}
  if(millis()-releasedAt<35)return;
  pressed=false;releasedAt=0;
  if(origin!=currentScreen || millis()-lastTouch<120)return;
  int dx=(int)endX-startX,dy=(int)endY-startY;lastTouch=millis();
  if(origin<=SETTINGS && abs(dx)>=45 && abs(dx)>abs(dy)*2){
    int index=(int)origin+(dx<0?1:-1);currentScreen=(Screen)((index+6)%6);redraw();return;
  }
  if(abs(dx)>14 || abs(dy)>14 || millis()-started>1500)return;
  if(origin<=SETTINGS && endY>=228 && endX>=128 && endX<=190){
    int index=constrain(((int)endX-130)/10,0,5);currentScreen=(Screen)index;redraw();return;
  }
  handleTap(startX,startY);
}

void setup(){
  Serial.begin(115200);loadPrefs();lcd.init();lcd.setRotation(1);delay(50);lcd.invertDisplay(true);
  surface.begin();
  pinMode(TFT_BL,OUTPUT);analogWrite(TFT_BL,cfgBrightness);
  SPI.begin(T_CLK,T_DOUT,T_DIN);ts.begin();ts.setRotation(1);
  lcd.fillScreen(C_BG);textAt("Universal Desk Dashboard",32,102);textAt("Starting...",126,130,1,C_MUTED);
  connectWiFi();
  weatherQueue=xQueueCreate(1,sizeof(WeatherData));makerQueue=xQueueCreate(1,sizeof(MakerData));
  if(weatherQueue && makerQueue)xTaskCreatePinnedToCore(networkTask,"dashboard-network",12288,nullptr,1,&networkTaskHandle,0);
  if(!setupMode && WiFi.status()==WL_CONNECTED){configTzTime(cfgTZ.c_str(),"pool.ntp.org","time.nist.gov");redraw();}
  else{scanWifiNetworks();currentScreen=WIFI_LIST;drawWifiList();}
}
void loop(){
  stopwatch.tick(millis());
  if(setupMode)dns.processNextRequest();server.handleClient();handleTouch();
  bool changed=false;WeatherData w;MakerData m;
  if(weatherQueue && xQueueReceive(weatherQueue,&w,0)==pdTRUE){weather=w;changed=true;}
  if(makerQueue && xQueueReceive(makerQueue,&m,0)==pdTRUE){maker=m;changed=true;}
  if(timerRunning && !timerLeft()){timerRunning=false;timerFinished=true;timerDurationSec=0;changed=true;}
  struct tm t;
  if(currentScreen==HOME && getLocalTime(&t,5) && t.tm_min!=lastMinuteDrawn){lastMinuteDrawn=t.tm_min;changed=true;}
  if(currentScreen==TIMER && ((stopwatchMode && stopwatch.running() && millis()-lastDraw>=100) || (!stopwatchMode && timerRunning && millis()-lastDraw>=1000)))changed=true;
  if(changed && currentScreen<=SETTINGS)redraw();
  delay(10);
}
