#include <Arduino.h>
#include <OneWire.h>

// ==================================================
// 센서 연결 핀
// ==================================================
#define TEMP_PIN 2   // DS18B20 DATA → D2
#define PH_PIN A1    // pH 센서 출력 → A1
#define DO_PIN A2    // DO 센서 출력 → A2

// ==================================================
// Arduino UNO Q ADC 설정
// ==================================================
#define ADC_BITS 12
#define ADC_MAX 4095.0
#define ADC_REFERENCE_V 3.3
#define ADC_REFERENCE_MV 3300.0

#define SAMPLE_COUNT 40

// ==================================================
// pH 2점 캘리브레이션값
// 반드시 실제 측정값으로 수정
// ==================================================

// 캘리브레이션 당시 버퍼용액의 실제 pH
// 버퍼용액 통의 온도-pH 표를 보고 입력
#define PH7_BUFFER_VALUE 7.00
#define PH4_BUFFER_VALUE 4.00

// 각 버퍼용액에서 측정한 안정된 전압
// 아래 값은 예시이므로 반드시 수정
#define PH7_VOLTAGE 1.500
#define PH4_VOLTAGE 0.650

// ==================================================
// SEN0237-A DO 1점 캘리브레이션값
// 반드시 실제 캘리브레이션 결과로 수정
// ==================================================

// 예: 25°C에서 포화 전압이 1600mV
#define DO_CAL_V 1600.0
#define DO_CAL_T 25.0

// ==================================================
// DS18B20 설정
// ==================================================
OneWire oneWire(TEMP_PIN);

byte temperatureAddress[8];
bool temperatureSensorFound = false;

// ==================================================
// 온도별 포화 용존산소량
// 0~40°C, 단위 μg/L
// ==================================================
const uint16_t DO_TABLE[41] = {
    14460, 14220, 13820, 13440, 13090,
    12740, 12420, 12110, 11810, 11530,
    11260, 11010, 10770, 10530, 10300,
    10080,  9860,  9660,  9460,  9270,
     9080,  8900,  8730,  8570,  8410,
     8250,  8110,  7960,  7820,  7690,
     7560,  7430,  7300,  7180,  7070,
     6950,  6840,  6730,  6630,  6530,
     6410
};

// ==================================================
// DS18B20 검색
// ==================================================
bool findTemperatureSensor()
{
    oneWire.reset_search();

    while (oneWire.search(temperatureAddress))
    {
        // 주소 CRC 확인
        if (OneWire::crc8(temperatureAddress, 7)
            != temperatureAddress[7])
        {
            continue;
        }

        // DS18B20 패밀리 코드 확인
        if (temperatureAddress[0] == 0x28)
        {
            return true;
        }
    }

    return false;
}

// ==================================================
// DS18B20 수온 측정
// ==================================================
float readTemperatureC()
{
    if (!temperatureSensorFound)
    {
        return NAN;
    }

    byte data[9];

    if (!oneWire.reset())
    {
        return NAN;
    }

    oneWire.select(temperatureAddress);
    oneWire.write(0x44, 1);  // 온도 변환 시작

    // DS18B20 12비트 변환 시간
    delay(750);

    if (!oneWire.reset())
    {
        return NAN;
    }

    oneWire.select(temperatureAddress);
    oneWire.write(0xBE);     // Scratchpad 읽기

    for (int i = 0; i < 9; i++)
    {
        data[i] = oneWire.read();
    }

    // 측정 데이터 CRC 확인
    if (OneWire::crc8(data, 8) != data[8])
    {
        return NAN;
    }

    int16_t rawTemperature =
        ((int16_t)data[1] << 8) | data[0];

    float temperature =
        (float)rawTemperature / 16.0;

    // 비정상 범위 확인
    if (temperature < -55.0 || temperature > 125.0)
    {
        return NAN;
    }

    return temperature;
}

// ==================================================
// 아날로그 센서 평균 ADC 측정
// 최댓값과 최솟값 각각 하나씩 제외
// ==================================================
double readAverageADC(int pin)
{
    uint32_t sum = 0;
    int minValue = 4095;
    int maxValue = 0;

    // 아날로그 채널 변경 직후 첫 값은 버림
    analogRead(pin);
    delay(5);

    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        int value = analogRead(pin);

        sum += value;

        if (value < minValue)
        {
            minValue = value;
        }

        if (value > maxValue)
        {
            maxValue = value;
        }

        delay(20);
    }

    sum -= minValue;
    sum -= maxValue;

    return (double)sum / (SAMPLE_COUNT - 2);
}

