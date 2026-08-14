#include <Arduino.h>
#include <OneWire.h>
#include <math.h>

// ==================================================
// 핀 연결
// ==================================================
#define TEMP_PIN 4   // DS18B20 DATA → D2
#define PH_PIN A1    // pH 센서 A/PO → A1
#define DO_PIN A5    // DO 센서 DO → A2

// ==================================================
// UNO Q ADC 설정
// ==================================================
#define ADC_BITS 12
#define ADC_MAX 4095.0
#define ADC_REFERENCE_V 3.3
#define ADC_REFERENCE_MV 3300.0
#define SAMPLE_COUNT 40

// ==================================================
// pH 소프트웨어 2점 캘리브레이션값
// 네가 실제 측정한 값 반영
// ==================================================
#define PH7_BUFFER_VALUE 7.00
#define PH4_BUFFER_VALUE 4.00

#define PH7_VOLTAGE 1.142
#define PH4_VOLTAGE 0.888

// pH 4·7 캘리브레이션 당시 평균 수온
#define PH_CALIBRATION_T 25.47

// ==================================================
// DO 캘리브레이션값
// 캘리브레이션 후 실제 포화전압(mV)으로 변경
// ==================================================
#define DO_CAL_V 1505.0
#define DO_CAL_T 25.4   // 캘리브레이션 당시 DS18B20 수온으로 수정
OneWire oneWire(TEMP_PIN);

byte temperatureAddress[8];
bool temperatureSensorFound = false;

// 0~40°C 포화 용존산소량 (μg/L)
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

bool findTemperatureSensor()
{
  oneWire.reset_search();

  while (oneWire.search(temperatureAddress))
  {
    if (OneWire::crc8(temperatureAddress, 7)
        != temperatureAddress[7])
    {
      continue;
    }

    if (temperatureAddress[0] == 0x28)
    {
      return true;
    }
  }

  return false;
}

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
  oneWire.write(0x44, 1);
  delay(750);

  if (!oneWire.reset())
  {
    return NAN;
  }

  oneWire.select(temperatureAddress);
  oneWire.write(0xBE);

  for (int i = 0; i < 9; i++)
  {
    data[i] = oneWire.read();
  }

  if (OneWire::crc8(data, 8) != data[8])
  {
    return NAN;
  }

  int16_t rawTemperature =
    ((int16_t)data[1] << 8) | data[0];

  return rawTemperature / 16.0;
}

double readAverageADC(int pin)
{
  uint32_t sum = 0;
  int minValue = 4095;
  int maxValue = 0;

  analogRead(pin);
  delay(5);

  for (int i = 0; i < SAMPLE_COUNT; i++)
  {
    int value = analogRead(pin);

    sum += value;

    if (value < minValue) minValue = value;
    if (value > maxValue) maxValue = value;

    delay(20);
  }

  sum -= minValue;
  sum -= maxValue;

  return (double)sum / (SAMPLE_COUNT - 2);
}

float adcToVoltage(double adcValue)
{
  return adcValue * ADC_REFERENCE_V / ADC_MAX;
}

float adcToMillivolts(double adcValue)
{
  return adcValue * ADC_REFERENCE_MV / ADC_MAX;
}

// ==================================================
// pH 4·7 소프트웨어 2점 보정 + 수온 감도 보정
// ==================================================
float calculatePH(float voltage, float waterTemperature)
{
  if (isnan(waterTemperature))
  {
    return NAN;
  }

  float voltageDifference = PH4_VOLTAGE - PH7_VOLTAGE;

  if (fabs(voltageDifference) < 0.001)
  {
    return NAN;
  }

  // pH/V 기울기: 현재 값 기준 약 29.412
  float calibrationSlope =
    (PH4_BUFFER_VALUE - PH7_BUFFER_VALUE) /
    voltageDifference;

  // 수온에 따른 pH 전극 감도 보정
  float temperatureSlope =
    calibrationSlope *
    ((PH_CALIBRATION_T + 273.15) /
     (waterTemperature + 273.15));

  // pH 7 캘리브레이션점을 기준으로 계산
  return PH7_BUFFER_VALUE +
         temperatureSlope *
         (voltage - PH7_VOLTAGE);
}

float calculateDO(float voltageMv, float temperature)
{
  if (isnan(temperature) || DO_CAL_V <= 0.0)
  {
    return NAN;
  }

  int temperatureIndex = constrain((int)round(temperature), 0, 40);

  float saturationVoltage =
    DO_CAL_V +
    35.0 * ((float)temperatureIndex - DO_CAL_T);

  if (saturationVoltage <= 0.0)
  {
    return NAN;
  }

  return voltageMv *
         (float)DO_TABLE[temperatureIndex] /
         saturationVoltage / 1000.0;
}

void setup()
{
  Serial.begin(115200);
  analogReadResolution(ADC_BITS);

  delay(1000);

  temperatureSensorFound = findTemperatureSensor();

  Serial.println();
  Serial.println("================================");
  Serial.println("UNO Q Water Quality Measurement");
  Serial.println("Temperature + pH + DO");
  Serial.println("================================");

  if (temperatureSensorFound)
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
  float waterTemperature = readTemperatureC();

  double phADC = readAverageADC(PH_PIN);
  float phVoltage = adcToVoltage(phADC);
  float phValue = calculatePH(phVoltage, waterTemperature);

  double doADC = readAverageADC(DO_PIN);
  float doVoltageMv = adcToMillivolts(doADC);
  float doValue = calculateDO(doVoltageMv, waterTemperature);

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
  Serial.print(" V | pH: ");

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
  Serial.print(" mV | DO: ");

  if (DO_CAL_V <= 0.0)
  {
    Serial.println("캘리브레이션 필요");
  }
  else if (isnan(doValue))
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