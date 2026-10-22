#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Aurebesh10pt7b.h"
#include <time.h>
#include <ArduinoOTA.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Initialize both displays (ESP8266 default I2C pins: SDA=D2, SCL=D1)
Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

WiFiClient espClient;
PubSubClient client(espClient);

// Weather Cache Struct
struct WeatherData {
  float temp = 0.0;
  int code = 0;
};

WeatherData homeWeather;
WeatherData hothWeather;
WeatherData tatWeather;

unsigned long lastScreenRotate = 0;
const unsigned long SCREEN_1_ROTATE_INTERVAL = 5000;    // Rotate display every 5 seconds (ms)
const unsigned long SCREEN_2_ROTATE_INTERVAL = 10000;    // Rotate display every 5 seconds (ms)
int screen1State = 0;

// Screen 2 Data Globals
String g_holo_cpu = "0", g_holo_mem = "0", g_holo_gpu = "0", g_holo_c = "0", g_holo_d = "0";
String g_scarif_cpu = "0", g_scarif_mem = "0", g_scarif_vol2 = "0", g_scarif_vol3 = "0";
String g_unifi_cpu = "0", g_unifi_mem = "0", g_unifi_rx = "0", g_unifi_tx = "0", g_unifi_up = "0";
String g_kuma_down = "0", g_kuma_list = "";
String g_p1s_status = "Unknown", g_p1s_prog = "0", g_p1s_rem = "0";
String g_px_av_cpu = "0", g_px_av_mem = "0", g_px_av_stat = "Unknown";
String g_px_dv_cpu = "0", g_px_dv_mem = "0", g_px_dv_stat = "Unknown";
String g_px_ex_cpu = "0", g_px_ex_mem = "0", g_px_ex_stat = "Unknown";
String g_plex_streams = "0";

unsigned long lastScreen2Rotate = 0;
int screen2State = 0;

int kumaScrollX = SCREEN_WIDTH;
bool kumaScrollCompleted = true;

// Remote Display Power Control (Home Assistant Occupancy)
bool g_displayPower = true;
void updateScreen1();
void updateScreen2();

