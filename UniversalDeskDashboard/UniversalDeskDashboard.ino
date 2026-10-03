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

static const uint16_t C_BG=0x0821, C_PANEL=0x10A3, C_PANEL2=0x18E4;
static const uint16_t C_TEXT=0xFFFF, C_MUTED=0x8410, C_CYAN=0x3DDF;
static const uint16_t C_GREEN=0x47E7, C_YELLOW=0xFD20;
static const uint16_t C_BLUE=0x24DF, C_RED=0xF986;

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
  lcd.fillRoundRect(x,y,w,h,14,color);
}

void smallDot(int x,int y,uint16_t color){
  lcd.fillCircle(x,y,3,color);
}

void titleBar(const String &title){
  lcd.fillRect(0,0,320,30,C_BG);
  lcd.setTextDatum(ML_DATUM);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(title,10,15);

  lcd.setTextDatum(MR_DATUM);
  lcd.setTextFont(1);
  bool online=WiFi.status()==WL_CONNECTED;
  lcd.setTextColor(online?C_GREEN:C_MUTED);
  lcd.drawString(online?"ONLINE":"OFFLINE",306,15);
  smallDot(313,15,online?C_GREEN:C_MUTED);
}

void nav(){
  lcd.fillRoundRect(7,201,306,35,13,0x1062);
  const char* labels[]={"Home","Weather","Timer","Setup"};
  int xs[]={44,121,199,276};
  for(int i=0;i<4;i++){
    bool active=((int)currentScreen==i);
    if(active) lcd.fillRoundRect(xs[i]-31,205,62,27,10,C_PANEL2);
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextFont(1);
    lcd.setTextColor(active?C_CYAN:C_MUTED);
    lcd.drawString(labels[i],xs[i],218);
    if(active) lcd.fillCircle(xs[i],230,2,C_CYAN);
  }
}

