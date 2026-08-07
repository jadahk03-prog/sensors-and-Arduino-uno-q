#include <OneWire.h> 
#include <DS18B20.h> 
DS18B20 DS18B20_Sensor(4); // DS18B20 센서가 연결된 핀 번호
void setup() {
  Serial.begin(9600);
  Serial.println("DS18B20 온도측정 예제");
}
void loop() {
  Serial.println(DS18B20_Sensor.getTempC());
  delay(1000);
}