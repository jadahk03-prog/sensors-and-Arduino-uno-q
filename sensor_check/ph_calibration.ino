#include <Arduino.h>

#define PH_PIN A1

#define ADC_BITS 12
#define ADC_MAX 4095.0
#define ADC_REFERENCE 3.3

#define SAMPLE_COUNT 40

// 버퍼용액 표에서 확인한 현재 온도의 pH
#define BUFFER_PH 7.00

int samples[SAMPLE_COUNT];

double readAverageADC()
{
    long sum = 0;

    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        samples[i] = analogRead(PH_PIN);
        sum += samples[i];
        delay(20);
    }

    // 최솟값과 최댓값 찾기
    int minValue = samples[0];
    int maxValue = samples[0];

    for (int i = 1; i < SAMPLE_COUNT; i++)
    {
        if (samples[i] < minValue)
            minValue = samples[i];

        if (samples[i] > maxValue)
            maxValue = samples[i];
    }

    // 최솟값과 최댓값 각각 하나씩 제외
    sum -= minValue;
    sum -= maxValue;

    return (double)sum / (SAMPLE_COUNT - 2);
}

void setup()
{
    Serial.begin(115200);
    analogReadResolution(ADC_BITS);

    delay(1000);

    Serial.println();
    Serial.println("============================");
    Serial.println("pH 7 Calibration");
    Serial.println("============================");
    Serial.println("센서를 버퍼용액에 넣고");
    Serial.println("값이 안정될 때까지 기다리세요.");
    Serial.println();
}

void loop()
{
    double adcAverage = readAverageADC();

    double voltage =
        adcAverage * ADC_REFERENCE / ADC_MAX;

    // DFRobot 기본 계산식에서 필요한 OFFSET 계산
    double calculatedOffset =
        BUFFER_PH - (3.5 * voltage);

    Serial.print("ADC: ");
    Serial.print(adcAverage, 1);

    Serial.print(" | Voltage: ");
    Serial.print(voltage, 3);
    Serial.print(" V");

    Serial.print(" | 필요한 OFFSET: ");
    Serial.println(calculatedOffset, 3);

    delay(1000);
}