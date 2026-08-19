#include <Arduino.h>

// =====================================
// Keystudio Turbidity Sensor V1.0
// Arduino UNO Q 탁도 측정 코드
// 전압이 높을수록 물이 맑음
// =====================================

#define TURBIDITY_PIN A0

// UNO Q ADC 설정
#define ADC_BITS 12
#define ADC_MAX 4095.0
#define ADC_REF_V 3.3

// 전압분배기:
// 센서 AO -- 10kΩ -- A0 -- 20kΩ -- GND
// 실제 센서 AO 전압 = A0에서 읽은 전압 × 1.5
#define VOLTAGE_DIVIDER_RATIO 1.5

#define SAMPLE_COUNT 50

// 네 캘리브레이션 기준값
#define CLEAR_WATER_VOLTAGE 4.810   // 정제수: 맑음도 100%
#define VERY_TURBID_VOLTAGE 0.140   // 매우 탁한 물: 맑음도 0%

float readFilteredADC() {
  long sum = 0;
  int samples[SAMPLE_COUNT];

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    samples[i] = analogRead(TURBIDITY_PIN);
    sum += samples[i];
    delay(10);
  }

  int minValue = samples[0];
  int maxValue = samples[0];

  for (int i = 1; i < SAMPLE_COUNT; i++) {
    if (samples[i] < minValue) minValue = samples[i];
    if (samples[i] > maxValue) maxValue = samples[i];
  }

  // 최솟값/최댓값 하나씩 제외
  sum -= minValue;
  sum -= maxValue;

  return sum / float(SAMPLE_COUNT - 2);
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(ADC_BITS);

  Serial.println();
  Serial.println("=== 탁도 센서 측정 시작 ===");
  Serial.println("전압이 높을수록 맑은 물입니다.");
  Serial.println();
}

void loop() {
  // ADC 평균값
  float filteredADC = readFilteredADC();

  // UNO Q A0 핀에서 실제 측정한 전압
  float voltageAtA0 = filteredADC * ADC_REF_V / ADC_MAX;

  // 전압분배기 전, 센서 AO의 실제 전압
  float sensorVoltage = voltageAtA0 * VOLTAGE_DIVIDER_RATIO;

  // 맑음도 계산
  // 0%   = VERY_TURBID_VOLTAGE
  // 100% = CLEAR_WATER_VOLTAGE
  float cleanlinessPercent =
    (sensorVoltage - VERY_TURBID_VOLTAGE) /
    (CLEAR_WATER_VOLTAGE - VERY_TURBID_VOLTAGE) * 100.0;

  cleanlinessPercent = constrain(cleanlinessPercent, 0.0, 100.0);

  Serial.print("ADC: ");
  Serial.print(filteredADC, 1);

  Serial.print(" | 센서 전압: ");
  Serial.print(sensorVoltage, 3);
  Serial.print(" V");

  Serial.print(" | 맑음도: ");
  Serial.print(cleanlinessPercent, 1);
  Serial.print(" %");

  // 맑음도 5단계 판정
  if (cleanlinessPercent >= 80.0) {
    Serial.println(" | 상태: 매우 맑음");
  }
  else if (cleanlinessPercent >= 60.0) {
    Serial.println(" | 상태: 맑음");
  }
  else if (cleanlinessPercent >= 40.0) {
    Serial.println(" | 상태: 보통");
  }
  else if (cleanlinessPercent >= 20.0) {
    Serial.println(" | 상태: 탁함");
  }
  else {
    Serial.println(" | 상태: 매우 탁함");
  }

  delay(1000);
}