# Panopticon robot firmware

A small Arduino sketch for testing an ESP32 robot's connections and hardware with **ROS 2 Jazzy**. It supports Wi-Fi teleoperation and raw sensor topics. It does not implement navigation, odometry, or PID speed control.

`firmware` is the conventional name for software running on the microcontroller. `panopticon_robot` identifies the project and the software running on each robot; the name remains appropriate as features are added. Arduino requires the sketch directory and its main `.ino` file to share that name.

## Files

```text
panopticon_robot/
  panopticon_robot.ino   setup() and loop()
  config.h            pins, limits, and robot name
  secrets.example.h   placeholder Wi-Fi and agent settings
  secrets.h           local credentials, excluded from Git
  motors.h            PWM outputs and command timeout
  drive_math.h        differential command mixing
  encoders.h          encoder pulse counting
  i2c_bus.h           shared I2C reads/writes
  battery.h           INA226 bus voltage
  imu.h               MPU6500 / MPU9250 accelerometer and gyroscope
  ros_connection.h    Wi-Fi, micro-ROS connection, and topics
```

The sensor drivers use `Wire` directly to keep dependencies small. The external library is `micro_ros_arduino`; Wi-Fi and Wire come with the ESP32 Arduino core.

## Topics

| Topic | ROS 2 type | Meaning |
| --- | --- | --- |
| `/robot_01/cmd_vel` | `geometry_msgs/msg/Twist` | Input: `linear.x` and `angular.z` |
| `/robot_01/encoder_ticks` | `std_msgs/msg/Int64MultiArray` | Output at about 50 Hz: `[left, right]` signed counts |
| `/robot_01/imu/data_raw` | `sensor_msgs/msg/Imu` | Output at about 50 Hz: acceleration in m/s² and angular velocity in rad/s |
| `/robot_01/battery_voltage` | `std_msgs/msg/Float32` | Optional output at about 1 Hz: pack voltage in volts |
| `/robot_01/status` | `std_msgs/msg/String` | Diagnostic output every 2 seconds: command count, IMU identity/state/errors, battery enable/state, Wi-Fi RSSI |

Outputs use best-effort QoS. Encoder decoding is **x1**: one rising edge on A, direction from B. Counts reset on reboot and have no timestamp in this minimal message.

IMU values use the sensor's axes in `robot_01/imu_link`. Acceleration includes gravity. Orientation is unavailable (`orientation_covariance[0] = -1`); zero gyro/acceleration covariances mean unknown. There is no magnetometer/barometer output. The clock is synchronized with the agent at connection; timestamps represent read time. Long-session clock drift, bias calibration, and robot-frame transforms are future work.

Battery output is voltage only, with no percentage, current measurement, or automatic undervoltage shutdown. Missing or failed sensor reads do not produce fabricated zero measurements.

## Before powering the motors

- EN right is **GPIO21**, SDA **GPIO23**, and SCL **GPIO22**.
- Left encoder A now uses **GPIO32** in firmware. Battery-powered startup succeeded after disconnecting this signal from GPIO12. Move the physical wire to GPIO32 with power disconnected, and upload this updated sketch before testing the encoder. The wiring change and upload still need to be completed on the robot.
- Encoder signals must be compatible with **3.3 V**. GPIO34/35 have no internal pull-ups; use external 3.3 V pull-ups where the encoder output requires them.
- Left EN is GPIO4 in the current code; the originally reported wiring used GPIO16. Verify that the physical connection matches GPIO4. This is not a universal S3/C3/PSRAM-module pinout.
- Each configured EN must drive both R_EN and L_EN if your BTS module has both. Verify logic levels and add suitable hardware pull-downs to keep drivers disabled during reset.
- **Battery measurement is disabled by default.** Enable `BATTERY_ENABLED` only after the INA226 VBUS wiring is identified. Do not route motor current through an unverified R100 shunt. Logic VCC remains 3.3 V.
- Start with the **wheels raised** and a physical motor-power switch within reach. PWM limiting is not current protection or LiPo protection.

## 1. Configure and upload

Use this specific combination:

