#pragma once
#include "config.h"

namespace Encoders {
static portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
static volatile int64_t left = 0, right = 0;
// Decodare x1: un impuls la fiecare front crescator pe A, sens din B.
static void IRAM_ATTR leftTick() {
  const int step = digitalRead(Config::LEFT_B) ? -1 : 1;
  portENTER_CRITICAL_ISR(&mutex);
  left += step * Config::LEFT_ENCODER_SIGN;
  portEXIT_CRITICAL_ISR(&mutex);
}
static void IRAM_ATTR rightTick() {
  const int step = digitalRead(Config::RIGHT_B) ? -1 : 1;
  portENTER_CRITICAL_ISR(&mutex);
  right += step * Config::RIGHT_ENCODER_SIGN;
  portEXIT_CRITICAL_ISR(&mutex);
}
inline void begin() {
  for (int pin : {Config::LEFT_A, Config::LEFT_B, Config::RIGHT_A, Config::RIGHT_B})
    pinMode(pin, INPUT); // Pull-up extern la 3,3 V daca iesirea il necesita.
  attachInterrupt(digitalPinToInterrupt(Config::LEFT_A), leftTick, RISING);
  attachInterrupt(digitalPinToInterrupt(Config::RIGHT_A), rightTick, RISING);
}
inline void read(int64_t &l, int64_t &r) {
  portENTER_CRITICAL(&mutex);
  l = left; r = right;
  portEXIT_CRITICAL(&mutex);
}
}
