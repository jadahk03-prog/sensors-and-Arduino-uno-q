#include <OneWire.h>
#include <Arduino.h>

OneWire ds(2);

void setup() {
  Serial.begin(9600);
  delay(1000);

  byte addr[8];

  if (ds.search(addr)) {
    Serial.println("센서 발견");
  } else {
    Serial.println("센서 없음");
  }
}

void loop() {
}