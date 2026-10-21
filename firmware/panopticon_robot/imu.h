#pragma once
#include "i2c_bus.h"

namespace Imu {
static uint8_t deviceId = 0xFF;
static bool idRead = false;
inline bool begin() {
  idRead = Bus::read(Config::IMU_ADDRESS, 0x75, &deviceId, 1);
  Serial.printf("IMU: adresa=0x%02X, citire_ID=%d, WHO_AM_I=0x%02X\n",
                Config::IMU_ADDRESS, int(idRead), deviceId);
  // MPU6500 (0x70) si MPU9250 (0x71): aceleasi registre accel/gyro folosite aici.
  // Identificatorul cipului este in config.h.
  if (!idRead || (deviceId != 0x70 && deviceId != 0x71)) return false;
  if (!Bus::write8(Config::IMU_ADDRESS, 0x6B, 0x01)) return false;
  delay(100);
  return Bus::write8(Config::IMU_ADDRESS, 0x1B, 0x00) && // gyro +/-250 deg/s
         Bus::write8(Config::IMU_ADDRESS, 0x1C, 0x00) && // accel +/-2 g
         Bus::write8(Config::IMU_ADDRESS, 0x1A, 0x03) &&
         Bus::write8(Config::IMU_ADDRESS, 0x1D, 0x03) &&
         Bus::write8(Config::IMU_ADDRESS, 0x19, 19); // 50 Hz
}
inline bool read(float accel[3], float gyro[3]) {
  uint8_t bytes[14];
  if (!Bus::read(Config::IMU_ADDRESS, 0x3B, bytes, 14)) return false;
  for (int i = 0; i < 3; ++i) {
    const int a = i * 2, g = 8 + i * 2;
    const int16_t rawA = int16_t((uint16_t(bytes[a]) << 8) | bytes[a + 1]);
    const int16_t rawG = int16_t((uint16_t(bytes[g]) << 8) | bytes[g + 1]);
    accel[i] = rawA * (9.80665f / 16384.0f);
    gyro[i] = rawG * (0.01745329252f / 131.0f);
  }
  return true;
}
}
