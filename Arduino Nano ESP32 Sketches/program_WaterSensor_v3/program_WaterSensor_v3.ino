#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// WIFI / SERVER
const char* WIFI_SSID = "Braeden's S21";
const char* WIFI_PASS = "ezto9685";
const char* WATER_URL = "http://10.17.158.59:8080/water";
const unsigned long sendInterval = 5000;   // ms between uploads
unsigned long lastSend = 0;

// PINS
const int pumpPin = D2;
const int ledPin = LED_BUILTIN;
const int trigPin = D4;
const int echoPin = D3;

// SETTINGS
const float waterLimit = 15.0;          // cm
const unsigned long wateringTime = 3000;
const unsigned long checkInterval = 250;

// VARIABLES
bool watering = false;
unsigned long startTime = 0;
unsigned long lastCheck = 0;

// READ WATER DISTANCE
float readWaterLevel() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, 25000UL);

  if (duration == 0) {
    return -1;
  }

  float distance = duration * 0.0343 / 2.0;

  if (distance < 2.0 || distance > 400.0) {
    return -1;
  }

  return distance;
}

// SEND WATER DISTANCE TO RASPBERRY PI
int failCount = 0;   // consecutive failed uploads

void sendWater(float distance) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost - reconnecting");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    if (++failCount >= 12) ESP.restart();   // ~60 s without WiFi
    return;
  }

  String json = "{\"water\":" + String(distance, 1) + "}";

  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(1500);   // connect can no longer hang the loop
  http.setTimeout(1500);
  http.setReuse(false);           // fresh connection every time
  http.begin(client, WATER_URL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(json);
  Serial.print("POST: ");
  Serial.println(code);
  http.end();
  client.stop();

  if (code == 200) {
    failCount = 0;
  } else if (++failCount >= 12) {
    ESP.restart();                // recover if uploads keep failing
  }
}

// STOP PUMP
void stopWatering() {
  digitalWrite(pumpPin, LOW);
  digitalWrite(ledPin, LOW);

  watering = false;
  Serial.println("Watering Stopped");
}

// START PUMP
void startWatering(const char* mode) {
  if (watering) {
    Serial.println("Pump Already Running");
    return;
  }

  float distance = readWaterLevel();

  if (distance < 0) {
    Serial.println("Sensor Error - Pump Blocked");
    return;
  }

  Serial.print("Water Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance >= waterLimit) {
    Serial.println("LOW WATER - Pump Blocked");
    return;
  }

  watering = true;
  startTime = millis();
  lastCheck = millis();

  digitalWrite(pumpPin, HIGH);
  digitalWrite(ledPin, HIGH);

  Serial.print(mode);
  Serial.println(" Watering Started");
}

// SETUP
void setup() {
  digitalWrite(pumpPin, LOW);
  pinMode(pumpPin, OUTPUT);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  digitalWrite(trigPin, LOW);

  Serial.begin(115200);
  delay(1000);

  Serial.println("System Ready");
  Serial.println("R = Manual Watering");
  Serial.println("A = Automatic Watering Test");
  Serial.println("S = Stop Watering");
  Serial.println("L = Check Water Level");
  Serial.println("Water Limit = 15 cm");

  // Connect WiFi (10 s timeout so the pump still works offline)
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // WiFi power-save can cause stalls
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\nWiFi OK" : "\nWiFi FAILED");
}

// LOOP
void loop() {
  delay(100);
  float distance = readWaterLevel();

  if (distance < 0) {
    Serial.println("Sensor Error");
  } else {
    Serial.print("Water Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    // Upload only when the pump is off (POST can block briefly)
    if (!watering && millis() - lastSend >= sendInterval) {
      lastSend = millis();
      sendWater(distance);
    }
  }

  if (Serial.available()) {
    char command = Serial.read();

    if (command == 'R' || command == 'r') {
      startWatering("Manual");
    }

    else if (command == 'A' || command == 'a') {
      startWatering("Automatic");
    }

    else if (command == 'S' || command == 's') {
      if (watering) {
        stopWatering();
      }
    }

    //else if (command == 'L' || command == 'l') {
    //  delay(100);
    //  float distance = readWaterLevel();

    //  if (distance < 0) {
    //    Serial.println("Sensor Error");
    //  } else {
    //    Serial.print("Water Distance: ");
    //    Serial.print(distance);
    //    Serial.println(" cm");
    //  }
    //}
  }

  // CHECK PUMP WHILE RUNNING
  if (watering) {

    // AUTOMATIC STOP AFTER 3 SECONDS PODEMOSW CAMBIAR EL TIEMPO AQUI
    if (millis() - startTime >= wateringTime) {
      stopWatering();
      Serial.println("3 Seconds Completed");
      return;
    }

    // CHECK WATER LEVEL EVERY 250 ms
    if (millis() - lastCheck >= checkInterval) {
      lastCheck = millis();

      float distance = readWaterLevel();

      if (distance < 0) {
        stopWatering();
        Serial.println("Sensor Error - Pump Stopped");
      }
      else if (distance >= waterLimit) {
        stopWatering();
        Serial.println("LOW WATER - Pump Stopped");
      }
    }
  }
}
