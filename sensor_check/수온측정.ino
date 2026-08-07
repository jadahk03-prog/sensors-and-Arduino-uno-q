#include <Arduino.h>
#include <OneWire.h>

#define TEMP_PIN 2

OneWire ds(TEMP_PIN);
byte sensorAddress[8];
bool sensorFound = false;

bool findTemperatureSensor()
{
    ds.reset_search();

    while (ds.search(sensorAddress))
    {
        if (OneWire::crc8(sensorAddress, 7) != sensorAddress[7])
            continue;

        // DS18B20 패밀리 코드
        if (sensorAddress[0] == 0x28)
            return true;
    }

    return false;
}

float readTemperatureC()
{
    byte data[9];

    ds.reset();
    ds.select(sensorAddress);
    ds.write(0x44, 1);  // 온도 변환 시작

    delay(750);

    ds.reset();
    ds.select(sensorAddress);
    ds.write(0xBE);     // Scratchpad 읽기

    for (int i = 0; i < 9; i++)
        data[i] = ds.read();

    if (OneWire::crc8(data, 8) != data[8])
        return NAN;

    int16_t rawTemperature =
        ((int16_t)data[1] << 8) | data[0];

    return rawTemperature / 16.0;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    sensorFound = findTemperatureSensor();

    if (sensorFound)
    {
        Serial.println("DS18B20 온도센서 발견");
    }
    else
    {
        Serial.println("DS18B20 온도센서 없음");
    }
}

void loop()
{
    if (!sensorFound)
    {
        delay(1000);
        return;
    }

    float temperature = readTemperatureC();

    if (isnan(temperature))
    {
        Serial.println("온도 읽기 오류");
    }
    else
    {
        Serial.print("수온: ");
        Serial.print(temperature, 2);
        Serial.println(" °C");
    }

    delay(1000);
}