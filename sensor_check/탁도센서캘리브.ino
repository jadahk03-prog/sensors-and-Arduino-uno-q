#define SENSOR_PIN A0
#define CLEAR_WATER_VOLTAGE 4.81  // 맑은 물일 때의 평균 전압을 4.81V로 설정

// 안정적인 측정을 위한 샘플링 변수
const int numReadings = 10;
int readings[numReadings];
int readIndex = 0;
long total = 0;
float averageVoltage = 0;

void setup() {
  Serial.begin(9600);
  // 배열 초기화
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
}

void loop() {
  // 이동 평균 필터 (노이즈 제거)
  total = total - readings[readIndex];
  readings[readIndex] = analogRead(SENSOR_PIN);
  total = total + readings[readIndex];
  readIndex = readIndex + 1;

  if (readIndex >= numReadings) {
    readIndex = 0;
  }

  // 평균 전압 계산
  float averageRaw = (float)total / numReadings;
  averageVoltage = averageRaw * (5.0 / 1023.0);

  // 현재 전압이 설정한 맑은 물 전압보다 높으면 맑은 물 전압으로 고정
  if (averageVoltage > CLEAR_WATER_VOLTAGE) {
    averageVoltage = CLEAR_WATER_VOLTAGE;
  }

  // 4.81V 기준 변환 수식 (Keystudio 센서 특성 곡선 이동 보정)
  // 전압이 4.81V에 가까울수록 0 NTU가 되며, 전압이 낮아질수록 NTU가 올라갑니다.
  float ntu = 0.0;
  if (averageVoltage >= CLEAR_WATER_VOLTAGE) {
    ntu = 0.0;
  } else {
    // 현재 센서 전압 범위를 공식 규격에 맞게 맵핑 변환 후 수식 적용
    float v_mapped = averageVoltage * (4.20 / CLEAR_WATER_VOLTAGE); 
    ntu = -1120.4 * (v_mapped * v_mapped) + 5742.3 * v_mapped - 4352.9;
  }

  // 예외 처리 (음수 방지 및 최대값 제한)
  if (ntu < 0) ntu = 0;
  if (ntu > 3000) ntu = 3000; 

  // 결과 출력
  Serial.print("Filtered Voltage: ");
  Serial.print(averageVoltage, 3);
  Serial.print(" V | Turbidity: ");
  Serial.print(ntu, 1);
  Serial.println(" NTU");

  delay(200); // 필터가 부드럽게 작동하도록 측정 간격을 줄였습니다.
}