void setDisplayPower(bool enable) {
  if (g_displayPower == enable) return;
  g_displayPower = enable;

  if (g_displayPower) {
    Serial.println("Display Power: ON (Waking OLEDs)");
    display1.ssd1306_command(SSD1306_DISPLAYON);
    display2.ssd1306_command(SSD1306_DISPLAYON);
    updateScreen1();
    updateScreen2();
  } else {
    Serial.println("Display Power: OFF (Sleeping OLEDs)");
    display1.clearDisplay();
    display1.display();
    display2.clearDisplay();
    display2.display();
    display1.ssd1306_command(SSD1306_DISPLAYOFF);
    display2.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}

// --- Helper Function to Center Text ---
void printCentered(Adafruit_SSD1306 &disp, const String &text, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  disp.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  disp.setCursor((SCREEN_WIDTH - w) / 2 - x1, y);
  disp.print(text);
}

// --- Mini Progress Bar Helper ---
void drawBar(Adafruit_SSD1306 &disp, int x, int y, int w, int h, int percent) {
  disp.drawRect(x, y, w, h, SSD1306_WHITE);
  int innerW = w - 2;
  int innerH = h - 2;
  if (innerW > 0 && innerH > 0) {
    int fillW = map(constrain(percent, 0, 100), 0, 100, 0, innerW);
    if (fillW > 0) {
      disp.fillRect(x + 1, y + 1, fillW, innerH, SSD1306_WHITE);
    }
  }
}

// Check if any infrastructure alert is currently active
bool hasActiveAlert() {
  bool isKumaAlert = (g_kuma_down != "0" && g_kuma_down != "");
  bool isPxAlert = ((g_px_av_stat != "online" && g_px_av_stat != "UP" && g_px_av_stat != "Unknown") ||
                    (g_px_dv_stat != "online" && g_px_dv_stat != "UP" && g_px_dv_stat != "Unknown") ||
                    (g_px_ex_stat != "online" && g_px_ex_stat != "UP" && g_px_ex_stat != "Unknown"));
  return isKumaAlert || isPxAlert;
}

// Uniform Header with optional blinking Alert Pip
void drawHeader(Adafruit_SSD1306 &disp, const String &title, bool checkAlert = true) {
  disp.setFont(NULL);
  disp.setTextSize(1);
  disp.setTextColor(SSD1306_WHITE);
  printCentered(disp, title, 0);
  disp.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  if (checkAlert && hasActiveAlert()) {
    // Blinks every 500ms
    if ((millis() / 500) % 2 == 0) {
      disp.fillRoundRect(112, 0, 15, 9, 2, SSD1306_WHITE);
      disp.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      disp.setCursor(117, 1);
      disp.print("!");
      disp.setTextColor(SSD1306_WHITE);
    } else {
      disp.drawRoundRect(112, 0, 15, 9, 2, SSD1306_WHITE);
      disp.setCursor(117, 1);
      disp.print("!");
    }
  }
}

// --- Weather Geometry Functions ---
void drawCloud(Adafruit_SSD1306 &disp, int cx, int cy) {
  disp.fillCircle(cx, cy - 2, 4, SSD1306_WHITE);
  disp.fillCircle(cx - 5, cy + 1, 3, SSD1306_WHITE);
  disp.fillCircle(cx + 5, cy + 1, 3, SSD1306_WHITE);
  disp.fillRect(cx - 5, cy - 1, 11, 6, SSD1306_WHITE);
}

void drawWeatherIcon(Adafruit_SSD1306 &disp, int cx, int cy, int kind) {
  switch (kind) {
    case 0: { // sun
      disp.fillCircle(cx, cy, 5, SSD1306_WHITE);
      for (int a = 0; a < 360; a += 45) {
        float r = a * PI / 180.0;
        disp.drawLine(cx + (int)(cos(r) * 7), cy + (int)(sin(r) * 7),
                      cx + (int)(cos(r) * 9), cy + (int)(sin(r) * 9), SSD1306_WHITE);
      }
      break;
    }
    case 1: // partly cloudy
      disp.fillCircle(cx - 4, cy - 4, 4, SSD1306_WHITE); // mini sun
      drawCloud(disp, cx + 2, cy + 2);
      break;
    case 2: // overcast
      drawCloud(disp, cx, cy);
      break;
    case 3: // fog
      drawCloud(disp, cx, cy - 2);
      for (int i = 0; i < 3; i++) disp.drawFastHLine(cx - 7, cy + 5 + i * 2, 15, SSD1306_WHITE);
      break;
    case 4: // rain
      drawCloud(disp, cx, cy - 2);
      for (int i = -4; i <= 6; i += 5) disp.drawLine(cx + i, cy + 4, cx + i - 2, cy + 8, SSD1306_WHITE);
      break;
    case 5: // snow
      drawCloud(disp, cx, cy - 2);
      for (int i = -4; i <= 6; i += 5) {
        disp.drawPixel(cx + i, cy + 6, SSD1306_WHITE);
        disp.drawFastHLine(cx + i - 1, cy + 6, 3, SSD1306_WHITE);
        disp.drawFastVLine(cx + i, cy + 5, 3, SSD1306_WHITE);
      }
      break;
    case 6: // storm
      drawCloud(disp, cx, cy - 2);
      disp.drawLine(cx, cy + 4, cx - 3, cy + 7, SSD1306_WHITE);
      disp.drawLine(cx - 3, cy + 7, cx + 1, cy + 7, SSD1306_WHITE);
      disp.drawLine(cx + 1, cy + 7, cx - 2, cy + 10, SSD1306_WHITE);
      break;
  }
}

String wxText(int code) {
  if (code == 0)                  return "CLEAR";
  if (code <= 2)                  return "PARTLY CLOUDY";
  if (code == 3)                  return "OVERCAST";
  if (code == 45 || code == 48)   return "FOG";
  if (code >= 51 && code <= 57)   return "DRIZZLE";
  if (code >= 61 && code <= 67)   return "RAIN";
  if (code >= 71 && code <= 77)   return "SNOW";
  if (code >= 80 && code <= 82)   return "SHOWERS";
  if (code >= 85 && code <= 86)   return "SNOW";
  if (code >= 95)                 return "STORM";
  return "UNKNOWN";
}

int wxKind(int code) {
  if (code == 0)                                       return 0;
  if (code <= 2)                                       return 1;
  if (code == 3)                                       return 2;
  if (code == 45 || code == 48)                        return 3;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return 4;
  if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) return 5;
  if (code >= 95)                                      return 6;
  return 2;
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;

    // Animate Tactical Link progress bar on Display 2
    int progress = map(constrain(attempts, 0, 30), 0, 30, 15, 95);
    display2.fillRect(14, 42, 100, 8, SSD1306_BLACK);
    drawBar(display2, 14, 42, 100, 8, progress);
    display2.display();
  }

  display2.clearDisplay();
  display2.setFont(NULL);
  display2.setTextSize(1);
  display2.setTextColor(SSD1306_WHITE);
  printCentered(display2, "TACTICAL COMM-LINK", 2);
  display2.drawLine(0, 12, SCREEN_WIDTH, 12, SSD1306_WHITE);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected, IP: ");
    Serial.println(WiFi.localIP());

    printCentered(display2, "LINK ESTABLISHED", 25);
    String ipStr = "IP: " + WiFi.localIP().toString();
    printCentered(display2, ipStr, 42);
    display2.display();
    delay(1200); // Brief pause to view assigned IP before dashboard starts
  } else {
    Serial.println("\nWiFi connection timed out; will retry in background.");
    printCentered(display2, "OFFLINE MODE", 25);
    printCentered(display2, "RETRYING IN BG", 42);
    display2.display();
    delay(1000);
  }
}

