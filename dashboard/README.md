# Robot console

This web interface for robot testing and live status visualization was developed with AI coding agents, guided by project-specific requirements and iterative review.

Local browser dashboard for the existing ROS 2 Jazzy bench-test firmware. The planned visual palette is sunny orange and mint green on a dark background. It discovers robot namespaces and subscribes to `encoder_ticks`, `imu/data_raw`, `battery_voltage`, and `status` with best-effort sensor QoS. Expand a topic to inspect its latest message; the ROS catalog also lists other discovered topic names and types (it does not subscribe to arbitrary types).

## Run

From the repository root:

```bash
docker compose -f docker/compose.yaml up -d --build agent dashboard
```

Open [the console](http://localhost:8080) on the Docker host. Linux host networking is required, as in the existing agent setup. The server binds only to loopback. For a remote Jetson, forward it over SSH:

```bash
ssh -L 8080:127.0.0.1:8080 user@jetson
```

Then open the same localhost URL on your computer. Do not expose the service directly to a network; it is a local console, without user accounts or TLS. Host validation and a per-process control token protect local command requests from unrelated browser origins.

With ROS 2 Jazzy installed locally:

```bash
source /opt/ros/jazzy/setup.bash
python3 dashboard/server.py
```

To inspect the UI without ROS (no simulated telemetry and no motor control):

```bash
python3 dashboard/server.py --preview
```

## Telemetry

- Robot connectivity requires encoder messages received within one second, not just presence in the ROS graph.
- Each topic shows receipt age/frequency. IMU/encoder data expires after one second, battery after three seconds, status after five seconds. Rates use host receipt times, so network jitter affects them.
- Encoder counts are `[left, right]`. Tick rates are computed from consecutive messages. The wheel diameter defaults to the confirmed 100 mm (0.100 m). The measured powered-wheel track is 350 mm (0.35 m), configured as `TRACK_M` in the firmware. Enter published ticks per wheel revolution to obtain signed linear speed; the diameter can be adjusted if needed. Calibration is saved in this browser per namespace. Account for gearbox ratio and the firmware's encoder counting mode. No encoder resolution is assumed.
- Direction assumes the configured encoder signs correspond to forward wheel travel. Unequal resolution or wheel diameter is not supported in this simple view.
- Yaw integrates gyro Z from receipt intervals; it is relative, drifts, and is not an absolute heading. Reset its reference in the UI. No orientation quaternion is claimed: the firmware marks orientation unavailable. Roll/pitch come from gravity and are only approximate while stationary. These are separate visual estimates, not fused odometry or navigation.
- Charts show the last 100 UI samples (roughly 20 seconds at 5 Hz); they are not lossless recordings of the 50 Hz ROS stream. Missing IMU data produces gaps.
- Missing battery telemetry is expected when `BATTERY_ENABLED` is false.

## Teleoperation

Select a connected robot, enable control, then hold a direction button or W/A/S/D or an arrow key. Linear and angular commands are capped at the firmware limits of 0.15 m/s and 0.8 rad/s. The current firmware maps these to open-loop PWM; actual speed is not regulated.

Release, Space, Escape, loss of focus, hiding the tab, or switching robots stops control. Space/Escape and switching robots also disarm it. Commands repeat at 10 Hz while held. The server expires a command lease after 250 ms, checked every 50 ms, and sends zero; the firmware independently stops after 400 ms without commands. Browser/network stalls therefore cannot maintain a command indefinitely. One browser client at a time can hold the console's lease. This does not arbitrate other ROS publishers: stop command-line teleop before using the console. The UI STOP is a software stop, not a physical emergency cutoff.

Test with wheels raised first, including release, tab loss, network loss, and firmware timeout, before placing the robot on the floor.

## Checks

```bash
python3 -m unittest discover -s dashboard/tests -v
python3 -m py_compile dashboard/core.py dashboard/server.py
```

For an integration check with the console already running, source Jazzy and run:

```bash
python3 dashboard/tests/integration_check.py
```

This creates a unique synthetic namespace, publishes encoder telemetry, and checks Twist delivery, command expiration, and rejected requests. It never commands an existing robot namespace. Restart the console after testing to clear the synthetic robot from its in-memory history.
