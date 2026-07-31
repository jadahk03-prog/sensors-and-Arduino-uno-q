#include <Arduino.h>

const int pHPin = A0;

void setup() {
  Serial.begin(9600);
}

void loop() {
  int value = analogRead(pHPin);
  Serial.println(value);
  delay(1000);
}