unsigned long lastMqttRetry = 0;
const unsigned long MQTT_RETRY_INTERVAL = 5000;

void ensureMqttConnected() {
  if (client.connected()) return;

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastMqttRetry >= MQTT_RETRY_INTERVAL) {
    lastMqttRetry = currentMillis;
    Serial.print("Attempting MQTT connection...");

    String clientId = "HolonetClient-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected!");
      client.subscribe("displays/Holonet");
      client.subscribe("displays/Holonet/power");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" will retry in 5s");
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  // Dedicated Power Topic for Ecobee Occupancy / Home Assistant
  if (strcmp(topic, "displays/Holonet/power") == 0) {
    char cmd[16] = {0};
    unsigned int copyLen = (length < sizeof(cmd) - 1) ? length : (sizeof(cmd) - 1);
    memcpy(cmd, payload, copyLen);
    cmd[copyLen] = '\0';
    if (strcasecmp(cmd, "off") == 0 || strcmp(cmd, "0") == 0 || strcasecmp(cmd, "false") == 0) {
      setDisplayPower(false);
    } else {
      setDisplayPower(true);
    }
    return;
  }

  // Use heap-managed JsonDocument to avoid eating half the 4KB stack
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("JSON Parsing failed: ");
    Serial.println(error.c_str());
    return;
  }

  // Optional power toggle within main JSON payload
  JsonVariant powerVar = !doc["display_power"].isNull() ? doc["display_power"] : doc["screen_power"];
  if (!powerVar.isNull()) {
    if (powerVar.is<bool>()) {
      setDisplayPower(powerVar.as<bool>());
    } else {
      String p = powerVar.as<String>();
      p.toLowerCase();
      if (p == "off" || p == "0" || p == "false") {
        setDisplayPower(false);
      } else {
        setDisplayPower(true);
      }
    }
  }

  // Parse Holocron
  g_holo_cpu = String(doc["holocron_cpu"] | "0");
  g_holo_mem = String(doc["holocron_mem"] | "0");
  g_holo_gpu = String(doc["holocron_gpu"] | "0");
  g_holo_c = String(doc["holocron_c"] | "0");
  g_holo_d = String(doc["holocron_d"] | "0");

  // Parse Scarif
  g_scarif_cpu = String(doc["scarif_cpu"] | "0");
  g_scarif_mem = String(doc["scarif_mem"] | "0");
  g_scarif_vol2 = String(doc["scarif_vol2"] | "0");
  g_scarif_vol3 = String(doc["scarif_vol3"] | "0");

  // Parse Unifi
  g_unifi_cpu = String(doc["unifi_cpu"] | "0");
  g_unifi_mem = String(doc["unifi_mem"] | "0");
  g_unifi_rx = String(doc["unifi_rx"] | "0");
  g_unifi_tx = String(doc["unifi_tx"] | "0");
  g_unifi_up = String(doc["unifi_up"] | "0");

  // Parse Kuma
  g_kuma_down = String(doc["kuma_down"] | "0");
  g_kuma_list = String(doc["kuma_list"] | "");

  // Parse P1S
  g_p1s_status = String(doc["p1s_status"] | "Unknown");
  g_p1s_prog = String(doc["p1s_prog"] | "0");
  g_p1s_rem = String(doc["p1s_rem"] | "0");

  // Parse Proxmox
  g_px_av_cpu = String(doc["px_av_cpu"] | "0");
  g_px_av_mem = String(doc["px_av_mem"] | "0");
  g_px_av_stat = String(doc["px_av_stat"] | "Unknown");
  
  g_px_dv_cpu = String(doc["px_dv_cpu"] | "0");
  g_px_dv_mem = String(doc["px_dv_mem"] | "0");
  g_px_dv_stat = String(doc["px_dv_stat"] | "Unknown");
  
  g_px_ex_cpu = String(doc["px_ex_cpu"] | "0");
  g_px_ex_mem = String(doc["px_ex_mem"] | "0");
  g_px_ex_stat = String(doc["px_ex_stat"] | "Unknown");

  // Parse Plex
  g_plex_streams = String(doc["plex_streams"] | "0");

  // Parse Weather (relayed via Home Assistant)
  homeWeather.temp = doc["wx_home_temp"] | homeWeather.temp;
  homeWeather.code = doc["wx_home_code"] | homeWeather.code;
  hothWeather.temp = doc["wx_hoth_temp"] | hothWeather.temp;
  hothWeather.code = doc["wx_hoth_code"] | hothWeather.code;
  tatWeather.temp  = doc["wx_tat_temp"]  | tatWeather.temp;
  tatWeather.code  = doc["wx_tat_code"]  | tatWeather.code;

  // Detect emergency alert transition for instant jump
  static bool prevAlertState = false;
  bool isKumaAlert = (g_kuma_down != "0" && g_kuma_down != "");
  bool isPxAlert = ((g_px_av_stat != "online" && g_px_av_stat != "UP" && g_px_av_stat != "Unknown") ||
                    (g_px_dv_stat != "online" && g_px_dv_stat != "UP" && g_px_dv_stat != "Unknown") ||
                    (g_px_ex_stat != "online" && g_px_ex_stat != "UP" && g_px_ex_stat != "Unknown"));
  bool currentAlertState = isKumaAlert || isPxAlert;

  if (currentAlertState && !prevAlertState) {
    setDisplayPower(true); // Automatically wake OLEDs on emergency alert!
    if (isKumaAlert) {
      screen2State = 3; // Immediately jump to Kuma Alerts
      kumaScrollX = SCREEN_WIDTH;
      kumaScrollCompleted = false;
    } else if (isPxAlert) {
      screen2State = 5; // Immediately jump to Proxmox Fleet
    }
    lastScreen2Rotate = millis();
    updateScreen2();
  }
  prevAlertState = currentAlertState;
}

