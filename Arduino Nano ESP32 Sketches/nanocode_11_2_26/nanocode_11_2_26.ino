// Per-pot sensor sketch: STEMMA moisture + DS18B20 soil temp + DFRobot pH + 16x2 LCD
// Board: Arduino Nano ESP32  |  Tools > Pin Numbering > "By GPIO number (legacy)"
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Adafruit_seesaw.h"
#include "DFRobot_RGBLCD1602.h"
#include "DFRobot_PH.h"

const int PH_PIN = 1;   // A0 = GPIO1 (ADC1)
const int DS_PIN = 5;   // D2 = GPIO5

Adafruit_seesaw ss;
DFRobot_RGBLCD1602 lcd(0x2D, 16, 2);
OneWire oneWire(DS_PIN);
DallasTemperature probe(&oneWire);
DFRobot_PH ph;

float voltage = 0, phValue = 7.0, soilC = 25.0;

// Pads to 16 chars so the LCD never needs clear() (no flicker)
void show(uint8_t row, const char *text) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", text);
  lcd.setCursor(0, row);
  lcd.print(buf);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  EEPROM.begin(32);        // lets the pH calibration persist on ESP32
  ph.begin();
  probe.begin();
  lcd.init();
  lcd.setRGB(0, 255, 0);
  if (!ss.begin(0x36)) {
    show(0, "Soil sensor FAIL");
    Serial.println("Soil sensor not found");
    while (1) delay(10);
  }
  Serial.println("Soil sensor OK");
}

void loop() {
  static unsigned long t = 0;
  if (millis() - t > 1000) {
    t = millis();

    uint16_t moisture = ss.touchRead(0);          // raw capacitance count, no real unit
    probe.requestTemperatures();
    float c = probe.getTempCByIndex(0);           // -127 = probe not detected
    bool probeOk = (c > -100);
    if (probeOk) soilC = c;

    voltage = analogReadMilliVolts(PH_PIN);       // mV
    phValue = ph.readPH(voltage, soilC);          // temp-compensated pH

    char l1[24], l2[24];
    snprintf(l1, sizeof(l1), "M:%uraw pH:%.1f", moisture, phValue);
    if (probeOk) snprintf(l2, sizeof(l2), "Soil:%.1fC", soilC);
    else         snprintf(l2, sizeof(l2), "Soil: no probe");
    show(0, l1);
    show(1, l2);

    Serial.printf("Moisture (raw): %u  Soil temp (C): %.1f  pH: %.2f  pH mV: %.0f\n",
                  moisture, soilC, phValue, voltage);
  }
  ph.calibration(voltage, soilC);   // listens for enterph / calph / exitph over Serial
}