// ==================================================
// ADC → V 변환
// ==================================================
float adcToVoltage(double adcValue)
{
    return adcValue * ADC_REFERENCE_V / ADC_MAX;
}

// ==================================================
// ADC → mV 변환
// ==================================================
float adcToMillivolts(double adcValue)
{
    return adcValue * ADC_REFERENCE_MV / ADC_MAX;
}

// ==================================================
// pH 4·7 두 점을 이용한 pH 계산
// ==================================================
float calculatePH(float voltage)
{
    float voltageDifference =
        PH4_VOLTAGE - PH7_VOLTAGE;

    // 두 캘리브레이션 전압이 같으면 계산 불가
    if (fabs(voltageDifference) < 0.001)
    {
        return NAN;
    }

    float slope =
        (PH4_BUFFER_VALUE - PH7_BUFFER_VALUE) /
        voltageDifference;

    float intercept =
        PH7_BUFFER_VALUE -
        (slope * PH7_VOLTAGE);

    return (slope * voltage) + intercept;
}

// ==================================================
// 수온을 이용한 DO 계산
// ==================================================
float calculateDO(float voltageMv, float temperature)
{
    if (isnan(temperature))
    {
        return NAN;
    }

    int temperatureIndex =
        (int)round(temperature);

    // DO 표는 0~40°C 범위
    temperatureIndex =
        constrain(temperatureIndex, 0, 40);

    // 1점 캘리브레이션 온도 보정
    float saturationVoltage =
        DO_CAL_V +
        35.0 *
        ((float)temperatureIndex - DO_CAL_T);

    if (saturationVoltage <= 0)
    {
        return NAN;
    }

    float dissolvedOxygenUgL =
        voltageMv *
        (float)DO_TABLE[temperatureIndex] /
        saturationVoltage;

    // μg/L → mg/L
    return dissolvedOxygenUgL / 1000.0;
}

// ==================================================
// 초기 설정
// ==================================================
void setup()
{
    Serial.begin(115200);
    analogReadResolution(ADC_BITS);

    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("UNO Q Water Quality Measurement");
    Serial.println("Temperature + pH + DO");
    Serial.println("================================");

    temperatureSensorFound =
        findTemperatureSensor();

    if (temperatureSensorFound)
    {
        Serial.println("DS18B20 온도센서 발견");
    }
    else
    {
        Serial.println("DS18B20 온도센서 없음");
        Serial.println("D2 연결과 4.7k 저항을 확인하세요.");
    }

    Serial.println();
}

// ==================================================
// 반복 측정
// ==================================================
void loop()
{
    // 1. 수온 측정
    float waterTemperature =
        readTemperatureC();

    // 2. pH 측정
    double phADC =
        readAverageADC(PH_PIN);

    float phVoltage =
        adcToVoltage(phADC);

    float phValue =
        calculatePH(phVoltage);

    // 3. DO 측정
    double doADC =
        readAverageADC(DO_PIN);

    float doVoltageMv =
        adcToMillivolts(doADC);

    float doValue =
        calculateDO(doVoltageMv, waterTemperature);

    // ==================================================
    // 결과 출력
    // ==================================================
    Serial.println("--------------------------------");

    Serial.print("수온: ");

    if (isnan(waterTemperature))
    {
        Serial.println("측정 오류");
    }
    else
    {
        Serial.print(waterTemperature, 2);
        Serial.println(" C");
    }

    Serial.print("pH ADC: ");
    Serial.print(phADC, 1);

    Serial.print(" | 전압: ");
    Serial.print(phVoltage, 3);
    Serial.print(" V");

    Serial.print(" | pH: ");

    if (isnan(phValue))
    {
        Serial.println("계산 불가");
    }
    else
    {
        Serial.println(phValue, 2);
    }

    Serial.print("DO ADC: ");
    Serial.print(doADC, 1);

    Serial.print(" | 전압: ");
    Serial.print(doVoltageMv, 0);
    Serial.print(" mV");

    Serial.print(" | DO: ");

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