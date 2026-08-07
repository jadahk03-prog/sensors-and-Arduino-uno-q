#include <Arduino.h>
#include <OneWire.h>

// ================================
// 핀 설정
// ================================
#define TEMP_PIN 2
#define PH_PIN A1
#define DO_PIN A2

// ================================
// UNO Q ADC 설정
// ================================
#define ADC_BITS 12
#define ADC_MAX 4095.0
#define ADC_REFERENCE_V 3.3
#define ADC_REFERENCE_MV 3300.0

#define SAMPLE_COUNT 40

// ================================
// pH 캘리브레이션값
// pH 7 용액으로 구한 OFFSET 입력
// ================================
#define PH_OFFSET 0.000

// ================================
// DO 1점 캘리브레이션값
// 예: 24°C에서 1585mV였다면
// DO_CAL_V 1585
// DO_CAL_T 24
// ================================
#define DO_CAL_V 1600.0
#define DO_CAL_T 25.0

OneWire ds(TEMP_PIN);

byte tempAddress[8];
bool tempSensorFound = false;

// 온도별 포화 용존산소량, 단위 μg/L
const uint16_t DO_TABLE[41] = {
    14460, 14220, 13820, 13440, 13090,
    12740, 12420, 12110, 11810, 11530,
    11260, 11010, 10770, 10530, 10300,
    10080, 9860, 9660, 9460, 9270,
    9080, 8900, 8730, 8570, 8410,
    8250, 8110, 7960, 7820, 7690,
    7560, 7430, 7300, 7180, 7070,
    6950, 6840, 6730, 6630, 6530,
    6410
};

// ================================
// DS18B20 검색
// ================================
bool findTemperatureSensor()
{
    ds.reset_search();

    while (ds.search(tempAddress))
    {
        if (OneWire::crc8(tempAddress, 7) != tempAddress[7])
            continue;

        // DS18B20 패밀리 코드
        if (tempAddress[0] == 0x28)
            return true;
    }

    return false;
}

// ================================
// DS18B20 온도 측정
// ================================
float readTemperatureC()
{
    if (!tempSensorFound)
        return NAN;

    byte data[9];

    ds.reset();
    ds.select(tempAddress);
    ds.write(0x44, 1);

    delay(750);

    ds.reset();
    ds.select(tempAddress);
    ds.write(0xBE);

    for (int i = 0; i < 9; i++)
        data[i] = ds.read();

    if (OneWire::crc8(data, 8) != data[8])
        return NAN;

    int16_t rawTemperature =
        ((int16_t)data[1] << 8) | data[0];

    return rawTemperature / 16.0;
}

// ================================
// 아날로그 센서 평균 ADC 측정
// 최댓값과 최솟값 하나씩 제외
// ================================
double readAverageADC(uint8_t pin)
{
    uint32_t sum = 0;
    int minValue = 4095;
    int maxValue = 0;

    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        int value = analogRead(pin);

        sum += value;

        if (value < minValue)
            minValue = value;

        if (value > maxValue)
            maxValue = value;

        delay(20);
    }

    sum -= minValue;
    sum -= maxValue;

    return (double)sum / (SAMPLE_COUNT - 2);
}

// ================================
// pH 계산
// ================================
float calculatePH(double adcValue)
{
    float voltage =
        adcValue * ADC_REFERENCE_V / ADC_MAX;

    return 3.5 * voltage + PH_OFFSET;
}

// ================================
// DO 계산
// ================================
float calculateDO(double adcValue, float temperature)
{
    int temperatureIndex = (int)round(temperature);

    // 공식 표의 사용 범위는 0~40°C
    temperatureIndex =
        constrain(temperatureIndex, 0, 40);

    float voltageMv =
        adcValue * ADC_REFERENCE_MV / ADC_MAX;

    // 1점 캘리브레이션 온도 보정
    float saturationVoltage =
        DO_CAL_V +
        35.0 * (temperatureIndex - DO_CAL_T);

    if (saturationVoltage <= 0)
        return NAN;

    float dissolvedOxygenUgL =
        voltageMv *
        DO_TABLE[temperatureIndex] /
        saturationVoltage;

    // μg/L → mg/L
    return dissolvedOxygenUgL / 1000.0;
}

void setup()
{
    Serial.begin(115200);
    analogReadResolution(ADC_BITS);

    delay(1000);

    Serial.println();
    Serial.println("============================");
    Serial.println("Temperature + pH + DO");
    Serial.println("============================");

    tempSensorFound = findTemperatureSensor();

    if (tempSensorFound)
        Serial.println("DS18B20 발견");
    else
        Serial.println("DS18B20 없음");

    Serial.println();
}

void loop()
{
    // 1. 수온 측정
    float temperature = readTemperatureC();

    // 2. pH 센서 측정
    double phADC = readAverageADC(PH_PIN);
    float phVoltage =
        phADC * ADC_REFERENCE_V / ADC_MAX;
    float phValue = calculatePH(phADC);

    // 3. DO 센서 측정
    double doADC = readAverageADC(DO_PIN);
    float doVoltageMv =
        doADC * ADC_REFERENCE_MV / ADC_MAX;

    float doValue = NAN;

    if (!isnan(temperature))
        doValue = calculateDO(doADC, temperature);

    Serial.println("----------------------------");

    if (isnan(temperature))
    {
        Serial.println("수온: 측정 오류");
    }
    else
    {
        Serial.print("수온: ");
        Serial.print(temperature, 2);
        Serial.println(" C");
    }

    Serial.print("pH ADC: ");
    Serial.print(phADC, 1);
    Serial.print(" | 전압: ");
    Serial.print(phVoltage, 3);
    Serial.print(" V | pH: ");
    Serial.println(phValue, 2);

    Serial.print("DO ADC: ");
    Serial.print(doADC, 1);
    Serial.print(" | 전압: ");
    Serial.print(doVoltageMv, 0);
    Serial.print(" mV | DO: ");

    if (isnan(doValue))
    {
        Serial.println("계산 불가");
    }
    else
    {
        Serial.print(doValue, 2);
        Serial.println(" mg/L");
    }

    delay(1000);
}