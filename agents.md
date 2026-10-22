Build **Layer 1 (Real-Time Control)** of the Atlas V1 ESP32 firmware using the existing project and build environment.

### 1. Files to Create

- `AtlasConfig.h` — centralized hardware configuration and tunable parameters.
- `MotionController.h/.cpp` — motor control, encoder processing, distance-based movement, and 90° turns.
- `SensorManager.h/.cpp` — four ultrasonic sensors and validated distance readings.
- `SafetyController.h/.cpp` — local safety monitoring and emergency stops.
- `Atlas.ino` — initialization and nonblocking coordination of these modules.

### 2. Hardware

- ESP32 DevKit V1.
- Two DRV8833 motor drivers controlling four TT motors: one group for the left wheels and one for the right.
- Independent left/right wheel encoder inputs.
- Four ultrasonic sensors: front, back, left, and right.
- Wheel diameter: **6.5 cm**.
- Turns: **90° only**.

Do not invent GPIO assignments, encoder resolution, encoder count conventions, or wheel track width. If required information is missing, expose it as an unconfigured setting and explain what must be measured before the relevant feature can operate safely.

### 3. Configuration — `AtlasConfig.h`

Centralize all adjustable parameters, including:

- GPIO assignments and pin mappings.
- Motor direction inversions, PWM settings, speed limits, acceleration ramps, and braking behaviour.
- Wheel diameter, encoder counts per revolution, count convention, wheel track width, movement tolerances, and turn calibration.
- Ultrasonic sensor pins, valid distance ranges, obstacle thresholds, hysteresis, reading timeouts, and maximum reading age.
- Motor stall detection, movement timeouts, sensor-fault policies, and safety thresholds.
- Sensor sampling intervals, control-loop intervals, and diagnostic logging options.

Use clearly named constants or configuration structures. Validate settings at startup and fail safely when essential hardware parameters are invalid.

### 4. Module Responsibilities

**MotionController**
- Control both motor groups independently.
- Process encoder inputs using short, safe interrupt handlers.
- Convert encoder counts into distance.
- Support nonblocking forward/backward movements and calibrated 90° turns.
- Provide movement progress, completion, and failure status.
- Support immediate stopping from the safety system.

**SensorManager**
- Read all four ultrasonic sensors without long blocking delays.
- Stagger measurements to reduce cross-interference.
- Track reading validity, timestamps, and sensor faults.
- Never interpret invalid or stale readings as proof that a path is clear.

**SafetyController**
- Monitor obstacle conditions, sensor validity, motor stalls, and movement timeouts.
- Override normal movement and stop the motors when necessary.
- Latch critical faults where appropriate; require an explicit reset after critical failures.
- Operate independently of Wi-Fi, backend availability, and navigation code.

**Atlas.ino**
- Initialize configuration and hardware modules.
- Run a cooperative, nonblocking control loop.
- Coordinate motion, sensor updates, and safety checks.
- Include a simple configurable hardware-test mode for validating motors, encoders, sensors, and stopping behaviour.

### 5. Engineering Requirements

- Use `.h` and `.cpp` pairs for the modules listed above.
- Keep code modular, readable, and compatible with the project's existing framework and dependencies.
- Avoid unnecessary FreeRTOS tasks, busy loops, long `delay()` calls, blocking movement functions, and excessive dynamic memory allocation.
- Keep motor control and safety responsive even if a sensor reading fails.
- Include concise diagnostic output and useful fault codes.
- Do not implement Wi-Fi, backend communication, route recording/replay, checkpoint management, obstacle bypassing, display, or OTP functionality in this phase.
- Do not add dependencies, migrate frameworks, or rewrite unrelated working code without justification and approval.

### 6. Validation and Deliverables

1. Inspect the existing repository and build configuration before editing.
2. Create the files and implement Layer 1.
3. Compile using the project's actual build environment.
4. Provide a summary of files changed, configurable parameters, missing hardware information, and build results.
5. Provide a practical test checklist for motor direction, encoder calibration, distance accuracy, 90° turns, ultrasonic readings, and emergency stops.
6. Clearly distinguish features verified by compilation from those physically tested on hardware. Never claim a hardware test was performed if it was not.

**Priority:** Reliable, safe, testable hardware control with extensive but sensible customization. Do not overengineer Layer 1 or implement later layers prematurely.

### Mandatory Espressif MCP Usage

Before implementing Atlas V1 Layer 1, **use the configured Espressif Documentation MCP** to verify hardware APIs, ESP32-specific behaviour, and implementation details.

1. Consult official Espressif documentation for ESP32 GPIO, interrupt handling, PWM (LEDC), timing, and any FreeRTOS APIs used.
2. Verify that every ESP-IDF API or ESP32-specific feature is compatible with the framework and version used by this project. Do not mix ESP-IDF and Arduino APIs incorrectly.
3. Use the MCP to verify API names, function signatures, supported features, limitations, and recommended usage before implementing them.
4. Prefer official Espressif documentation over model memory or unverified third-party examples.
5. Do not invent APIs, functions, configuration options, or hardware capabilities. If the MCP cannot verify something, explicitly identify it as unverified and investigate further before relying on it.
6. Inspect the existing project and its build configuration before selecting APIs or changing dependencies.
7. After implementation, compile the firmware using the project's actual build environment and resolve any errors.

**MCP verification requirement:** Before coding, confirm that the Espressif MCP is available and retrieve relevant documentation. If the MCP is unavailable or a lookup fails, report that limitation instead of claiming the implementation was verified through it.

In your final response, briefly list which official documentation topics you consulted, which implementation decisions they informed, and whether compilation succeeded. Distinguish documentation verification and successful compilation from physical hardware testing.