// --- Helper Function for Weather Screens ---
void drawWeatherScreen(String title, float temp, int code) {
  drawHeader(display1, title, true);
  
  // Draw geometric weather icon at X=20, Y=28
  drawWeatherIcon(display1, 20, 28, wxKind(code));
  
  // Big Temperature
  display1.setTextSize(2);
  display1.setCursor(38, 23); 
  display1.print(temp, 0);
  display1.print(" F");
  
  // Condition text centered at the bottom
  display1.setTextSize(1);
  printCentered(display1, wxText(code), 52);
}

void updateScreen1() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  bool timeValid = (timeinfo.tm_year > 120);
  char timeStr[12];
  char dateStr[16];
  char gstTimeStr[12];
  char gstDateStr[16];

  if (timeValid) {
    strftime(timeStr, sizeof(timeStr), "%I:%M %p", &timeinfo);
    strftime(dateStr, sizeof(dateStr), "%b %d, %Y", &timeinfo);

    // Galactic Standard 24h Military Format & Czerka standard date
    strftime(gstTimeStr, sizeof(gstTimeStr), "%H:%M:%S", &timeinfo);
    strftime(gstDateStr, sizeof(gstDateStr), "%y.%m.%d", &timeinfo);

    // Auto night-dimming (11 PM - 6 AM) for OLED burn-in prevention
    static bool isDimmed = false;
    bool shouldDim = (timeinfo.tm_hour >= 23 || timeinfo.tm_hour < 6);
    if (shouldDim != isDimmed) {
      isDimmed = shouldDim;
      display1.dim(shouldDim);
      display2.dim(shouldDim);
    }
  } else {
    strcpy(timeStr, "12:00 AM");
    strcpy(dateStr, "Syncing...");
    strcpy(gstTimeStr, "00:00:00");
    strcpy(gstDateStr, "SYNCING");
  }

  display1.clearDisplay();
  display1.setTextColor(SSD1306_WHITE);

  switch (screen1State) {
    case 0: // English Date & Time
      drawHeader(display1, "LOCAL TIME", true);
      
      display1.setTextSize(2);
      printCentered(display1, timeStr, 22);
      
      display1.setTextSize(1);
      printCentered(display1, dateStr, 50);
      break;

    case 1: // Local Weather
      drawWeatherScreen("LOCAL WEATHER", homeWeather.temp, homeWeather.code);
      break;

    case 2: // Aurebesh Date & Time
      drawHeader(display1, "GALACTIC STANDARD", true);

      display1.setFont(&Aurebesh10pt7b); // Custom font
      display1.setTextSize(1);
      
      printCentered(display1, gstTimeStr, 34);
      printCentered(display1, gstDateStr, 58);
      break;

    case 3: // Hoth Weather 
      drawWeatherScreen("SYSTEM: HOTH", hothWeather.temp, hothWeather.code);
      break;

    case 4: // Tatooine Weather 
      drawWeatherScreen("SYSTEM: TATOOINE", tatWeather.temp, tatWeather.code);
      break;
  }

  display1.display();
}