- Arduino IDE 2.x.
- Boards Manager: **esp32 by Espressif Systems 2.0.2**.
- Board: **ESP32 Dev Module**.
- Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)** for a board with 4 MB flash.
- Library: **micro_ros_arduino 2.0.8-jazzy**, installed using Sketch -> Include Library -> Add .ZIP Library: [official archive](https://github.com/micro-ROS/micro_ros_arduino/archive/refs/tags/v2.0.8-jazzy.zip).

The precompiled micro-ROS library targets this ESP32 core version. Do not switch directly to core 3.x; its PWM API also differs. [Supported boards](https://github.com/micro-ROS/micro_ros_arduino)

1. For a fresh checkout, copy `secrets.example.h` to `secrets.h`. The local workspace already has the supplied settings; do not overwrite them with the example.
2. Use a **2.4 GHz** Wi-Fi SSID and the host computer's LAN address in `AGENT_IP`, not a container address or `127.0.0.1`.
3. Review pins and limits in `config.h`.
4. Open `panopticon_robot/panopticon_robot.ino`, select the USB port, and upload.
5. Open Serial Monitor at **115200 baud** for sensor and connection status.

`secrets.h` is excluded from Git. Without it, the sketch compiles with placeholders but cannot connect to your network. A sensor missing at startup is reported and stays inactive until reboot.

## 2. Start the Docker agent

On Linux with Docker Engine and the Compose plugin installed, run these commands **from the repository root**:

```bash
docker compose -f docker/compose.yaml up -d agent
docker compose -f docker/compose.yaml logs -f agent
```

Ctrl+C exits the log viewer without stopping the background agent.

```text
ESP32 -> Wi-Fi -> host LAN IP:8888/UDP -> micro-ROS Agent -> ROS 2 Jazzy
```

The ESP32 connects to an agent, not directly to Docker. Docker packages the host-side dependencies. The Compose project is named `panopticon` and uses Linux host networking. It requires no privileged mode or USB access for Wi-Fi transport.

The host and ESP32 must be reachable on the LAN. Avoid guest-network client isolation; allow UDP 8888 from the robot's network. ROS traffic between different computers also requires DDS connectivity. Both services use **ROS_DOMAIN_ID=0**.

Both containers also use `ipc: host`. Fast DDS may select shared-memory transport when host networking is enabled; isolated IPC namespaces can allow topic discovery while preventing data delivery. After changing Compose settings, recreate the agent with `docker compose -f docker/compose.yaml up -d --force-recreate agent` and reopen any ROS tools containers. [Fast DDS Docker networking and shared memory](https://fast-dds.docs.eprosima.com/en/3.5.x/docker/shm_docker.html)

## 3. Start keyboard teleoperation

Build the ROS tools image once:

```bash
docker compose -f docker/compose.yaml build ros
```

Run teleop in an interactive terminal:

```bash
docker compose -f docker/compose.yaml run --rm ros \
  ros2 run teleop_twist_keyboard teleop_twist_keyboard \
  --ros-args -r cmd_vel:=/robot_01/cmd_vel -p speed:=0.1 -p turn:=0.5
```

The default message is `Twist`, matching the firmware. Keys: `i` forward, `,` backward, `j`/`l` rotate, `k` stop, Ctrl+C exit.

**Hold the movement key.** Standard keyboard teleop publishes on key events; the robot stops if no command arrives within 400 ms. Keyboard autorepeat's initial delay can cause a brief pause. Do not increase the timeout simply to keep the robot moving without new commands. [Keyboard teleop documentation](https://docs.ros.org/en/jazzy/p/teleop_twist_keyboard/)

## 4. Inspect the topics

Open another terminal at the repository root:

```bash
docker compose -f docker/compose.yaml run --rm ros
```

Inside that ROS shell, run commands one at a time; use Ctrl+C to end each echo:

```bash
ros2 topic list
ros2 topic echo /robot_01/encoder_ticks --qos-reliability best_effort
ros2 topic echo /robot_01/imu/data_raw --qos-reliability best_effort
ros2 topic echo /robot_01/battery_voltage --qos-reliability best_effort
```

The battery topic is declared but sends no samples while `BATTERY_ENABLED` is false or INA226 initialization fails.

To stop the agent and remove this project's containers, run from the host:

```bash
docker compose -f docker/compose.yaml down
```

## Why the motor code is simple

The sketch mixes forward and turning commands into left/right PWM. `TRACK_M` is set to the measured wheel track of **0.35 m** (center-to-center between the powered wheels). `WHEEL_SPEED_AT_MAX_PWM` remains a **bench placeholder** to replace with a measured value. PWM is capped at 150/255; the requested m/s and rad/s do not guarantee actual speed. This is enough to test motor directions and communication before adding encoder-based PID.

A separate local task checks command age about every 10 ms and disables outputs after 400 ms without a new command. This continues while ROS/network calls block. Wi-Fi/agent loss or a ROS error causes a stopped reboot and reconnection; encoder counts restart from zero. A new command can enable motion again.

Agent checks run once per second with a 200 ms ping timeout. A failed check stops the motors and pauses command processing; three consecutive failures trigger reconnection by reboot. A single delayed ping no longer restarts the board. The independent 400 ms command timeout remains unchanged.

For intermittent failures, capture Serial Monitor output at 115200 baud. Startup prints `reset_reason`; ROS failures print the operation, return code, and error text. Every two seconds, `ROS activ` reports cumulative received commands (`cmd`), successful encoder publish calls (`enc_tx`), Wi-Fi RSSI, and free heap. A successful publish call is not proof of delivery to a host subscriber. Repeated startup lines indicate resets; increasing `enc_tx` with a fixed `cmd` indicates no commands reaching the callback.

`Twist` has no timestamp, so timeout is measured from receipt and cannot detect every delayed packet. Disabling a driver may allow coasting. This is a supervised bench test, not a navigation or safety controller.

## Bench checklist

1. Motors stay off at boot and while waiting for the agent.
2. Forward commands turn both wheels forward; adjust `*_MOTOR_SIGN` if needed.
3. Forward wheel rotation increases counts; adjust `*_ENCODER_SIGN` separately.
4. At rest, acceleration magnitude is approximately 9.81 m/s² and gyro values are near zero.
5. Stop command traffic, stop the agent, and disconnect Wi-Fi in separate tests. Confirm motor shutdown. For command silence, expected software response is about 400 ms plus the task period.
6. After reconnection, confirm reset counts and no motion until a new command.
7. When voltage sensing is enabled, compare against a multimeter. Check LiPo cell protection separately.

## Without Docker

You can install ROS 2 Jazzy, micro-ROS Agent, and keyboard teleop directly on a compatible host. With that ROS environment sourced:

```bash
export ROS_DOMAIN_ID=0
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888
# In another terminal with the same ROS environment:
ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r cmd_vel:=/robot_01/cmd_vel
```

Docker is the initial recommendation because it keeps the Jazzy environment reproducible without installing the ROS stack on the host. Native Jazzy system requirements must match the host OS; the container does not require replacing the Jetson OS. GPU integration is outside this test's scope.

For a second robot, use a different `ROBOT_NAMESPACE` and `IMU_FRAME` in `config.h`.

## Software validation

The sketch compiles with ESP32 core 2.0.2 and micro-ROS Arduino 2.0.8-jazzy. The Docker Jazzy tools image builds on x86_64 and includes the teleop executable. Hardware behavior, ESP32-to-agent communication, and Jetson ARM64 operation still need device testing.

Run the command-mixing and timeout arithmetic test from the repository root:

```bash
g++ -std=c++11 -Wall -Wextra -Werror tests/drive_math_test.cpp -o /tmp/platform-drive-test
/tmp/platform-drive-test
```

Sensor register references: [INA226](https://www.ti.com/lit/ds/symlink/ina226.pdf), [MPU9250 register map](https://invensense.tdk.com/wp-content/uploads/2015/02/RM-MPU-9250A-00-v1.6.pdf).

## Motor and sensor troubleshooting

Left encoder polarity is set to -1 after forward manual rotation was reported as negative; right polarity remains +1. Both motor polarities are now -1: the right side was reversed after backward motion was reported for a forward command, and the left side was subsequently reversed at the user's request. Confirm physical forward motion after uploading. These settings apply to the configured driver channels, not to a motor's physical location if motor cables have been swapped. The current configuration uses GPIO4 for left EN; confirm the physical wire matches this setting and drives both enable inputs. Do not confuse motor polarity with encoder polarity.

With the current mapping, straight motion at `speed:=0.1` commands approximately PWM 75/255 per wheel; at `speed:=0.15`, approximately 112/255. This is open-loop duty cycle, not measured speed. Test with wheels raised. If one motor still does not move, verify its power, both R_EN/L_EN inputs, and direction wiring before increasing PWM further.

For sensor diagnosis on battery power:

```bash
ros2 topic echo /robot_01/status --qos-reliability best_effort
```

`imu_id_read=0` means the WHO_AM_I register could not be read at the configured address. The driver accepts MPU6500 (`0x70`) and MPU9250 (`0x71`) for the shared accelerometer/gyroscope register interface; other identities are rejected. The reported board returns `0x70`, matching MPU6500. This identity is not the I2C bus address: leave the working address at `0x68`. No magnetometer support is implied. [MPU6500 register map](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6500-Register-Map2.pdf)

`imu_ready=0` can also mean configuration writes failed. If `imu_ready=1` but `imu_errors` increases, runtime I2C reads are failing. Increasing `imu_tx` counts successful publish calls, not confirmed reception by subscribers. After uploading the updated sketch, expect `imu_ready=1` and increasing `imu_tx`; verify `/robot_01/imu/data_raw` before troubleshooting the dashboard.

`battery_enabled=0` is intentional until voltage-only wiring is verified. Do not enable battery readings as a substitute for correcting the original R100 motor-current path.

## Battery voltage offset

Battery measurement is enabled in the current configuration. `BATTERY_OFFSET_V` is set to **-0.10 V**, based on the reported 12.47 V reading versus a 12.37 V reference. The published value is `max(0, raw_voltage + offset)`. This is a single-point correction, not a verified calibration across the discharge range or a smoothing filter. Compare measurements at the same terminals and at the same time, preferably with the motors stopped. Recheck at another battery voltage; set the offset to zero to recover the uncorrected reading.

The PWM ceiling was raised from 120 to 150, increasing commanded duty cycle by 25% for the same input. Actual speed depends on load and is not guaranteed to rise by 25%. Motor directions, encoder signs, and the 400 ms command timeout are unchanged.
