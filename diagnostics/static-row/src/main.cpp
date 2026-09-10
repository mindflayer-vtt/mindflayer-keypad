#include "StaticRow.h"

#include <ESP8266WiFi.h>

void setup() {
  Serial.begin(115200);
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  StaticRow::begin();
  Serial.println("STATIC-ROW DIAGNOSTIC: GPIO0/D3 LOW continuously (Z/X/C)");
  Serial.println("GPIO5/D1, GPIO4/D2, GPIO2/D4 HIGH; GPIO14/D5, GPIO12/D6, GPIO13/D7 INPUT_PULLUP");
  Serial.println(
      "No scanning, debounce, LEDs, server, OTA, or serial provisioning. Restore by USB.");
}

void loop() {
  Serial.printf("Digital only: D5/Z=%d D6/X=%d D7/C=%d\n", digitalRead(14), digitalRead(12),
                digitalRead(13));
  delay(1000); // Yield to the watchdog without changing row levels.
}
