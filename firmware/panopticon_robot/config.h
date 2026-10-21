#pragma once
#include <Arduino.h>


namespace Config {
constexpr char ROBOT_NAMESPACE[] = "robot_01";
constexpr char IMU_FRAME[] = "robot_01/imu_link";
constexpr uint16_t AGENT_PORT = 8888;
constexpr uint32_t COMMAND_TIMEOUT_MS = 400;

constexpr int LEFT_RPWM = 27, LEFT_LPWM = 14, LEFT_EN = 4;
constexpr int RIGHT_RPWM = 25, RIGHT_LPWM = 26, RIGHT_EN = 21;

constexpr int LEFT_A = 32, LEFT_B = 13;
constexpr int RIGHT_A = 34, RIGHT_B = 35;
// Sensuri inversate pentru montajul curent; encoderul se configureaza separat.
constexpr int LEFT_MOTOR_SIGN = -1, RIGHT_MOTOR_SIGN = -1;
constexpr int LEFT_ENCODER_SIGN = -1, RIGHT_ENCODER_SIGN = 1;
constexpr int SDA_PIN = 23, SCL_PIN = 22;

// Prima proba: roti ridicate. PWM limitat, fara PID / viteza masurata.
constexpr int MAX_PWM = 150;  // din 255; nu reprezinta limita de curent!
constexpr float TRACK_M = 0.35f;  // ecartament masurat: 35 cm
constexpr float WHEEL_SPEED_AT_MAX_PWM = 0.20f; // estimare de banc, m/s
constexpr float MAX_LINEAR = 0.15f;
constexpr float MAX_ANGULAR = 0.8f;

constexpr bool BATTERY_ENABLED = true;

// Corectie dupa citire
constexpr float BATTERY_OFFSET_V = -0.10f;
constexpr uint8_t INA_ADDRESS = 0x40, IMU_ADDRESS = 0x68;
}
