/* Universal Desk Dashboard v0.1 - CYD ESP32-2432S028 */
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
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

enum Screen { HOME, WEATHER, TIMER, SETTINGS, WIFI_LIST, WIFI_KEYBOARD };
Screen currentScreen = HOME;

String cfgSsid, cfgPass;
String cfgCity = "Brno";
String cfgTZ = "CET-1CEST,M3.5.0,M10.5.0/3";
String cfgCustomLink = "https://example.com";
float cfgLat = 49.1951;
float cfgLon = 16.6068;

bool setupMode = false;
const char* AP_NAME = "DeskDash-Setup";
IPAddress apIP(192,168,4,1);

struct WeatherData {
  bool valid = false;
  float temp = 0, feels = 0, wind = 0, tmax = 0, tmin = 0;
  int humidity = 0, code = -1;
  unsigned long updatedAt = 0;
} weather;

const unsigned long WEATHER_INTERVAL = 15UL * 60UL * 1000UL;
bool timerRunning = false;
unsigned long timerEndsAt = 0;
uint32_t timerDurationSec = 25 * 60;
unsigned long lastDraw = 0, lastTouch = 0;
int lastMinuteDrawn = -1;

static const uint16_t C_BG=0x0000, C_PANEL=0x1082, C_PANEL2=0x18E3;
static const uint16_t C_TEXT=0xFFFF, C_MUTED=0x9CF3, C_CYAN=0x04FF;
static const uint16_t C_GREEN=0x366B, C_YELLOW=0xFD20;
static const uint16_t C_BLUE=0x04FF, C_RED=0xF986;

void drawTimer();

void loadPrefs(){
  prefs.begin("desk-dash", true);
  cfgSsid=prefs.getString("ssid",""); cfgPass=prefs.getString("pass","");
  cfgCity=prefs.getString("city","Brno"); cfgTZ=prefs.getString("tz","CET-1CEST,M3.5.0,M10.5.0/3");
  cfgCustomLink=prefs.getString("link","https://example.com");
  cfgLat=prefs.getFloat("lat",49.1951f); cfgLon=prefs.getFloat("lon",16.6068f);
  prefs.end();
}

void savePrefs(){
  prefs.begin("desk-dash",false);
  prefs.putString("ssid",cfgSsid); prefs.putString("pass",cfgPass); prefs.putString("city",cfgCity);
  prefs.putString("tz",cfgTZ); prefs.putString("link",cfgCustomLink);
  prefs.putFloat("lat",cfgLat); prefs.putFloat("lon",cfgLon); prefs.end();
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
  h+="<style>body{font-family:system-ui;background:#0b1017;color:#f5f7fa;padding:24px}.w{max-width:620px;margin:auto}.c{background:#121b25;border:1px solid #233445;border-radius:18px;padding:20px;margin:0 0 16px}input{width:100%;box-sizing:border-box;padding:12px;border-radius:10px;border:1px solid #31465a;background:#0d151e;color:white}label{display:block;margin:12px 0 5px}button{width:100%;padding:13px;border:0;border-radius:11px;background:#11b8f3;font-weight:700;margin-top:16px}</style></head><body><div class='w'>";
  h+="<div class='c'><h1>Universal Desk Dashboard</h1><p>Device IP: "+ip+"</p></div><form method='POST' action='/save'>";
  h+="<div class='c'><h2>Wi-Fi</h2><label>SSID</label><input name='ssid' value='"+esc(cfgSsid)+"'><label>Password</label><input type='password' name='pass' value='"+esc(cfgPass)+"'></div>";
  h+="<div class='c'><h2>Location & time</h2><label>City</label><input name='city' value='"+esc(cfgCity)+"'><label>Latitude</label><input name='lat' value='"+String(cfgLat,5)+"'><label>Longitude</label><input name='lon' value='"+String(cfgLon,5)+"'><label>POSIX timezone</label><input name='tz' value='"+esc(cfgTZ)+"'></div>";
  h+="<div class='c'><h2>Custom</h2><label>Custom link</label><input name='link' value='"+esc(cfgCustomLink)+"'><button type='submit'>Save & restart</button></div></form></div></body></html>";
  return h;
}

