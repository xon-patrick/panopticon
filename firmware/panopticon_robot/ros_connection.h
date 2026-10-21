#pragma once
#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <geometry_msgs/msg/twist.h>
#include <sensor_msgs/msg/imu.h>
#include <std_msgs/msg/int64_multi_array.h>
#include <std_msgs/msg/float32.h>
#include <std_msgs/msg/string.h>
#include "motors.h"
#include "encoders.h"
#include "battery.h"
#include "imu.h"

namespace Ros {
static rclc_support_t support;
static rcl_allocator_t allocator;
static rcl_node_t node;
static rclc_executor_t executor;
static rcl_subscription_t velocitySub;
static rcl_publisher_t encoderPub, batteryPub, imuPub;
static rcl_publisher_t statusPub;
static std_msgs__msg__String statusMsg;
static char statusBuffer[256];
static geometry_msgs__msg__Twist velocityMsg;
static std_msgs__msg__Int64MultiArray encoderMsg;
static std_msgs__msg__Float32 batteryMsg;
static sensor_msgs__msg__Imu imuMsg;
static int64_t ticks[2];
static bool imuReady = false, batteryReady = false;
static uint32_t lastTelemetry = 0, lastBattery = 0, lastPing = 0;
static uint32_t lastStatus = 0, receivedCommands = 0, publishedEncoders = 0;
static uint8_t missedPings = 0;
static uint32_t imuPublished = 0, imuReadErrors = 0;

// Reconectare simpla: restart sigur; contoarele encoderelor reincep de la zero.
inline void restart(const char *reason) {
  Motors::stop();
  Serial.println(reason);
  delay(100); // lasa task-ul motoarelor sa dezactiveze iesirile
  ESP.restart();
}
inline void check(rcl_ret_t result, const char *operation = "initializare ROS") {
  if (result == RCL_RET_OK) return;
  Motors::stop();
  Serial.printf("Eroare la %s: cod=%d, detalii=%s\n", operation,
                int(result), rcl_get_error_string().str);
  rcl_reset_error();
  restart("Eroare ROS; restart cu motoarele oprite.");
}
inline void onVelocity(const void *message) {
  const auto *cmd = static_cast<const geometry_msgs__msg__Twist *>(message);
  ++receivedCommands;
  Motors::command(cmd->linear.x, cmd->angular.z);
}
inline void begin() {
  // Motoarele sunt deja initializate/oprite cand se conecteaza Wi-Fi.
  set_microros_wifi_transports(WIFI_SSID, WIFI_PASSWORD, AGENT_IP, Config::AGENT_PORT);
  WiFi.setSleep(false);
  Serial.print("Wi-Fi IP: "); Serial.println(WiFi.localIP());
  Serial.println("Astept micro-ROS Agent...");
  while (rmw_uros_ping_agent(100, 1) != RMW_RET_OK) {
    if (WiFi.status() != WL_CONNECTED) restart("Wi-Fi pierdut.");
    delay(500);
  }
  allocator = rcl_get_default_allocator();
  check(rclc_support_init(&support, 0, nullptr, &allocator));
  check(rclc_node_init_default(&node, "panopticon_robot", Config::ROBOT_NAMESPACE, &support));

  // Ultima comanda, fara coada de comenzi vechi sau retransmisii reliable.
  auto options = rcl_subscription_get_default_options();
  options.qos = rmw_qos_profile_sensor_data;
  options.qos.depth = 1;
  check(rcl_subscription_init(&velocitySub, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist), "cmd_vel", &options));
  check(rclc_publisher_init_best_effort(&encoderPub, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int64MultiArray), "encoder_ticks"));
  check(rclc_publisher_init_best_effort(&batteryPub, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32), "battery_voltage"));
  check(rclc_publisher_init_best_effort(&imuPub, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu), "imu/data_raw"));
  check(rclc_publisher_init_best_effort(&statusPub, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String), "status"));
  statusMsg.data.data = statusBuffer;
  statusMsg.data.capacity = sizeof(statusBuffer);

  // Memorie fixa pentru mesaje: fara alocari repetate in loop.
  encoderMsg.data.data = ticks;
  encoderMsg.data.size = encoderMsg.data.capacity = 2;
  imuMsg.header.frame_id.data = const_cast<char *>(Config::IMU_FRAME);
  imuMsg.header.frame_id.size = sizeof(Config::IMU_FRAME) - 1;
  imuMsg.header.frame_id.capacity = sizeof(Config::IMU_FRAME);
  imuMsg.orientation_covariance[0] = -1; // nu estimam orientarea
  imuMsg.orientation.w = 1.0;
  // Covariante gyro/accel zero = necunoscute, nu precizie perfecta.
  executor = rclc_executor_get_zero_initialized_executor();
  check(rclc_executor_init(&executor, &support.context, 1, &allocator));
  check(rclc_executor_add_subscription(&executor, &velocitySub, &velocityMsg, onVelocity, ON_NEW_DATA));
  if (rmw_uros_sync_session(1000) != RMW_RET_OK)
    restart("Sincronizarea ceasului a esuat.");
  Serial.println("ROS conectat. Teleop pregatit; timeout comenzi 400 ms.");
  lastPing = millis();
}
inline void update() {
  if (WiFi.status() != WL_CONNECTED) restart("Wi-Fi pierdut.");
  const uint32_t now = millis();
  if (now - lastPing >= 1000) {
    lastPing = now;
    if (rmw_uros_ping_agent(200, 1) != RMW_RET_OK) {
      Motors::stop();
      Serial.printf("Ping agent ratat: %u/3\n", ++missedPings);
      if (missedPings >= 3) restart("Agent pierdut dupa 3 verificari.");
    } else {
      missedPings = 0;
    }
  }
  // Nu prelua comenzi pana cand agentul raspunde din nou.
  if (missedPings != 0) return;
  const rcl_ret_t spinResult = rclc_executor_spin_some(&executor, RCL_MS_TO_NS(2));
  if (spinResult != RCL_RET_TIMEOUT) check(spinResult, "executor"); // fara comenzi = normal
  if (now - lastTelemetry >= 20) {
    lastTelemetry = now;
    Encoders::read(ticks[0], ticks[1]);
    check(rcl_publish(&encoderPub, &encoderMsg, nullptr), "publicare encodere");
    ++publishedEncoders;
    float accel[3], gyro[3];
    if (imuReady && Imu::read(accel, gyro)) {
      const int64_t stamp = rmw_uros_epoch_nanos();
      imuMsg.header.stamp.sec = stamp / 1000000000LL;
      imuMsg.header.stamp.nanosec = stamp % 1000000000LL;
      imuMsg.linear_acceleration.x = accel[0];
      imuMsg.linear_acceleration.y = accel[1];
      imuMsg.linear_acceleration.z = accel[2];
      imuMsg.angular_velocity.x = gyro[0];
      imuMsg.angular_velocity.y = gyro[1];
      imuMsg.angular_velocity.z = gyro[2];
      check(rcl_publish(&imuPub, &imuMsg, nullptr), "publicare IMU");
      ++imuPublished;
    } else if (imuReady) {
      ++imuReadErrors;
    }
  }
  if (now - lastBattery >= 1000) {
    lastBattery = now;
    if (batteryReady && Battery::read(batteryMsg.data))
      check(rcl_publish(&batteryPub, &batteryMsg, nullptr), "publicare baterie");
  }
  if (now - lastStatus >= 2000) {
    lastStatus = now;
    Serial.printf("ROS activ: cmd=%lu, enc_tx=%lu, RSSI=%d dBm, heap=%u\n",
      (unsigned long)receivedCommands, (unsigned long)publishedEncoders,
      WiFi.RSSI(), ESP.getFreeHeap());
    // Diagnostic prin Wi-Fi, util cand placa este alimentata de la baterie.
    snprintf(statusBuffer, sizeof(statusBuffer),
      "cmd=%lu imu_ready=%d imu_id_read=%d imu_id=0x%02X imu_tx=%lu imu_errors=%lu battery_enabled=%d battery_ready=%d rssi=%d",
      (unsigned long)receivedCommands, int(imuReady), int(Imu::idRead), Imu::deviceId,
      (unsigned long)imuPublished, (unsigned long)imuReadErrors,
      int(Config::BATTERY_ENABLED), int(batteryReady), WiFi.RSSI());
    statusMsg.data.size = strlen(statusBuffer);
    check(rcl_publish(&statusPub, &statusMsg, nullptr), "publicare status");
  }
}
}