void updateScreen2() {
  display2.clearDisplay();
  display2.setFont(NULL);
  display2.setTextSize(1);
  display2.setTextColor(SSD1306_WHITE);

  switch (screen2State) {
    case 0: { // Holocron
      drawHeader(display2, "HOLOCRON");
      
      // Row 1: CPU & RAM
      display2.setCursor(0, 14);
      display2.print("CPU");
      drawBar(display2, 22, 15, 38, 6, g_holo_cpu.toInt());
      display2.setCursor(64, 14);
      display2.print("RAM");
      drawBar(display2, 86, 15, 38, 6, g_holo_mem.toInt());

      // Row 2: GPU
      display2.setCursor(0, 25);
      display2.print("GPU");
      drawBar(display2, 22, 26, 38, 6, g_holo_gpu.toInt());
      display2.setCursor(66, 25);
      display2.print(g_holo_gpu);
      display2.print("%");

      // Separator
      display2.drawFastHLine(0, 36, SCREEN_WIDTH, SSD1306_WHITE);

      // Row 3: C Drive (% Free)
      display2.setCursor(0, 41);
      display2.print("C:");
      display2.print(g_holo_c);
      display2.print("%F");
      drawBar(display2, 54, 42, 72, 6, 100 - g_holo_c.toInt());

      // Row 4: D Drive (% Free)
      display2.setCursor(0, 52);
      display2.print("D:");
      display2.print(g_holo_d);
      display2.print("%F");
      drawBar(display2, 54, 53, 72, 6, 100 - g_holo_d.toInt());
      break;
    }

    case 1: { // Scarif
      drawHeader(display2, "SCARIF NAS");

      // Row 1: CPU & RAM
      display2.setCursor(0, 14);
      display2.print("CPU");
      drawBar(display2, 22, 15, 38, 6, g_scarif_cpu.toInt());
      display2.setCursor(64, 14);
      display2.print("RAM");
      drawBar(display2, 86, 15, 38, 6, g_scarif_mem.toInt());

      // Separator
      display2.drawFastHLine(0, 25, SCREEN_WIDTH, SSD1306_WHITE);

      // Volumes
      display2.setCursor(0, 29);
      display2.print("VOL2: ");
      display2.print(g_scarif_vol2);
      display2.print("%");
      drawBar(display2, 0, 39, SCREEN_WIDTH, 7, g_scarif_vol2.toInt());

      display2.setCursor(0, 48);
      display2.print("VOL3: ");
      display2.print(g_scarif_vol3);
      display2.print("%");
      drawBar(display2, 0, 57, SCREEN_WIDTH, 7, g_scarif_vol3.toInt());
      break;
    }

    case 2: { // Unifi
      drawHeader(display2, "UNIFI UDM");

      // CPU & RAM
      display2.setCursor(0, 15);
      display2.print("CPU");
      drawBar(display2, 22, 16, 38, 6, g_unifi_cpu.toInt());
      display2.setCursor(64, 15);
      display2.print("RAM");
      drawBar(display2, 86, 16, 38, 6, g_unifi_mem.toInt());

      // Divider
      display2.drawFastHLine(0, 27, SCREEN_WIDTH, SSD1306_WHITE);

      // Traffic
      display2.setCursor(4, 33);
      display2.write(0x19); // Down arrow
      display2.print(" RX: ");
      display2.print(g_unifi_rx);
      display2.print(" Mbps");

      display2.setCursor(4, 48);
      display2.write(0x18); // Up arrow
      display2.print(" TX: ");
      display2.print(g_unifi_tx);
      display2.print(" Mbps");
      break;
    }

    case 3: { // Kuma
      drawHeader(display2, "KUMA ALERTS", false);

      if (g_kuma_down == "0" || g_kuma_down == "") {
        display2.drawRoundRect(8, 18, 112, 38, 4, SSD1306_WHITE);
        printCentered(display2, "FLEET STATUS", 24);
        printCentered(display2, "ALL NOMINAL", 38);
      } else {
        display2.fillRoundRect(6, 14, 116, 18, 3, SSD1306_WHITE);
        display2.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        String alertText = "ALERT: " + g_kuma_down + " DOWN";
        printCentered(display2, alertText, 19);
        display2.setTextColor(SSD1306_WHITE);

        display2.drawRect(0, 38, SCREEN_WIDTH, 22, SSD1306_WHITE);
        display2.setTextWrap(false);
        display2.setCursor(kumaScrollX, 45);
        display2.print(g_kuma_list);
        display2.setTextWrap(true);
      }
      break;
    }

    case 4: { // P1S
      drawHeader(display2, "SABER FORGE");

      int prog = g_p1s_prog.toInt();
      display2.setCursor(0, 15);
      display2.print("Status: ");
      display2.println(g_p1s_status);

      display2.setCursor(0, 27);
      display2.print("Prog: ");
      display2.print(prog);
      display2.print("%");
      display2.setCursor(70, 27);
      display2.print("ETA: ");
      display2.print(g_p1s_rem);
      display2.print("m");

      // Full-width progress gauge
      drawBar(display2, 0, 42, SCREEN_WIDTH, 14, prog);
      break;
    }

    case 5: { // Proxmox Fleet
      drawHeader(display2, "PROXMOX FLEET", false);

      struct PxNode {
        const char* name;
        const String &stat;
        int cpu;
        int mem;
        int y;
      };

      PxNode nodes[3] = {
        {"AV", g_px_av_stat, g_px_av_cpu.toInt(), g_px_av_mem.toInt(), 15},
        {"DV", g_px_dv_stat, g_px_dv_cpu.toInt(), g_px_dv_mem.toInt(), 31},
        {"EX", g_px_ex_stat, g_px_ex_cpu.toInt(), g_px_ex_mem.toInt(), 47}
      };

      for (int i = 0; i < 3; i++) {
        display2.setCursor(0, nodes[i].y);
        display2.print(nodes[i].name);

        bool online = (nodes[i].stat == "online" || nodes[i].stat == "UP");
        if (online) {
          display2.fillCircle(18, nodes[i].y + 3, 2, SSD1306_WHITE);
          display2.setCursor(24, nodes[i].y);
          display2.print("C");
          drawBar(display2, 32, nodes[i].y + 1, 30, 6, nodes[i].cpu);
          display2.setCursor(68, nodes[i].y);
          display2.print("M");
          drawBar(display2, 76, nodes[i].y + 1, 30, 6, nodes[i].mem);
        } else {
          display2.drawCircle(18, nodes[i].y + 3, 2, SSD1306_WHITE);
          display2.setCursor(26, nodes[i].y);
          display2.print("OFFLINE");
        }
      }
      break;
    }

    case 6: { // Plex
      drawHeader(display2, "PLEX TRANSMISSIONS");
      
      int streams = g_plex_streams.toInt();
      display2.setTextSize(3);
      printCentered(display2, g_plex_streams, 20);

      display2.setTextSize(1);
      if (streams == 1) {
        printCentered(display2, "ACTIVE STREAM", 49);
      } else {
        printCentered(display2, "ACTIVE STREAMS", 49);
      }
      break;
    }
  }
  
  display2.display();
}

