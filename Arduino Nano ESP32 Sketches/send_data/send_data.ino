#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_seesaw.h>
#include <DFRobot_RGBLCD1602.h>

// ---- Settings ----
const char* WIFI_SSID = "Braeden's S21";
const char* WIFI_PASS = "ezto9685";
const char* SERVER_URL = "http://10.17.158.59:8080/data";

const int PH_PIN = 1;
const float PH_NEUTRAL_MV = 1500.0;
const float PH_ACID_MV = 2032.44;
const unsigned long INTERVAL_MS = 1000;
const unsigned long SEND_MS = 5000;

Adafruit_seesaw soil;
DFRobot_RGBLCD1602 lcd(0x2D, 16, 2);

bool soilOk = false;
unsigned long lastRead = 0;
unsigned long lastSend = 0;

float readPH() {
  const int samples = 20;
  float mv = 0;
  for (int i = 0; i < samples; i++) {
    mv += analogReadMilliVolts(PH_PIN);
    delay(5);
  }
  mv /= samples;
  float slope = (7.0 - 4.0) / ((PH_NEUTRAL_MV - 1500.0) / 3.0 - (PH_ACID_MV - 1500.0) / 3.0);
  float intercept = 7.0 - slope * (PH_NEUTRAL_MV - 1500.0) / 3.0;
  return slope * (mv - 1500.0) / 3.0 + intercept;
}

void sendData(float ph, float temp, uint16_t moist) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    return;
  }
  if (isnan(temp)) temp = 0;

  // Peace Lily = real sensor data. Light/Water have no sensor yet, so "--".
  // Snake Plant = placeholder row so row 2 of the site stays filled.
  String json = "[{\"name\":\"Peace Lily\",\"ph\":" + String(ph, 2) +
                ",\"moisture\":" + String(moist) +
                ",\"temp\":" + String(temp, 1) +
                ",\"light\":\"--\",\"water\":\"--\"},"
                "{\"name\":\"Snake Plant\",\"ph\":\"--\",\"moisture\":\"--\","
                "\"temp\":\"--\",\"light\":\"--\",\"water\":\"--\"}]";

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(json);
  Serial.print("POST: ");
  Serial.println(code);
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("1: boot");
  Wire.begin();

  lcd.init();
  lcd.setRGB(0, 255, 0);
  lcd.print("Starting...");
  Serial.println("2: lcd ok");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println(WiFi.status());
    delay(500);
  }
  Serial.println("3: wifi ok");
  lcd.clear();
  lcd.print("WiFi OK");

  soilOk = soil.begin(0x36);
  Serial.println("4: soil done");
  if (!soilOk) {
    Serial.println("Soil sensor not found");
    lcd.clear();
    lcd.print("Soil sensor ERR");
  }
}

void loop() {
  if (millis() - lastRead < INTERVAL_MS) return;
  lastRead = millis();

  float ph = readPH();
  float temp = soilOk ? soil.getTemp() : NAN;
  uint16_t moist = soilOk ? soil.touchRead(0) : 0;

  Serial.print("pH: ");
  Serial.print(ph, 2);
  Serial.print(" | Temp: ");
  Serial.print(temp, 1);
  Serial.print(" C | Moisture: ");
  Serial.println(moist);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("pH ");
  lcd.print(ph, 2);
  lcd.print(" ");
  lcd.print(temp, 1);
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("Moisture ");
  lcd.print(moist);

  if (millis() - lastSend >= SEND_MS) {
    lastSend = millis();
    sendData(ph, temp, moist);
  }
}