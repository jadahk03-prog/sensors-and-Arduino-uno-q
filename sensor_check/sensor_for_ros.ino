#include <Arduino.h>
#include <OneWire.h>
#include <math.h>

// ==================================================
// 핀 연결
// ==================================================
#define TEMP_PIN 4       // DS18B20 DATA → D4
#define PH_PIN A1        // pH 센서 A/PO → A1
#define DO_PIN A5        // DO 센서 DO → A5
#define TURBIDITY_PIN A0 // 탁도센서 AO → A0 (전압분배기 거친 뒤)

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
// ==================================================
#define PH7_BUFFER_VALUE 7.00
#define PH4_BUFFER_VALUE 4.00

#define PH7_VOLTAGE 1.142
#define PH4_VOLTAGE 0.888

#define PH_CALIBRATION_T 25.47

// ==================================================
// DO 캘리브레이션값
// ==================================================
#define DO_CAL_V 1505.0
#define DO_CAL_T 25.4

// ==================================================
// 탁도 캘리브레이션값
// 전압이 높을수록 맑은 물
// ==================================================

// AO -- 10kΩ -- A0 -- 20kΩ -- GND
// 실제 탁도센서 AO 전압 = A0 전압 × 1.5
#define TURBIDITY_DIVIDER_RATIO 1.5

#define CLEAR_WATER_VOLTAGE 4.810   // 정제수 기준: 맑음도 100%
#define VERY_TURBID_VOLTAGE 0.140   // 매우 탁한 물: 맑음도 0%

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

// ==================================================
// DS18B20
// ==================================================
bool findTemperatureSensor() {
  oneWire.reset_search();

  while (oneWire.search(temperatureAddress)) {
    if (OneWire::crc8(temperatureAddress, 7) != temperatureAddress[7]) {
      continue;
    }

    if (temperatureAddress[0] == 0x28) {
      return true;
    }
  }

  return false;
}

float readTemperatureC() {
  if (!temperatureSensorFound) {
    return NAN;
  }

  byte data[9];

  if (!oneWire.reset()) {
    return NAN;
  }

  oneWire.select(temperatureAddress);
  oneWire.write(0x44, 1);
  delay(750);

  if (!oneWire.reset()) {
    return NAN;
  }

  oneWire.select(temperatureAddress);
  oneWire.write(0xBE);

  for (int i = 0; i < 9; i++) {
    data[i] = oneWire.read();
  }

  if (OneWire::crc8(data, 8) != data[8]) {
    return NAN;
  }

  int16_t rawTemperature = ((int16_t)data[1] << 8) | data[0];

  return rawTemperature / 16.0;
}

// ==================================================
// ADC: 최댓값/최솟값 제거 평균
// ==================================================
double readAverageADC(int pin) {
  uint32_t sum = 0;
  int minValue = 4095;
  int maxValue = 0;

  analogRead(pin);
  delay(5);

  for (int i = 0; i < SAMPLE_COUNT; i++) {
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

float adcToVoltage(double adcValue) {
  return adcValue * ADC_REFERENCE_V / ADC_MAX;
}

float adcToMillivolts(double adcValue) {
  return adcValue * ADC_REFERENCE_MV / ADC_MAX;
}

// ==================================================
// pH 계산
// ==================================================
float calculatePH(float voltage, float waterTemperature) {
  if (isnan(waterTemperature)) {
    return NAN;
  }

  float voltageDifference = PH4_VOLTAGE - PH7_VOLTAGE;

  if (fabs(voltageDifference) < 0.001) {
    return NAN;
  }

  float calibrationSlope =
    (PH4_BUFFER_VALUE - PH7_BUFFER_VALUE) / voltageDifference;

  float temperatureSlope =
    calibrationSlope *
    ((PH_CALIBRATION_T + 273.15) / (waterTemperature + 273.15));

  return PH7_BUFFER_VALUE +
         temperatureSlope * (voltage - PH7_VOLTAGE);
}

// ==================================================
// DO 계산
// ==================================================
float calculateDO(float voltageMv, float temperature) {
  if (isnan(temperature) || DO_CAL_V <= 0.0) {
    return NAN;
  }

  int temperatureIndex = constrain((int)round(temperature), 0, 40);

  float saturationVoltage =
    DO_CAL_V + 35.0 * ((float)temperatureIndex - DO_CAL_T);

  if (saturationVoltage <= 0.0) {
    return NAN;
  }

  return voltageMv *
         (float)DO_TABLE[temperatureIndex] /
         saturationVoltage / 1000.0;
}

// ==================================================
// 탁도: 맑음도 계산
// 0% = 매우 탁함 / 100% = 정제수 수준으로 맑음
// ==================================================
float calculateCleanliness(float turbiditySensorVoltage) {
  float cleanliness =
    (turbiditySensorVoltage - VERY_TURBID_VOLTAGE) /
    (CLEAR_WATER_VOLTAGE - VERY_TURBID_VOLTAGE) * 100.0;

  return constrain(cleanliness, 0.0, 100.0);
}

const char* getCleanlinessLevel(float cleanlinessPercent) {
  if (cleanlinessPercent >= 80.0) {
    return "very_clear";
  }
  else if (cleanlinessPercent >= 60.0) {
    return "clear";
  }
  else if (cleanlinessPercent >= 40.0) {
    return "normal";
  }
  else if (cleanlinessPercent >= 20.0) {
    return "turbid";
  }

  return "very_turbid";
}

// JSON 숫자 또는 null 출력
void printJsonFloat(float value, int digits) {
  if (isnan(value)) {
    Serial.print("null");
  } else {
    Serial.print(value, digits);
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(ADC_BITS);

  delay(1000);

  temperatureSensorFound = findTemperatureSensor();
}

void loop() {
  // 수온
  float waterTemperature = readTemperatureC();

  // pH
  double phADC = readAverageADC(PH_PIN);
  float phVoltage = adcToVoltage(phADC);
  float phValue = calculatePH(phVoltage, waterTemperature);

  // DO
  double doADC = readAverageADC(DO_PIN);
  float doVoltageMv = adcToMillivolts(doADC);
  float doValue = calculateDO(doVoltageMv, waterTemperature);

  // 탁도
  double turbidityADC = readAverageADC(TURBIDITY_PIN);
  float turbidityA0Voltage = adcToVoltage(turbidityADC);

  // 전압분배기 전의 실제 AO 전압
  float turbiditySensorVoltage =
    turbidityA0Voltage * TURBIDITY_DIVIDER_RATIO;

  float cleanlinessPercent =
    calculateCleanliness(turbiditySensorVoltage);

  const char* cleanlinessLevel =
    getCleanlinessLevel(cleanlinessPercent);

  // ==================================================
  // ROS 2용: JSON 한 줄만 출력
  // ==================================================
  Serial.print("{\"ms\":");
  Serial.print(millis());

  Serial.print(",\"temp_c\":");
  printJsonFloat(waterTemperature, 2);

  Serial.print(",\"ph\":");
  printJsonFloat(phValue, 2);

  Serial.print(",\"do_mg_l\":");
  printJsonFloat(doValue, 2);

  Serial.print(",\"turbidity_voltage_v\":");
  printJsonFloat(turbiditySensorVoltage, 3);

  Serial.print(",\"clarity_pct\":");
  printJsonFloat(cleanlinessPercent, 1);

  Serial.print(",\"clarity_level\":\"");
  Serial.print(cleanlinessLevel);
  Serial.println("\"}");

  delay(1000);
}