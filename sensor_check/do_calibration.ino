#include <Arduino.h>

#define DO_PIN A2

#define ADC_BITS 12
#define ADC_MAX 4095.0
#define ADC_REFERENCE_MV 3300.0

#define SAMPLE_COUNT 40

double readAverageVoltage()
{
    uint32_t sum = 0;
    int minValue = 4095;
    int maxValue = 0;

    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        int value = analogRead(DO_PIN);

        sum += value;

        if (value < minValue)
            minValue = value;

        if (value > maxValue)
            maxValue = value;

        delay(20);
    }

    // 최댓값과 최솟값 각각 하나씩 제외
    sum -= minValue;
    sum -= maxValue;

    double adcAverage =
        (double)sum / (SAMPLE_COUNT - 2);

    return adcAverage * ADC_REFERENCE_MV / ADC_MAX;
}

void setup()
{
    Serial.begin(115200);
    analogReadResolution(ADC_BITS);

    delay(1000);

    Serial.println();
    Serial.println("============================");
    Serial.println("SEN0237-A DO Calibration");
    Serial.println("============================");
    Serial.println("젖은 센서를 공기 중에 놓으세요.");
    Serial.println("값이 안정되면 전압을 기록하세요.");
    Serial.println();
}

void loop()
{
    double voltage = readAverageVoltage();

    double adcValue =
        voltage * ADC_MAX / ADC_REFERENCE_MV;

    Serial.print("ADC: ");
    Serial.print(adcValue, 1);

    Serial.print(" | Saturation Voltage: ");
    Serial.print(voltage, 0);
    Serial.println(" mV");

    delay(1000);
}