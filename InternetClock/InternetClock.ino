#include <TFT_eSPI.h>
#include "XBM.h"
#include <WiFi.h>
#include <ArduinoJson.h>

#define UTC_OFFSET 8
#define API_UPDATE_INTERVAL_M 10

//WiFi配置
const char* ssid =     "<WiFi名字>";
const char* password = "<WiFi密码>";
//心知天气
const String location = "<城市名>";
const String apiKey = "<心知天气APIkey>";

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

const char* host = "api.seniverse.com";

char hm[6];   char s[3];
char ohm[6];  char os[3];
char ymd[11]; char oymd[11];
char wday[4];
String ostr = "";
String textNow = "--";
int temperature = 99;
int code = 99;
bool initial = true, flipWeather = false, flipTime = false, flipDate = false;
unsigned long targetTime = 0;

const uint8_t* getIconBitmap() {
  switch (code) {
    case 0: return _0;
    case 1: return _0;
    case 2: return _0;
    case 3: return _0;
    case 4: return _4;
    case 5: return _4;
    case 6: return _4;
    case 7: return _4;
    case 8: return _4;
    default: return _99;
  }
}

void updateScreen() {
  tft.setTextSize(2);tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String str = location;
  str.toUpperCase();
  int w = tft.drawString(str, 10, 13);

  str = "UTC+" + String(UTC_OFFSET);
  int tw = tft.textWidth(str);
  int sprWidth = tw + 9, sprHeight = tft.fontHeight() + 6;
  spr.setColorDepth(16);
  spr.createSprite(sprWidth, sprHeight);
  spr.fillSprite(TFT_BLACK);
  spr.fillRoundRect(0, 0, sprWidth, sprHeight, 4, TFT_GREEN);
  spr.setTextSize(2);spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TFT_BLACK, TFT_GREEN);
  spr.drawString(str, 4, 3);
  spr.pushSprite(w + 20, 10);

  if (flipWeather || initial) {
    str = textNow + " / " + temperature + "C";
    tw = tft.textWidth(ostr);
    tft.fillRect(10, 40, tw, tft.fontHeight(), TFT_BLACK);
    tft.setTextSize(2);tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString(str, 10, 40);
    ostr = str;

    tft.fillRect(200, 2, xbmWidth, xbmHeight, TFT_BLACK);
    tft.drawXBitmap(
      200, 2, getIconBitmap(), xbmWidth, xbmHeight, TFT_ORANGE);
  }
  tft.drawLine(0, 68, tft.width(), 68, 0xBDD7);

  if (flipTime || initial) {
    tw = tft.textWidth(hm);
    tft.fillRect(10, 80, tw, tft.fontHeight(), TFT_BLACK);
    tft.setTextSize(8);tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    w = tft.drawString(hm, 10, 80);

    tw = tft.textWidth(s);
    tft.fillRect(w + 20, 100, tw, tft.fontHeight(), TFT_BLACK);
    tft.setTextSize(4);tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString(s, w + 20, 100);
  }

  if (flipDate || initial) {
    str = String(ymd) + "  " + String(wday);
    tw = tft.textWidth(str);
    tft.fillRect(10, 150, tw, tft.fontHeight(), TFT_BLACK);
    tft.setTextSize(2);tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(str, 10, 150);
  }

  tft.drawLine(0, 180, tft.width(), 180, 0xBDD7);

  tft.setTextSize(2);tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Local IP: " + WiFi.localIP().toString(), 10, 190);

  // reset
  flipWeather = false;
  flipTime = false;
  flipDate = false;
}

void getWeather() {
  NetworkClient client;
  while (!client.connect(host, 80)) delay(5000);

  String url = "/v3/weather/now.json?key=" + apiKey + "&location=" + location + "&language=en&unit=c";
  client.print(
    "GET " + url + " HTTP/1.1\r\n" +
    "Host: " + String(host) + "\r\n" +
    "Connection: close\r\n\r\n");
  
  unsigned long timeout = millis();
  while (!client.available()) {
    if (millis() - timeout > 5000) {
      client.stop();
      return;
    }
  }

  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    client.stop();
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, client);
  if (error) {
    client.stop();
    return;
  }
  textNow = doc["results"][0]["now"]["text"].as<String>();
  temperature = doc["results"][0]["now"]["temperature"].as<int>();
  code = doc["results"][0]["now"]["code"].as<int>();
  client.stop();
}

void setup() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(1500);
  configTime(3600 * UTC_OFFSET, 0, "ntp.aliyun.com");

  tft.begin();
  tft.setRotation(1);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);
  updateScreen();

  initial = false;
}

void loop() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  strftime(hm, 6, "%R", &timeinfo);
  strftime(s, 3, "%S", &timeinfo);
  strftime(ymd, 11, "%F", &timeinfo);
  strftime(wday, 4, "%a", &timeinfo);

  if (strcmp(os, s) != 0) {
    flipTime = true;
    strcpy(os, s);
    if (strcmp(oymd, ymd) != 0) {
      flipDate = true;
      strcpy(oymd, ymd);
    }
  }

  if (targetTime < millis()) {
    getWeather();
    flipWeather = true;
    targetTime = millis() + 60000 * API_UPDATE_INTERVAL_M;   
  }

  updateScreen();
}






















