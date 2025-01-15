#include <TFT_eSPI.h>
#include <WiFiClientSecure.h>
#include "XBM.h"
#include <WiFi.h>
#include <ArduinoJson.h>

#define UTC_OFFSET 8
#define API_UPDATE_INTERVAL_M 10

// WiFi
const char* ssid = "<你的WiFi名称>";
const char* password = "<你的WiFi密码>";
// 心知天气
const String location = "<你的城市名称>";
const String apiKey = "<你的心知APIkey>";
// B站
const String vmid = "<你的B站ID>";

#define WEATHER (1 << 1)
#define TIME (1 << 2)
#define DATE (1 << 3)
#define FOLLOWER (1 << 4)

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

const char* host1 = "api.seniverse.com";
const char* host2 = "api.bilibili.com";

char hm[6];
char s[3];
char ohm[6];
char os[3];
char ymd[11];
char oymd[11];
char wday[4];

String ostr;
String textNow = "n/a";
int temperature = 99;
int code = 99;
String follower = "n/a";

uint8_t updateFlag = 0xFF;

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
    case 9: return _9;
    default: return _99;
  }
}

void updateScreen() {
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);
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
  spr.setTextSize(2);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TFT_BLACK, TFT_GREEN);
  spr.drawString(str, 4, 3);
  spr.pushSprite(w + 20, 10);

  if (WEATHER & updateFlag) {  // b'0001'
    str = textNow + " / " + temperature + "C";
    tw = tft.textWidth(ostr);
    tft.fillRect(10, 40, tw, tft.fontHeight(), TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString(str, 10, 40);
    ostr = str;

    tft.fillRect(200, 2, xbmWidth, xbmHeight, TFT_BLACK);
    tft.drawXBitmap(
      200, 2, getIconBitmap(), xbmWidth, xbmHeight, TFT_ORANGE);
  }
  tft.drawLine(0, 68, tft.width(), 68, 0xBDD7);

  if (TIME & updateFlag) {  // b'0010'
    tw = tft.textWidth(hm);
    tft.fillRect(10, 80, tw, tft.fontHeight(), TFT_BLACK);
    tft.setTextSize(8);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    w = tft.drawString(hm, 10, 80);

    tw = tft.textWidth(s);
    tft.fillRect(w + 20, 100, tw, tft.fontHeight(), TFT_BLACK);
    tft.setTextSize(4);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString(s, w + 20, 100);
  }

  if (DATE & updateFlag) {  // b'0100'
    str = String(ymd) + "  " + String(wday);
    tw = tft.textWidth(str);
    tft.fillRect(10, 150, tw, tft.fontHeight(), TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(str, 10, 150);
  }

  tft.drawLine(0, 180, tft.width(), 180, 0xBDD7);

  if (FOLLOWER & updateFlag) {  // b'1000'
    tft.fillRect(0, 180, 320, 60, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String("Follower: ") + follower, 10, 190);
  }

  updateFlag = 0x00;  // 重置显示
}

void sendRequest(WiFiClient* client, const char* host, String url) {
  while (!client->connect(host, 443)) delay(5000);
  client->print(
    "GET " + url + " HTTP/1.1\r\n" + "Host: " + String(host) + "\r\n" + "Connection: close\r\n\r\n");

  unsigned long timeout = millis();
  while (!client->available()) {
    if (millis() - timeout > 5000) {
      client->stop();
      return;
    }
  }

  char endOfHeaders[] = "\r\n\r\n";
  if (!client->find(endOfHeaders)) {
    client->stop();
    return;
  }
}

void getFollower() {
  WiFiClientSecure client;
  client.setInsecure();
  String url = "/x/relation/stat?vmid=" + vmid;
  sendRequest(&client, host2, url);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, client);
  if (error) {
    client.stop();
    return;
  }
  follower = doc["data"]["follower"].as<String>();
  client.stop();
}

void getWeather() {
  WiFiClientSecure client;
  client.setInsecure();
  String url = "/v3/weather/now.json?key=" + apiKey + "&location=" + location + "&language=en&unit=c";
  sendRequest(&client, host1, url);

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

  updateFlag = 0x00;
}

void loop() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  strftime(hm, 6, "%R", &timeinfo);
  strftime(s, 3, "%S", &timeinfo);
  strftime(ymd, 11, "%F", &timeinfo);
  strftime(wday, 4, "%a", &timeinfo);

  if (strcmp(os, s) != 0) {
    updateFlag |= TIME;
    strcpy(os, s);
    if (strcmp(oymd, ymd) != 0) {
      updateFlag |= DATE;
      strcpy(oymd, ymd);
    }
  }

  if (targetTime < millis()) {
    getWeather();
    getFollower();
    updateFlag |= (WEATHER | FOLLOWER);
    targetTime = millis() + 60000 * API_UPDATE_INTERVAL_M;
  }

  updateScreen();
}
