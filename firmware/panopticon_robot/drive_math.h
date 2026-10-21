#pragma once
#include <math.h>
#include <stdint.h>

namespace DriveMath {
inline float limit(float value, float maxValue) {
  return fmaxf(-maxValue, fminf(value, maxValue));
}
inline bool fresh(uint32_t now, uint32_t received, uint32_t timeout) {
  return uint32_t(now - received) < timeout;
}
inline void mix(float v, float w, float track, float fullSpeed,
                float &left, float &right) {
  left = (v - w * track / 2.0f) / fullSpeed;
  right = (v + w * track / 2.0f) / fullSpeed;
  const float scale = fmaxf(1.0f, fmaxf(fabsf(left), fabsf(right)));
  left /= scale;
  right /= scale;
}
}
