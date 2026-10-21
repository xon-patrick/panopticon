#pragma once
#include "config.h"
#include "drive_math.h"

namespace Motors {
static portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
static float requestedV = 0, requestedW = 0;
static uint32_t receivedAt = 0;
static bool armed = false;

inline void stop() {
  portENTER_CRITICAL(&mutex);
  armed = false;
  requestedV = requestedW = 0;
  portEXIT_CRITICAL(&mutex);
}
inline void command(float v, float w) {
  if (!isfinite(v) || !isfinite(w)) { stop(); return; }
  portENTER_CRITICAL(&mutex);
  requestedV = DriveMath::limit(v, Config::MAX_LINEAR);
  requestedW = DriveMath::limit(w, Config::MAX_ANGULAR);
  receivedAt = millis();
  armed = true;
  portEXIT_CRITICAL(&mutex);
}
inline void writeMotor(int channel, int enable, float value, int sign) {
  const int pwm = int(value * sign * Config::MAX_PWM);
  // Opreste ambele sensuri inainte de schimbare.
  ledcWrite(channel, 0);
  ledcWrite(channel + 1, 0);
  digitalWrite(enable, pwm != 0 ? HIGH : LOW);
  if (pwm > 0) ledcWrite(channel, pwm);
  if (pwm < 0) ledcWrite(channel + 1, -pwm);
}
// Task independent: timeout-ul functioneaza si daca ROS/Wi-Fi se blocheaza.
inline void task(void *) {
  for (;;) {
    float v, w;
    portENTER_CRITICAL(&mutex);
    if (!armed || !DriveMath::fresh(millis(), receivedAt, Config::COMMAND_TIMEOUT_MS)) {
      armed = false;
      requestedV = requestedW = 0;
    }
    v = requestedV; w = requestedW;
    portEXIT_CRITICAL(&mutex);
    float left, right;
    DriveMath::mix(v, w, Config::TRACK_M, Config::WHEEL_SPEED_AT_MAX_PWM, left, right);
    writeMotor(0, Config::LEFT_EN, left, Config::LEFT_MOTOR_SIGN);
    writeMotor(2, Config::RIGHT_EN, right, Config::RIGHT_MOTOR_SIGN);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
inline bool begin() {
  pinMode(Config::LEFT_EN, OUTPUT); digitalWrite(Config::LEFT_EN, LOW);
  pinMode(Config::RIGHT_EN, OUTPUT); digitalWrite(Config::RIGHT_EN, LOW);
  const int pins[] = {Config::LEFT_RPWM, Config::LEFT_LPWM, Config::RIGHT_RPWM, Config::RIGHT_LPWM};
  for (int i = 0; i < 4; ++i) {
    ledcSetup(i, 20000, 8);
    ledcAttachPin(pins[i], i);
    ledcWrite(i, 0);
  }
  return xTaskCreate(task, "motor_watchdog", 2048, nullptr, 2, nullptr) == pdPASS;
}
}
