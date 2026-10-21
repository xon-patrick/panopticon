#pragma once
#include "i2c_bus.h"

namespace Battery {
inline bool begin() {
  if (!Config::BATTERY_ENABLED) return false;
  uint16_t manufacturer, chip;
  return Bus::read16(Config::INA_ADDRESS, 0xFE, manufacturer) && manufacturer == 0x5449 &&
         Bus::read16(Config::INA_ADDRESS, 0xFF, chip) && (chip >> 4) == 0x226;
}
inline bool read(float &voltage) {
  uint16_t raw;
  if (!Config::BATTERY_ENABLED || !Bus::read16(Config::INA_ADDRESS, 0x02, raw)) return false;
  voltage = raw * 0.00125f; // VBUS: 1,25 mV/LSB; nu necesita calibrare de shunt.
  voltage += Config::BATTERY_OFFSET_V;
  if (voltage < 0.0f) voltage = 0.0f;
  return true;
}
}