void setup() {
  Serial.begin(115200);

  // Initialize I2C bus at 400kHz fast mode (Default ESP8266 pins: SDA=D2, SCL=D1)
  Wire.begin();
  Wire.setClock(400000);

  // Initialize display 1 (Default Address 0x3C)
  if(!display1.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 #1 allocation failed");
  }
  
  // Initialize display 2 (Modded Address 0x3D)
  if(!display2.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    Serial.println("SSD1306 #2 allocation failed");
  }

  // Clear the buffer for both screens
  display1.clearDisplay();
  display1.display();
  display2.clearDisplay();
  display2.display();

  // Imperial Boot Splash - Display 1
  display1.setFont(NULL);
  display1.setTextSize(1);
  display1.setTextColor(SSD1306_WHITE);
  printCentered(display1, "IMPERIAL HOLONET", 2);
  display1.drawLine(0, 12, SCREEN_WIDTH, 12, SSD1306_WHITE);
  
  display1.setFont(&Aurebesh10pt7b);
  display1.setTextSize(1);
  printCentered(display1, "SEC-TERM 01", 36);
  
  display1.setFont(NULL);
  display1.setTextSize(1);
  printCentered(display1, "SUBSYSTEM ONLINE", 52);
  display1.display();

  // Imperial Boot Splash - Display 2
  display2.setFont(NULL);
  display2.setTextSize(1);
  display2.setTextColor(SSD1306_WHITE);
  printCentered(display2, "TACTICAL COMM-LINK", 2);
  display2.drawLine(0, 12, SCREEN_WIDTH, 12, SSD1306_WHITE);
  printCentered(display2, "INITIALIZING...", 26);
  drawBar(display2, 14, 42, 100, 8, 10);
  display2.display();

  setup_wifi();

  // Configure ArduinoOTA Wireless Flashing
  ArduinoOTA.setHostname("holonet-display");
  ArduinoOTA.onStart([]() {
    display1.clearDisplay();
    display1.setFont(NULL);
    display1.setTextSize(1);
    display1.setTextColor(SSD1306_WHITE);
    printCentered(display1, "OTA FIRMWARE UPDATE", 15);
    display1.display();

    display2.clearDisplay();
    display2.setFont(NULL);
    display2.setTextSize(1);
    display2.setTextColor(SSD1306_WHITE);
    printCentered(display2, "TRANSMITTING DATA", 15);
    display2.display();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int pct = (progress * 100) / total;
    drawBar(display1, 10, 36, 108, 12, pct);
    display1.display();
    drawBar(display2, 10, 36, 108, 12, pct);
    display2.display();
  });
  ArduinoOTA.onEnd([]() {
    display1.clearDisplay();
    printCentered(display1, "UPDATE COMPLETE", 28);
    display1.display();

    display2.clearDisplay();
    printCentered(display2, "REBOOTING SYSTEM", 28);
    display2.display();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]\n", error);
  });
  ArduinoOTA.begin();

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
  tzset();

  // Port 1883 is standard for unencrypted MQTT
  client.setBufferSize(2048);
  client.setServer(MQTT_SERVER, 1883);

  client.setCallback(callback);
}

