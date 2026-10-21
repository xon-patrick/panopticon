# Panopticon

A low-cost mobile robot fleet with ESP32 controllers, centralized ROS 2 Jazzy processing, and a long-term goal of localization through fixed cameras.

## Current status

The repository contains a **bench-test firmware**, not autonomous navigation:

- Wi-Fi connection to a micro-ROS Agent.
- Keyboard teleoperation through `cmd_vel`.
- Encoder counts, raw accelerometer/gyroscope data, and optional battery voltage.
- Local motor shutdown when commands stop arriving.

Start with the [firmware guide](firmware/README.md) and [panopticon_robot.ino](firmware/panopticon_robot/panopticon_robot.ino). The sketch has compiled for a classic ESP32 with micro-ROS Jazzy. Hardware behavior still requires testing on the actual platform.

## Local robot console

A local browser interface shows robot connectivity, live topic messages, IMU plots, relative orientation, encoder direction/speed, and hold-to-drive teleoperation. See the [dashboard guide](dashboard/README.md) for startup, calibration, and control behavior.

```bash
docker compose -f docker/compose.yaml up -d --build agent dashboard
```

Open [localhost:8080](http://localhost:8080) on the Docker host.

## Hardware

| Part | Purpose and assessment |
| --- | --- |
| Six wheels: two powered center wheels, four passive wheels | Suitable for mechanical experiments. Fixed-axis passive wheels resist sideways motion during turns; test turning under load. Swiveling support wheels may simplify an initial design. |
| Two JGB37-520 geared DC motors with encoders | Exact voltage, gear ratio, speed, stall current, and encoder resolution must be identified. The family name alone is insufficient. |
| Two BTS7960 driver modules | One module per motor. The advertised 43 A is not a guaranteed continuous rating for the assembled module. Verify logic compatibility, wiring, and cooling. |
| Classic ESP32 with Wi-Fi | Handles sensors, motor outputs, communication, and local command timeout. Later, it should also run wheel-speed PID control. |
| MPU9250 + BMP280 module | Gyroscope and accelerometer are used in the test. Magnetometer and barometer are not needed initially. Accelerometer integration alone does not provide stable position. |
| INA226 | Optional battery-voltage measurement for this test. No current measurement or battery percentage is implemented. |
| HRB LiPo 3S, 11.1 V, 6000 mAh, XT60 | A standard 4.2 V/cell pack reaches 12.6 V fully charged. Nominal energy is about 66.6 Wh; runtime must be measured. |
| XY-3606 buck converter, 5 V output | Supplies electronics. Verify output voltage, real continuous capability, temperature, and voltage dips during motor startup. |
| Mini Blade holder and 10 A fuse | Size protection for wiring, connectors, and actual motor currents. The holder's 30 A label does not determine the fuse rating. |
| PETG chassis/rims and TPU tires | Useful for prototyping. Check load, deformation, temperature, and turning resistance from high-grip tires. |

## Electrical work before driving

The reported right motor EN connection has been moved to **GPIO21**, leaving GPIO22 for I2C SCL. Other proposed changes are not assumed to have been completed.

### Power distribution

The originally reported power path was:

```text
LiPo + -> fuse -> switch -> INA226 shunt -> positive distribution
                                           +-> left BTS B+
                                           +-> right BTS B+
                                           +-> buck converter -> 5 V electronics
LiPo - -> common ground distribution
```

**Do not run the motors through an unverified R100 shunt.** R100 usually means 0.1 ohm. With the INA226 shunt measurement range of approximately +/-81.92 mV, this gives only about +/-0.8192 A of measurement range. At 10 A, that resistor would dissipate 10 W and drop 1 V. Software calibration cannot increase the physical voltage range. [INA226 datasheet](https://www.ti.com/lit/ds/symlink/ina226.pdf)

For the current voltage-only goal, the proposed arrangement is a properly rated power path directly from the switch to the positive distribution, with INA226 VBUS sensing battery voltage. **The exact VBUS connection depends on the breakout board and has not been verified.** The chip's logic supply remains 3.3 V. Keep `BATTERY_ENABLED` false until the board wiring is identified.

If current measurement is added later, choose a suitably rated shunt and power path. As a calculation example, 5 milliohms gives about +/-16.38 A of measurement range and dissipates 0.5 W at 10 A. That does not establish the safe current rating of a board or shunt assembly.

### Required checks

- Encoder outputs must be compatible with 3.3 V GPIO even if the encoders are powered at 5 V. Use suitable level conversion for 5 V push-pull outputs, or external 3.3 V pull-ups for compatible open-collector outputs.
- Connect 5 V only to a development-board supply input designed for it, never to the ESP32 3V3 pin. Check simultaneous USB and external power behavior.
- Verify both BTS `R_EN` and `L_EN` connections and 3.3 V command compatibility. Hardware pull-downs should keep drivers disabled during reset/boot.
- Keep motor-current returns out of sensor/ESP32 ground wiring. Use suitable power wiring, local decoupling, and check supply transients during startup and braking.
- Put the fuse near the battery and provide an accessible physical motor-power cutoff.
- Use suitable LiPo charging/balancing and per-cell undervoltage protection. Total pack voltage does not reveal every individual cell condition.

Disabling a driver may allow the robot to coast. Measure actual stopping distance. The BTS7960 chip specification does not guarantee the continuous current capability of a commercial module. [Infineon datasheet](https://www.infineon.com/dgdl/BTS7960_Datasheet.pdf?fileId=db3a304412b407950112b43945006d5d&folderId=db3a304412b407950112b408e8c90004)

## Current firmware pinout

This targets a **classic ESP32** with these pins available, not an ESP32-S3/C3 or a module reserving these pins for other hardware.

| Signal | GPIO |
| --- | --- |
| Left RPWM / LPWM / EN | 27 / 14 / 4 |
| Right RPWM / LPWM / EN | 25 / 26 / 21 |
| Left encoder A / B | 32 / 13 |
| Right encoder A / B | 34 / 35 |
| I2C SDA / SCL | 23 / 22 |

The robot connected immediately on battery power after the left encoder wire was disconnected from GPIO12, strongly implicating that boot strapping pin. Firmware now uses **GPIO32** for left encoder A: move the physical wire to GPIO32 and upload the updated sketch together. The physical move and upload have not yet been confirmed. GPIO34/35 are input-only and have no internal pull-ups. GPIO16 may be unavailable on some modules with PSRAM. [ESP32 documentation](https://documentation.espressif.com/esp32_datasheet_en.html)

Expected I2C addresses are INA226 `0x40`, MPU9250 `0x68`, and BMP280 `0x76`. Pull-ups must go to 3.3 V. Confirm the combined sensor board's pin labels: MPU nCS high and AD0 low select the intended I2C arrangement; BMP CSB high and SDO low select its intended arrangement. The firmware checks device identity for the INA226 and MPU9250 and does not use BMP280. [MPU9250 specification](https://invensense.tdk.com/wp-content/uploads/2015/02/PS-MPU-9250A-01-v1.1.pdf), [BMP280 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf)

## Planned architecture

```text
Fixed cameras -> calibration, detection, tracking -> metric poses and obstacles
                                                        |
Jetson Orin Nano Super: ROS 2 Jazzy, estimation, Nav2, fleet coordination
                                                        |
                                      micro-ROS Agent, UDP / Wi-Fi
                                                        |
                         ESP32: sensors, local motor control, command timeout
```

The ESP32 needs local control and shutdown behavior even when most computation is centralized. Wi-Fi latency is variable. The current test maps commands to limited PWM; closed-loop wheel-speed PID belongs in the next stage after motor and encoder calibration.

The Jetson is a reasonable prototype host, but supported camera/robot counts must be established from measured latency, memory use, and processing load. TOPS alone does not specify fleet capacity. [NVIDIA platform](https://www.nvidia.com/en-eu/autonomous-machines/embedded-systems/jetson-orin/nano-super-developer-kit/)

Nav2 should plan and produce velocity commands on the host. Separate fleet coordination must manage shared corridors, priorities, and deadlocks. Each robot needs its own namespace and unique TF frames, for example:

```text
map -> robot_01/odom -> robot_01/base_link -> robot_01/imu_link
```

A topic namespace does not automatically prefix TF frame IDs. Local odometry supplies `odom -> base_link`; global localization supplies `map -> odom`, with one publisher per transform. The current firmware supplies neither odometry nor TF. [Nav2 transform requirements](https://docs.nav2.org/rolling/configuration_and_development/first_time_robot_setup_guide/transformation/setup_transforms/)

## Camera navigation: realistic first target

Start with one robot, one calibrated camera, a known-size ArUco/AprilTag marker, and a manually defined or assisted floor map in a controlled area. Estimate metric position and heading, fuse with wheel/IMU information, and convert obstacle observations into a metric costmap.

A floor homography applies to points on the floor plane. A marker on top of the robot is above that plane; its height and marker-to-robot transform must be accounted for. Image detections alone are not metric obstacle footprints. [OpenCV homography explanation](https://docs.opencv.org/4.5.2/d9/dab/tutorial_homography.html)

Existing warehouse cameras may have blind spots, compression delay, poor angles, or insufficient resolution. Stop when required observations become stale or uncertain; an unseen area is not automatically free space. Local bump/proximity sensing is recommended before broader operation. Initial trials should remain supervised in a separated test area.

Automatic mapping and reliable operation in an arbitrary warehouse with arbitrary existing cameras remain research goals, not standard Nav2 functionality.

## Development path

| Stage | Deliverable | Completion check |
| --- | --- | --- |
| 1. Electrical validation | Verified supplies, logic levels, pinout, and protection | No pin conflict or unverified high-current shunt path |
| 2. Current bench test | Wi-Fi teleop and raw telemetry | Correct motor/encoder directions and shutdown on command/network loss |
| 3. Local speed control | Measured motor/encoder parameters and PID | Repeatable wheel speeds under load |
| 4. Odometry | Timed wheel data, geometry, estimator, TF | Measured straight-line and turning errors |
| 5. Camera localization | Calibrated camera and robot marker | Measured pose error/latency and stop on lost observations |
| 6. Single-robot Nav2 | Initial map, costmap, goal following | Repeatable controlled obstacle and stopping tests |
| 7. Two robots | Separate identities and corridor reservations | Controlled intersection handling and failure behavior |
| 8. Expansion | More robots/cameras and assisted mapping | Measured latency budget and blind-spot handling |

Before navigation tests, define maximum speed, acceptable pose error, observation age, and stopping distance. Include network/perception delay, measured braking, and uncertainty margins.

Still needed: exact ESP32/module variants, sensor-board wiring, motor/encoder specifications, loaded mass, wheel dimensions, track width, passive-wheel geometry, camera access/placement, and target operating area.
