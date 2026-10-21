#pragma once
#include <Wire.h>
#include "config.h"

namespace Bus {
inline void begin() {
  Wire.begin(Config::SDA_PIN, Config::SCL_PIN);
  Wire.setClock(100000);
  Wire.setTimeOut(20);
}
inline bool write8(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address); Wire.write(reg); Wire.write(value);
  return Wire.endTransmission() == 0;
}
inline bool read(uint8_t address, uint8_t reg, uint8_t *bytes, uint8_t count) {
  Wire.beginTransmission(address); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, count) != count) return false;
  for (uint8_t i = 0; i < count; ++i) bytes[i] = Wire.read();
  return true;
}
inline bool read16(uint8_t address, uint8_t reg, uint16_t &value) {
  uint8_t bytes[2];
  if (!read(address, reg, bytes, 2)) return false;
  value = (uint16_t(bytes[0]) << 8) | bytes[1];
  return true;
}
}
