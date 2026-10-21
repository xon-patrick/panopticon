#include "config.h"
#include <esp_system.h>
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#include "motors.h"
#include "encoders.h"
#include "i2c_bus.h"
#include "battery.h"
#include "imu.h"
#include "ros_connection.h"

void setup() {
  Serial.begin(115200);
  Serial.printf("Pornire ESP32: reset_reason=%d\n", int(esp_reset_reason()));
  if (!Motors::begin()) {
    Serial.println("Task motoare indisponibil; iesirile raman dezactivate.");
    while (true) delay(1000);
  }
  Encoders::begin();
  Bus::begin();
  Ros::imuReady = Imu::begin();
  Ros::batteryReady = Battery::begin();
  Serial.println(Ros::imuReady ? "IMU accel/gyro OK" : "IMU absent / ID neacceptat / configurare esuata");
  Serial.println(Ros::batteryReady ? "INA226 OK" : "Baterie dezactivata / INA226 absent");
  Ros::begin();
}

void loop() {
  Ros::update();
  delay(1);
}
