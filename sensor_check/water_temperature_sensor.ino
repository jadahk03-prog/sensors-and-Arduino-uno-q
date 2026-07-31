#include <Arduino.h>

const byte pin = 2;

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("DS18B20 시작");
}

void loop() {
  if (!ds_reset()) {
    Serial.println("센서를 찾을 수 없습니다.");
    delay(2000);
    return;
  }

  // 온도 변환 시작
  ds_write(0xCC);   // Skip ROM
  ds_write(0x44);   // Convert T

  delay(800);

  if (!ds_reset()) {
    Serial.println("센서 응답 없음");
    delay(2000);
    return;
  }

  ds_write(0xCC);
  ds_write(0xBE);   // Read Scratchpad

  byte lsb = ds_read();
  byte msb = ds_read();

  int16_t raw = (msb << 8) | lsb;
  float temp = raw / 16.0;

  Serial.print("수온 : ");
  Serial.print(temp, 2);
  Serial.println(" °C");

  delay(2000);
}

bool ds_reset() {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(480);

  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(70);

  bool ok = (digitalRead(pin) == LOW);

  delayMicroseconds(410);

  return ok;
}

void ds_write_bit(bool b) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);

  if (b) {
    delayMicroseconds(10);
    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(55);
  } else {
    delayMicroseconds(65);
    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(5);
  }
}

void ds_write(byte data) {
  for (byte i = 0; i < 8; i++) {
    ds_write_bit(data & 0x01);
    data >>= 1;
  }
}

bool ds_read_bit() {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(3);

  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(10);

  bool b = digitalRead(pin);

  delayMicroseconds(53);

  return b;
}

byte ds_read() {
  byte value = 0;

  for (byte i = 0; i < 8; i++) {
    if (ds_read_bit()) {
      value |= (1 << i);
    }
  }

  return value;
}