void drawHome(){
  lcd.fillScreen(C_BG);

  struct tm t;
  char tb[6]="--:--";
  char db[24]="Waiting for time";
  if(getLocalTime(&t,50)){
    strftime(tb,sizeof(tb),"%H:%M",&t);
    strftime(db,sizeof(db),"%a, %d %b",&t);
  }

  // Slim top status line.
  lcd.setTextDatum(ML_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(cfgCity,10,14);
  bool online=WiFi.status()==WL_CONNECTED;
  lcd.setTextDatum(MR_DATUM);
  lcd.setTextColor(online?C_GREEN:C_MUTED);
  lcd.drawString(online?"Wi-Fi connected":"Offline",304,14);
  smallDot(312,14,online?C_GREEN:C_MUTED);

  // Time tile.
  card(8,30,148,83,C_PANEL);
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(7);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(tb,82,65);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(db,82,102);

  // Weather tile.
  card(164,30,148,83,C_PANEL);
  lcd.setTextDatum(TL_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_CYAN);
  lcd.drawString("WEATHER",176,41);
  lcd.setTextFont(4);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(weather.valid?String(weather.temp,0)+" C":"-- C",176,58);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  String wt=weather.valid?weatherText(weather.code):"Loading...";
  if(wt.length()>18) wt=wt.substring(0,18);
  lcd.drawString(wt,176,88);
  lcd.drawString(cfgCity,176,101);

  // Focus tile.
  card(8,121,148,72,C_PANEL2);
  lcd.setTextDatum(TL_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_CYAN);
  lcd.drawString("FOCUS",20,132);
  uint32_t r=timerDurationSec;
  if(timerRunning){
    long ms=(long)(timerEndsAt-millis());
    r=ms>0?ms/1000:0;
  }
  char b[10];
  snprintf(b,sizeof(b),"%02lu:%02lu",(unsigned long)(r/60),(unsigned long)(r%60));
  lcd.setTextFont(4);
  lcd.setTextColor(timerRunning?C_GREEN:C_TEXT);
  lcd.drawString(b,20,151);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(timerRunning?"Running":"Tap Timer to start",20,180);

  // Status / today tile.
  card(164,121,148,72,C_PANEL);
  lcd.setTextFont(1);
  lcd.setTextColor(C_CYAN);
  lcd.drawString("TODAY",176,132);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  String range=weather.valid?String(weather.tmax,0)+" / "+String(weather.tmin,0)+" C":"-- / -- C";
  lcd.drawString(range,176,151);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(weather.valid?("Humidity "+String(weather.humidity)+"%"):"Weather pending",176,177);

  nav();
}

void drawWeather(){
  lcd.fillScreen(C_BG);
  titleBar("Weather");

  card(8,34,304,78,C_PANEL2);
  lcd.setTextDatum(TL_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_CYAN);
  lcd.drawString(cfgCity,20,45);
  lcd.setTextFont(7);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(weather.valid?String(weather.temp,1)+" C":"--.- C",20,60);
  lcd.setTextDatum(TR_DATUM);
  lcd.setTextFont(2);
  lcd.setTextColor(C_MUTED);
  String wt=weather.valid?weatherText(weather.code):"Loading...";
  lcd.drawString(wt,298,55);
  lcd.setTextFont(1);
  lcd.drawString(weather.valid?("Feels "+String(weather.feels,0)+" C"):"",298,82);

  card(8,120,96,72,C_PANEL);
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString("HUMIDITY",56,134);
  lcd.setTextFont(4);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(weather.valid?String(weather.humidity)+"%":"--",56,160);

  card(112,120,96,72,C_PANEL);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString("WIND",160,134);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(weather.valid?String(weather.wind,0)+" km/h":"--",160,162);

  card(216,120,96,72,C_PANEL);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString("HIGH / LOW",264,134);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(weather.valid?String(weather.tmax,0)+" / "+String(weather.tmin,0):"-- / --",264,162);

  nav();
}

void drawTimer(){
  lcd.fillScreen(C_BG);
  titleBar("Focus");

  uint32_t r=timerDurationSec;
  if(timerRunning){
    long ms=(long)(timerEndsAt-millis());
    if(ms<=0){timerRunning=false;r=0;}
    else r=ms/1000;
  }
  char b[10];
  snprintf(b,sizeof(b),"%02lu:%02lu",(unsigned long)(r/60),(unsigned long)(r%60));

  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(timerRunning?"FOCUS SESSION":"READY TO FOCUS",160,44);
  lcd.setTextFont(7);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(b,160,81);

  // Keep the button inside the existing touch hitbox y=120..170.
  lcd.fillRoundRect(35,122,250,44,14,timerRunning?0x31A6:C_BLUE);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(timerRunning?"PAUSE":"START",160,144);

  const char* presets[]={"25 min","5 min","15 min"};
  int centers[]={65,160,255};
  for(int i=0;i<3;i++){
    lcd.fillRoundRect(centers[i]-38,173,76,24,9,C_PANEL);
    lcd.setTextFont(1);
    lcd.setTextColor(C_MUTED);
    lcd.drawString(presets[i],centers[i],185);
  }

  nav();
}

void drawSettings(){
  lcd.fillScreen(C_BG);
  titleBar("Setup");

  card(8,36,304,64,C_PANEL2);
  lcd.setTextDatum(TL_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString("NETWORK",20,48);
  lcd.setTextFont(2);
  lcd.setTextColor(WiFi.status()==WL_CONNECTED?C_GREEN:C_YELLOW);
  lcd.drawString(WiFi.status()==WL_CONNECTED?"Connected":"Not connected",20,66);
  lcd.setTextDatum(TR_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():"Tap Setup below",300,72);

  card(8,108,148,84,C_PANEL);
  lcd.setTextDatum(TL_DATUM);
  lcd.setTextFont(1);
  lcd.setTextColor(C_CYAN);
  lcd.drawString("LOCATION",20,120);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(cfgCity,20,140);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString(String(cfgLat,2)+", "+String(cfgLon,2),20,168);

  card(164,108,148,84,C_PANEL);
  lcd.setTextFont(1);
  lcd.setTextColor(C_CYAN);
  lcd.drawString("DEVICE",176,120);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString("CYD 2USB",176,140);
  lcd.setTextFont(1);
  lcd.setTextColor(C_MUTED);
  lcd.drawString("ST7789 / Touch",176,168);

  nav();
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
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(2);
  lcd.setTextColor(C_TEXT);
  lcd.drawString("WI-FI SELECTION",160,16);

  lcd.setTextDatum(ML_DATUM);
  lcd.setTextFont(2);
  if(wifiCount==0){
    lcd.setTextColor(C_YELLOW);
    lcd.drawString("No networks found",14,55);
  }else{
    int shown=0;
    for(int i=wifiOffset;i<wifiCount && shown<5;i++,shown++){
      int y=39+shown*31;
      String s=wifiSSIDs[i];
      if(s.length()>23) s=s.substring(0,20)+"...";
      lcd.setTextColor(C_TEXT);
      lcd.drawString(s,14,y);
      lcd.drawFastHLine(10,y+23,300,0x2945);
    }
  }

  lcd.setTextDatum(MC_DATUM);
  lcd.setTextFont(1);
  lcd.drawRoundRect(8,204,72,29,7,C_MUTED);
  lcd.setTextColor(C_MUTED); lcd.drawString("BACK",44,219);
  lcd.drawRoundRect(88,204,88,29,7,C_CYAN);
  lcd.setTextColor(C_CYAN); lcd.drawString("REFRESH",132,219);

  if(wifiOffset>0){
    lcd.drawRoundRect(190,204,54,29,7,C_CYAN);
    lcd.drawString("UP",217,219);
  }
  if(wifiOffset+5<wifiCount){
    lcd.drawRoundRect(252,204,60,29,7,C_CYAN);
    lcd.drawString("DOWN",282,219);
  }
}

void drawWifiKeyboard(){
  lcd.fillScreen(C_BG);
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextColor(C_TEXT);
  lcd.setTextFont(2);
  lcd.drawString("Wi-Fi Password",160,13);
  lcd.setTextFont(1);
  String ss=selectedSSID;
  if(ss.length()>28) ss=ss.substring(0,25)+"...";
  lcd.setTextColor(C_CYAN);
  lcd.drawString(ss,160,31);

  lcd.drawRect(10,42,300,28,C_MUTED);
  lcd.setTextDatum(ML_DATUM);
  lcd.setTextColor(C_TEXT);
  String shown=showPassword?passwordBuffer:String("");
  if(!showPassword) for(size_t i=0;i<passwordBuffer.length();i++) shown+="*";
  lcd.drawString(shown,17,53);

  const char* rowsAlpha[]={"qwertyuiop","asdfghjkl","zxcvbnm"};
  const char* rowsNum[]={"1234567890","!@#$%^&*(/",")-_+=.,?"};
  lcd.setTextDatum(MC_DATUM);
  for(int r=0;r<3;r++){
    const char* row=keyboardNumbers?rowsNum[r]:rowsAlpha[r];
    int len=strlen(row);
    for(int i=0;i<len;i++){
      int bx=i*29+2, by=78+r*30;
      lcd.drawRect(bx,by,26,26,C_MUTED);
      char ch=row[i];
      if(keyboardShift && !keyboardNumbers) ch=toupper(ch);
      lcd.setTextColor(C_TEXT);
      lcd.drawString(String(ch),bx+13,by+13);
    }
  }

  lcd.drawRect(2,168,316,25,C_MUTED);
  lcd.drawString("Space",160,181);

  int bw=64, by=198, bh=35;
  lcd.drawRect(2,by,bw-4,bh,C_MUTED); lcd.drawString("Shift",32,216);
  lcd.drawRect(bw+2,by,bw-4,bh,C_MUTED); lcd.drawString("123",96,216);
  lcd.drawRect(2*bw+2,by,bw-4,bh,C_MUTED); lcd.drawString("Del",160,216);
  lcd.drawRect(3*bw+2,by,bw-4,bh,C_YELLOW); lcd.setTextColor(C_YELLOW); lcd.drawString("Back",224,216);
  lcd.drawRect(4*bw+2,by,bw-4,bh,C_GREEN); lcd.setTextColor(C_GREEN); lcd.drawString("OK",288,216);

  lcd.setTextColor(C_CYAN);
  lcd.drawRect(250,44,56,22,C_CYAN);
  lcd.drawString(showPassword?"Hide":"Show",278,55);
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
    if(y>=204){
      if(x<82){ currentScreen=HOME; redraw(); return; }
      if(x>=88 && x<=180){ scanWifiNetworks(); drawWifiList(); return; }
      if(x>=188 && x<=248 && wifiOffset>0){ wifiOffset=max(0,wifiOffset-5); drawWifiList(); return; }
      if(x>=250 && wifiOffset+5<wifiCount){ wifiOffset+=5; drawWifiList(); return; }
    }
    for(int row=0;row<5;row++){
      int yy=36+row*31;
      if(y>=yy && y<yy+29){
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
    return;
  }

  if(currentScreen==WIFI_KEYBOARD){
    if(x>=250 && x<=310 && y>=42 && y<=70){
      showPassword=!showPassword; drawWifiKeyboard(); return;
    }

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

  if(y>=200){
    if(x<80){currentScreen=HOME;redraw();}
    else if(x<160){currentScreen=WEATHER;redraw();}
    else if(x<240){currentScreen=TIMER;redraw();}
    else{
      scanWifiNetworks();
      currentScreen=WIFI_LIST;
      drawWifiList();
    }
    return;
  }

  if(currentScreen==TIMER){
    if(y>=120&&y<=170){
      if(timerRunning){
        long ms=(long)(timerEndsAt-millis());
        timerDurationSec=ms>0?ms/1000:0;
        timerRunning=false;
      }else{
        timerEndsAt=millis()+(unsigned long)timerDurationSec*1000UL;
        timerRunning=true;
      }
      redraw();
    }else if(y>=170&&y<202){
      if(x<110)setTimerMinutes(25);else if(x<210)setTimerMinutes(5);else setTimerMinutes(15);
    }
  }
}

void setup(){Serial.begin(115200);loadPrefs();lcd.init();lcd.setRotation(1);delay(50);lcd.invertDisplay(true);delay(50);pinMode(TFT_BL,OUTPUT);analogWrite(TFT_BL,180);SPI.begin(T_CLK,T_DOUT,T_DIN);ts.begin();ts.setRotation(1);lcd.fillScreen(C_BG);lcd.setTextDatum(MC_DATUM);lcd.setTextColor(C_TEXT);lcd.setTextFont(2);lcd.drawString("Universal Desk Dashboard",160,102);lcd.setTextFont(1);lcd.setTextColor(C_MUTED);lcd.drawString("Starting...",160,130);connectWiFi();if(!setupMode&&WiFi.status()==WL_CONNECTED){configTzTime(cfgTZ.c_str(),"pool.ntp.org","time.nist.gov");fetchWeather();redraw();}else{scanWifiNetworks();currentScreen=WIFI_LIST;drawWifiList();}}
void loop(){if(setupMode)dns.processNextRequest();server.handleClient();handleTouch();if(!setupMode&&WiFi.status()==WL_CONNECTED){if(!weather.valid||millis()-weather.updatedAt>WEATHER_INTERVAL)fetchWeather();}struct tm t;if(currentScreen==HOME&&getLocalTime(&t,5)&&t.tm_min!=lastMinuteDrawn){lastMinuteDrawn=t.tm_min;drawHome();}if(currentScreen==TIMER&&timerRunning&&millis()-lastDraw>1000)drawTimer();delay(10);}