void startWebServer(){
  server.on("/",HTTP_GET,[]{server.send(200,"text/html; charset=utf-8",setupPage());});
  server.on("/save",HTTP_POST,[]{
    if(server.hasArg("ssid"))cfgSsid=server.arg("ssid"); if(server.hasArg("pass"))cfgPass=server.arg("pass");
    if(server.hasArg("city"))cfgCity=server.arg("city"); if(server.hasArg("lat"))cfgLat=server.arg("lat").toFloat();
    if(server.hasArg("lon"))cfgLon=server.arg("lon").toFloat(); if(server.hasArg("tz"))cfgTZ=server.arg("tz");
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

String weatherText(int c){if(c==0)return"Clear";if(c<=3)return"Partly cloudy";if(c==45||c==48)return"Fog";if((c>=51&&c<=67)||(c>=80&&c<=82))return"Rain";if(c>=71&&c<=77)return"Snow";if(c>=95)return"Thunderstorm";return"Weather";}

void fetchWeather(){
  if(WiFi.status()!=WL_CONNECTED)return;
  String url="https://api.open-meteo.com/v1/forecast?latitude="+String(cfgLat,5)+"&longitude="+String(cfgLon,5)+"&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1";
  WiFiClientSecure client; client.setInsecure(); HTTPClient http; if(!http.begin(client,url))return; int st=http.GET();
  if(st==200){JsonDocument doc; if(!deserializeJson(doc,http.getString())){weather.temp=doc["current"]["temperature_2m"]|0.0;weather.humidity=doc["current"]["relative_humidity_2m"]|0;weather.feels=doc["current"]["apparent_temperature"]|weather.temp;weather.code=doc["current"]["weather_code"]|-1;weather.wind=doc["current"]["wind_speed_10m"]|0.0;weather.tmax=doc["daily"]["temperature_2m_max"][0]|0.0;weather.tmin=doc["daily"]["temperature_2m_min"][0]|0.0;weather.valid=true;weather.updatedAt=millis();}}
  http.end();
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

void drawWifiGlyph(int x,int y,uint16_t color){
  lcd.drawCircle(x,y,11,color);
  lcd.fillRect(x-13,y-12,26,13,C_BG);
  lcd.drawCircle(x,y,7,color);
  lcd.fillRect(x-9,y-8,18,9,C_BG);
  lcd.fillCircle(x,y+5,2,color);
}

void drawGearGlyph(int x,int y,uint16_t color){
  lcd.drawCircle(x,y,8,color);
  lcd.drawCircle(x,y,3,color);
  for(int i=-1;i<=1;i+=2){
    lcd.drawFastHLine(x-12,y+i*6,24,color);
    lcd.drawFastVLine(x+i*6,y-12,24,color);
  }
}

void drawHome(){
  lcd.fillScreen(C_BG);

  struct tm t;
  char tb[6]="--:--";
  char db[28]="Waiting for time";
  if(getLocalTime(&t,50)){
    strftime(tb,sizeof(tb),"%H:%M",&t);
    strftime(db,sizeof(db),"%A, %d %B",&t);
  }

  // Apple-like top area: time is the hero, setup stays subtle.
  lcd.setTextDatum(TL_DATUM);
  lcd.setTextFont(7);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(tb,12,8);

  lcd.setTextFont(2);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(db,14,57);

  if(WiFi.status()==WL_CONNECTED) drawWifiGlyph(275,24,C_TEXT);
  drawGearGlyph(305,24,C_MUTED);

  // Weather widget.
  card(8,82,148,62,C_PANEL);
  lcd.setTextDatum(TL_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString("WEATHER",20,93);
  lcd.setTextFont(4);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(weather.valid?String(weather.temp,0)+" C":"-- C",20,108);
  lcd.setTextDatum(TR_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  String wt=weather.valid?weatherText(weather.code):"Loading";
  if(wt.length()>15) wt=wt.substring(0,15);
  lcd.drawString(wt,145,121);

  // Focus widget.
  card(164,82,148,62,C_PANEL);
  lcd.setTextDatum(TL_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString("FOCUS",176,93);
  uint32_t r=timerDurationSec;
  if(timerRunning){
    long ms=(long)(timerEndsAt-millis());
    r=ms>0?ms/1000:0;
  }
  char b[10];
  snprintf(b,sizeof(b),"%02lu:%02lu",(unsigned long)(r/60),(unsigned long)(r%60));
  lcd.setTextFont(4);
  lcd.setTextColor(timerRunning?C_GREEN:C_TEXT);
  lcd.drawString(b,176,108);
  lcd.setTextDatum(TR_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(timerRunning?"Running":"Ready",300,121);

  // Today widget.
  card(8,152,148,78,C_PANEL);
  lcd.setTextDatum(TL_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString("TODAY",20,163);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(weather.valid?("H "+String(weather.tmax,0)+"  L "+String(weather.tmin,0)):"H --  L --",20,181);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(weather.valid?("Humidity "+String(weather.humidity)+"%"):"Weather pending",20,210);

  // Connection widget.
  card(164,152,148,78,WiFi.status()==WL_CONNECTED?0x1125:C_PANEL);
  lcd.setTextDatum(TL_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(WiFi.status()==WL_CONNECTED?C_GREEN:C_MUTED);
  lcd.drawString(WiFi.status()==WL_CONNECTED?"CONNECTED":"OFFLINE",176,163);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  String ss=cfgSsid.length()?cfgSsid:"Wi-Fi";
  if(ss.length()>16) ss=ss.substring(0,16);
  lcd.drawString(ss,176,181);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():"Tap to setup",176,210);
}

void drawWeather(){
  lcd.fillScreen(C_BG);
  appleHeader("Weather",true);

  lcd.setTextDatum(TL_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(cfgCity,20,49);

  lcd.setTextFont(7);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(weather.valid?String(weather.temp,0)+" C":"-- C",18,64);

  lcd.setTextFont(2);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(weather.valid?weatherText(weather.code):"Loading...",20,111);

  card(12,140,92,84,C_PANEL);
  card(114,140,92,84,C_PANEL);
  card(216,140,92,84,C_PANEL);

  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString("HUMIDITY",58,154);
  lcd.drawString("WIND",160,154);
  lcd.drawString("HIGH / LOW",262,154);

  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(weather.valid?String(weather.humidity)+"%":"--",58,182);
  lcd.drawString(weather.valid?String(weather.wind,0)+" km/h":"--",160,182);
  lcd.drawString(weather.valid?String(weather.tmax,0)+" / "+String(weather.tmin,0):"-- / --",262,182);

  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(weather.valid?("Feels "+String(weather.feels,0)+" C"):"",58,207);
}

void drawTimer(){
  lcd.fillScreen(C_BG);
  appleHeader("Focus",true);

  uint32_t r=timerDurationSec;
  if(timerRunning){
    long ms=(long)(timerEndsAt-millis());
    if(ms<=0){timerRunning=false;r=0;}
    else r=ms/1000;
  }
  char b[10];
  snprintf(b,sizeof(b),"%02lu:%02lu",(unsigned long)(r/60),(unsigned long)(r%60));

  // Simple Apple Watch-like focus ring.
  for(int rr=61;rr<=65;rr++) lcd.drawCircle(160,104,rr,0x2124);
  if(timerRunning){
    for(int rr=61;rr<=65;rr++){
      lcd.drawArc(160,104,rr,rr-1,200,340,C_BLUE,C_BG,true);
      lcd.drawArc(160,104,rr,rr-1,20,160,C_CYAN,C_BG,true);
    }
  }

  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(7);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(b,160,92);
  lcd.setTextFont(2);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(timerRunning?"Running":"Ready",160,124);

  lcd.fillRoundRect(72,163,176,37,18,timerRunning?0x2945:C_GREEN);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(timerRunning?"PAUSE":"START",160,181);

  const char* p[]={"25","5","15"};
  int xx[]={70,160,250};
  for(int i=0;i<3;i++){
    lcd.fillRoundRect(xx[i]-31,207,62,27,13,C_PANEL);
    lcd.setTextFont(1);
    lcd.setTextColor(C_TEXT);
    lcd.drawString(String(p[i])+" min",xx[i],220);
  }
}

void drawSettings(){
  lcd.fillScreen(C_BG);
  appleHeader("Setup",true);

  card(10,50,300,49,C_PANEL);
  card(10,106,300,49,C_PANEL);
  card(10,162,300,49,C_PANEL);

  lcd.setTextDatum(ML_DATUM);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString("Wi-Fi",24,66);
  lcd.drawString("Weather",24,122);
  lcd.drawString("Device",24,178);

  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  String net=WiFi.status()==WL_CONNECTED?(cfgSsid+"  "+WiFi.localIP().toString()):"Choose network";
  if(net.length()>34) net=net.substring(0,34);
  lcd.drawString(net,24,86);
  lcd.drawString(cfgCity+"  "+String(cfgLat,2)+", "+String(cfgLon,2),24,142);
  lcd.drawString("CYD 2.8  |  ST7789  |  Touch",24,198);

  lcd.setTextDatum(MR_DATUM);
  lcd.setTextFont(2);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(">",294,74);
  lcd.drawString(">",294,130);
}


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
      lcd.fillRoundRect(bx,by,26,26,6,0x2945);
      char ch=row[i];
      if(keyboardShift && !keyboardNumbers) ch=toupper(ch);
      lcd.setTextColor(C_TEXT);
      lcd.drawString(String(ch),bx+13,by+13);
    }
  }

  lcd.fillRoundRect(44,168,232,25,8,0x2945);
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
    fetchWeather();
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

void redraw(){if(currentScreen==HOME)drawHome();else if(currentScreen==WEATHER)drawWeather();else if(currentScreen==TIMER)drawTimer();else if(currentScreen==SETTINGS)drawSettings();else if(currentScreen==WIFI_LIST)drawWifiList();else drawWifiKeyboard();lastDraw=millis();}
void setTimerMinutes(int m){timerDurationSec=m*60;timerRunning=false;drawTimer();}

bool readTouch(uint16_t &x, uint16_t &y){
  if(!ts.touched()) return false;
  TS_Point p = ts.getPoint();
  int tx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, 320);
  int ty = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 240);
  x = constrain(tx, 0, 319);
  y = constrain(ty, 0, 239);
  return true;
}

void handleTouch(){
  uint16_t x,y;
  if(!readTouch(x,y)) return;
  if(millis()-lastTouch<220) return;
  lastTouch=millis();

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
    if(y<48 && x>280){ currentScreen=SETTINGS; redraw(); return; }
    if(y>=82 && y<146){
      if(x<160){ currentScreen=WEATHER; redraw(); }
      else { currentScreen=TIMER; redraw(); }
      return;
    }
    if(y>=150 && x>=160){ currentScreen=SETTINGS; redraw(); return; }
    return;
  }

  if(currentScreen==WEATHER){
    if(y<45 && x<60){ currentScreen=HOME; redraw(); }
    return;
  }

  if(currentScreen==SETTINGS){
    if(y<45 && x<60){ currentScreen=HOME; redraw(); return; }
    if(y>=48 && y<=101){
      scanWifiNetworks();
      currentScreen=WIFI_LIST;
      drawWifiList();
      return;
    }
    return;
  }

  if(currentScreen==TIMER){
    if(y<45 && x<60){ currentScreen=HOME; redraw(); return; }

    if(y>=158 && y<=202){
      if(timerRunning){
        long ms=(long)(timerEndsAt-millis());
        timerDurationSec=ms>0?ms/1000:0;
        timerRunning=false;
      }else{
        timerEndsAt=millis()+(unsigned long)timerDurationSec*1000UL;
        timerRunning=true;
      }
      redraw();
      return;
    }

    if(y>=205){
      if(x<115)setTimerMinutes(25);
      else if(x<205)setTimerMinutes(5);
      else setTimerMinutes(15);
    }
  }
}

void setup(){Serial.begin(115200);loadPrefs();lcd.init();lcd.setRotation(1);delay(50);lcd.invertDisplay(true);delay(50);pinMode(TFT_BL,OUTPUT);analogWrite(TFT_BL,180);SPI.begin(T_CLK,T_DOUT,T_DIN);ts.begin();ts.setRotation(1);lcd.fillScreen(C_BG);lcd.setTextDatum(MC_DATUM);lcd.setTextColor(C_TEXT);lcd.setTextFont(2);lcd.drawString("Universal Desk Dashboard",160,102);lcd.setTextFont(1);lcd.setTextColor(C_MUTED);lcd.drawString("Starting...",160,130);connectWiFi();if(!setupMode&&WiFi.status()==WL_CONNECTED){configTzTime(cfgTZ.c_str(),"pool.ntp.org","time.nist.gov");fetchWeather();redraw();}else{scanWifiNetworks();currentScreen=WIFI_LIST;drawWifiList();}}
void loop(){if(setupMode)dns.processNextRequest();server.handleClient();handleTouch();if(!setupMode&&WiFi.status()==WL_CONNECTED){if(!weather.valid||millis()-weather.updatedAt>WEATHER_INTERVAL)fetchWeather();}struct tm t;if(currentScreen==HOME&&getLocalTime(&t,5)&&t.tm_min!=lastMinuteDrawn){lastMinuteDrawn=t.tm_min;drawHome();}if(currentScreen==TIMER&&timerRunning&&millis()-lastDraw>1000)drawTimer();delay(10);}