void loop() {
  ArduinoOTA.handle();

  if (!client.connected()) {
    ensureMqttConnected();
  } else {
    client.loop();
  }

  // Display rendering and rotation routines (only active when displays are powered on)
  if (g_displayPower) {
    // Rotate Screen 1
    if (millis() - lastScreenRotate >= SCREEN_1_ROTATE_INTERVAL) {
      lastScreenRotate = millis();
      screen1State = (screen1State + 1) % 5; // Cycles 0 -> 1 -> 2 -> 3 -> 4
      updateScreen1();
    }

    // Rotate Screen 2 (Hold screen if an alert ticker is currently scrolling)
    bool holdForKuma = (screen2State == 3 && g_kuma_down != "0" && g_kuma_down != "" && !kumaScrollCompleted);
    if (!holdForKuma && (millis() - lastScreen2Rotate >= SCREEN_2_ROTATE_INTERVAL)) {
      lastScreen2Rotate = millis();
      screen2State = (screen2State + 1) % 7; 
      kumaScrollX = SCREEN_WIDTH; // Reset scroll position on page turn
      kumaScrollCompleted = false;
      updateScreen2();
    }

    // Handle Kuma Scrolling Marquee
    static unsigned long lastKumaScroll = 0;
    if (screen2State == 3 && g_kuma_down != "0" && g_kuma_down != "") {
      if (millis() - lastKumaScroll >= 40) { // 40ms interval, smooth over 400kHz I2C
        lastKumaScroll = millis();
        kumaScrollX -= 2; // Slide left by 2 pixels
        
        // Temporarily disable wrap to get the TRUE pixel width of the single-line string
        display2.setTextWrap(false);
        int16_t x1, y1;
        uint16_t w, h;
        display2.getTextBounds(g_kuma_list, 0, 0, &x1, &y1, &w, &h);
        display2.setTextWrap(true);
        
        // Cast 'w' to int so the negative sign doesn't break the 16-bit unsigned integer
        if (kumaScrollX < -(int)w) {
          kumaScrollX = SCREEN_WIDTH; // Reset to the right edge once it clears
          kumaScrollCompleted = true; // Alert finished one full pass
        }
        updateScreen2(); 
      }
    }

    // Periodic refresh when alert is active so the [!] pip blinks smoothly
    static unsigned long lastAlertBlink = 0;
    if (hasActiveAlert() && (millis() - lastAlertBlink >= 500)) {
      lastAlertBlink = millis();
      if (screen2State != 3) { // Screen 2 State 3 already redraws on scroll
        updateScreen2();
      }
      updateScreen1();
    }
  }
}