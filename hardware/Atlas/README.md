# Atlas V1 — Layers 1, 2 and 3

PlatformIO Arduino firmware for **ESP32 DevKit V1 / classic ESP32**. Build the
existing `esp32dev` environment from this directory; the sketch and modules live
in `src/`. Do not combine it with `../libot-esp.ino` (two `setup()`/`loop()` pairs).
Layer 3 adds Wi-Fi provisioning, authenticated HTTPS and one isolated network worker.
It uses the existing ArduinoJson dependency and Arduino/ESP32 framework libraries.
The existing PlatformIO framework, board and dependency configuration is unchanged.

## Separate legacy firmware

`../libot-esp.ino` already handles EEPROM Wi-Fi profiles, captive-portal setup,
backend telemetry/commands, reprovisioning and reset. Its `moveRobot()` is empty,
and its call on line 545 lacks a semicolon. It has no motor, encoder or sensor
configuration. It is **unchanged**: its network calls and delays are not safe to
reuse in this cooperative real-time loop.

## Files and responsibilities

| File | Purpose |
| --- | --- |
| `AtlasConfig.h` | Pins, confirmations, calibration, tunings, startup validation and fault names. |
| `MotionController.h/.cpp` | Four PWM inputs controlling two wheel groups; unsigned encoder interrupts; ramped nonblocking distance moves and in-place 90° turns; progress, stops and encoder-preserving resume. |
| `SensorManager.h/.cpp` | Four staggered trigger/echo sensors, interrupt-captured pulse widths, timestamps and invalid/stale/fault states. |
| `SafetyController.h/.cpp` | Sole owner of motion permission; obstacle, sensor, stall, timeout and loop-overrun checks; critical fault latch and explicit reset. |
| `Navigation.h/.cpp` | Explicit robot states; manual commands, recording, replay, pause/resume, cancellation and fault coordination. |
| `Routes.h/.cpp` | Fixed-capacity RAM route; encoder-based straight/90° segments, execution, interruption and deadlines. |
| `CheckpointManager.h/.cpp` | Wheel endpoint validation, fresh sensor evidence and bounded observation retries. |
| `ObstacleAvoidance.h/.cpp` | Sustained all-sensor clearance, interruption budget and bounded wait/abort. No bypass manoeuvres. |
| `NetworkManager.h/.cpp` | Wi-Fi profiles, nonblocking reconnect, temporary captive portal and stop-approved profile writes. Scoped in `Atlas` to avoid the Arduino 3.x core class of the same name. |
| `BackendClient.h/.cpp` | Bounded, authenticated HTTPS requests and strict backend payload validation. |
| `TelemetryManager.h/.cpp` | Copied queues, command leases/deduplication, start acknowledgements, measured results and supported telemetry. |
| `AtlasSecrets.example.h` | Empty template for the ignored local device-key/trusted-CA header. |
| `Atlas.ino` | Initializes all three layers; interleaves safety checks, network queue processing, bounded diagnostics and optional test commands. |

Use all motion, sensor, safety and navigation interfaces from the single Arduino
loop, never from the network worker. Interrupt capture uses short spinlocks;
Layer 3 shares only atomic flags and bounded, copied queue messages.
Movement functions return acceptance immediately; inspect `progress()`, `state()`
and `fault()` for completion/failure. Positive distance is forward, negative is
backward; `turn90(true)` is clockwise and `turn90(false)` counterclockwise.

## Navigation interface and route semantics

Use the existing global `navigation` instance from the cooperative loop. Its
commands return acceptance, not completion; inspect `state()`, `error()`,
`pauseReason()` and `movementPending()`. Layer 3 dispatches through this interface;
it never drives PWM or clears safety faults directly.

- `driveDistance(signedCm)` and `turn90(clockwise)` initiate one bounded manual
  command in `IDLE`/`COMPLETED`, or append a completed command while `RECORDING`.
  Wait for its endpoint checkpoint before issuing another command.
- `beginRecording()` **replaces the previous RAM route**. Only completed,
  checkpoint-validated commands are appended. Straight segments store the mean
  measured wheel travel, not elapsed time; turns retain their calibrated 90° kind.
  This is commanded-segment recording, not freehand/joystick trajectory capture.
  `finishRecording()` commits a nonempty stationary draft. Cancellation or faults
  during recording discard the unfinished draft; power loss discards all routes.
- `replay()` executes the committed segments sequentially and enters `COMPLETED`
  only after the final checkpoint. Place the robot at the recorded start pose
  and orientation yourself. There is no automatic return-to-start or localization.
- `pause()` stops the current move and preserves the activity. `resume()` requests
  continuation only when safety permits; `cancel()` stops and abandons the current
  activity. A committed route survives cancellation/failure and can be replayed
  only after the operator restores a safe start pose and clears faults.
- `resetSafety(nowMs)` delegates to Layer 1's explicit reset. It never resumes.
  Emergency stop remains `safety.emergencyStop()` and always takes precedence.

Robot states are `IDLE`, `MANUAL`, `RECORDING`, `REPLAYING`, `PAUSED`, `COMPLETED`
and `FAULT`. Invalid/unsafe commands are rejected without starting motors.
Sensor/critical safety failures abort the activity; obstacle stops pause it.
`MotionController::resume()` is the only added Layer 1 motion interface. It retains
the **original independent wheel baselines, target, direction and duty**, counting
coasting travel during the stop. A partial turn therefore does not restart a full
90° turn. It still requires Layer 1 permission, calibration and stopped dwell.
Layer 2 rejects excessive paused overshoot rather than trying to reverse/correct it.

### Checkpoints and obstacle policy

Each endpoint requires both wheel distances near their target, limited wheel
mismatch, and a full fresh/clear four-sensor scan sampled after endpoint settling.
Replay also compares ranges with the recorded endpoint evidence. Retries only
re-observe; they never move the robot to search for a checkpoint. Similar ranges
cannot establish position or heading: slip, rollback, wrong direction and a changed
environment can invalidate replay despite encoder completion. Tolerances require
real floor/load calibration; this is not precise localization or collision proofing.

Obstacle clearance requires **all four** readings valid, fresh, nonfaulted and
above the existing stop threshold plus hysteresis continuously. Any unknown or
blocked reading resets that interval. With the unchanged default
`LatchObstacleStops=true`, the operator must clear the scene, explicitly reset,
and separately request resume after sustained clearance. Automatic continuation
is available only for non-latching stops when `AutoResumeAfterObstacle=true`.
A user pause always requires explicit resume, including during obstacle waiting;
it cannot bypass clearance. The obstacle deadline includes waiting for operator
acknowledgement. Exhausting time or interruption limits aborts without a bypass.

All settings are centralized in `src/AtlasConfig.h`:

| Setting | Initial value |
| --- | --- |
| `MaximumRouteSegments` | 32, fixed RAM storage |
| `RouteStartTimeoutMs` | 3,000 per launch/resume attempt |
| `RouteSegmentTimeoutMs` / `RouteTimeoutMs` | 60,000 / 600,000, including pauses |
| `CheckpointDistanceToleranceCm` / `CheckpointWheelMismatchCm` | 2 / 2 |
| `CheckpointSensorToleranceCm` | 35 |
| `CheckpointSettleMs` / `CheckpointAttemptTimeoutMs` | 200 / 1,200 |
| `CheckpointMaxRetries` | 2 additional observations |
| `ObstacleClearanceMs` / `ObstacleWaitTimeoutMs` | 1,000 / 15,000 |
| `MaximumObstacleInterruptions` | 3 per manual command, recording session or replay |

`navigationTuningError()` rejects inconsistent limits. Timeouts bound operations,
not movement distances; the unchanged Layer 1 movement/stall/control deadlines
also apply. Sensor thresholds/hysteresis are shared with Layer 1, not duplicated.

## Required configuration — no assumed wiring or calibration

Everything is in `AtlasConfig.h`. The delivered defaults intentionally produce
`CONFIG` and **cannot drive motors**. Unknown pins are `-1`; unknown calibration
is zero; hardware confirmation flags are false. Do not change a confirmation
flag until the corresponding electrical/wiring condition has been verified.

1. **Motor wiring:** supply two input pins for each side. The implementation
   assumes one separate DRV8833 H-bridge per motor, with the two bridges on a side
   sharing corresponding *input signals*. Never join bridge outputs. Confirm
   polarity, motor supply, common ground, stall current, thermal limits and input
   fanout before setting `MotorInputFanoutConfirmed`. If the hardware uses a
   different topology, adapt the interface before enabling it. Configure a common
   `DriverSleepPin` or explicitly confirm externally managed nSLEEP. Fit hardware
   pull-downs on control inputs/nSLEEP so boot, reset and unconfigured firmware
   cannot energize motors. Software cannot safely control unknown pins.
2. **GPIO:** supply two encoder inputs and four trigger/echo pairs. All pins must
   be distinct. The conservative policy excludes flash, boot straps, UART0 and
   module-dependent GPIO16/17. GPIO34/35/36/39 are input-only and need external
   pulls when required; internal encoder pull-ups are rejected on these pins.
   Confirm the actual DevKit/module pinout before changing the policy.
3. **Ultrasonic interface:** identify the actual sensor model and confirm its
   trigger timing, electrical levels and usable range. ESP32 inputs are not 5 V
   tolerant: provide appropriate echo level conversion. Set
   `SensorElectricalInterfaceConfirmed` only after this check. No echo, out-of-range,
   stuck-high and stale data mean **unknown/unsafe**, not an unobstructed path.
4. **Encoders:** confirm one countable signal per side, then set
   `SingleSignalEncodersConfirmed` and choose `CountEdges` (`Rising`, `Falling` or
   `Both`). Measure effective counts per **wheel** revolution independently for
   left and right using that exact edge mode. Do not substitute unqualified PPR,
   motor-shaft counts or assumed gearing. Counts are unsigned; they cannot detect
   wrong direction, rollback or wheel slip. Quadrature direction decoding is not
   implemented; do not enable this interface if signed feedback is required.
   Tune `EncoderMinEdgeIntervalUs` only after measuring noise and maximum valid
   edge rate. Edge travel must be no larger than `DistanceToleranceCm`.
5. **Turns:** measure effective wheel contact-centre track width and CW/CCW
   skid-steer calibration. A nominal in-place 90° move uses wheel travel
   `pi * track / 4 * directionScale`. Both direction scales default to zero, so
   turns remain rejected until explicitly calibrated. Distance moves do not
   require turn calibration. Encoder distance is not an absolute heading sensor.

Other centralized adjustments: direction inversions; PWM frequency/resolution
and core-2 channel reservation; cruise/turn/minimum/maximum duty; acceleration and
deceleration; coast/brake stop modes; travel/slowdown tolerances; direction dwell;
sensor ranges, thresholds, hysteresis, sound speed, timeouts and age; fault-latch
policies; control/stall/movement timings; logging and hardware-test settings.
Duty values are **PWM counts, not percentages**. Defaults are starting values,
not validated speed, torque, stopping distance or collision guarantees.

## Safety behaviour and limitations

- All four sensors must be fresh and clear before permission is granted at rest
  or a reset is accepted. Turns always require all four. Straight travel also
  requires all four by default; the optional directional policy still requires
  the front sensor for forward and back sensor for backward motion.
- An obstacle must clear its stop threshold **plus hysteresis** before release.
  Startup sensor warm-up inhibits movement without creating a sensor latch.
- The first invalid required measurement during motion stops immediately when
  observed. `SensorFaultAfterFailures` is diagnostic, not permission to drive
  blind. Sensor/obstacle motion stops latch by default; making them non-latching
   does not itself resume a stopped move. Layer 2 may request safe continuation
   only after its clearance and acknowledgement policy above.
- Critical motor stalls, movement timeout, emergency stop, PWM failure and
  excessive control-loop gaps latch. Reset requires stationary, initialized,
  calibrated hardware and all fresh/clear sensors. Reset never restarts motion.
  A failed PWM initialization requires fixing the cause and restarting.
- Stops bypass ramps. `Coast` removes drive; it does **not** guarantee zero wheel
  travel. `Brake` must be electrically and physically validated before use.
  Partial initialization/PWM faults force coast; new moves release any previous
  brake before ramping. A configured nSLEEP output is disabled first on coast.
- Nominal control interval is 5 ms; one sensor is triggered per 60 ms slot
  (approximately 240 ms per four-sensor scan), with a 25 ms echo deadline and
  400 ms maximum valid age. These are cooperative software timings, not hard
  real-time guarantees. Set thresholds using measured maximum speed, scan and
  processing latency, actual braking distance and the robot's swept footprint.
- Echo waiting is interrupt-driven; no `pulseIn`, waiting for a move, or long
  delays. The only deliberate busy wait is a bounded 10–20 microsecond trigger.
  Loop diagnostics use fixed buffers and are dropped if TX space is unavailable.
- Stall checks examine each side independently after startup grace, when its
  duty reaches the configured minimum; a side already at its target is ignored.
- Fault labels: `CONFIG`, `PWM`, `OBSTACLE`, `SENSOR`, `STALL_LEFT`, `STALL_RIGHT`,
  `MOVE_TIMEOUT`, `E_STOP`, `CONTROL_GAP`; `OK` indicates no current safety fault.
  Sensor errors: 0 none, 1 unconfigured, 2 no reading, 3 timeout, 4 stuck high,
  5 out of range, 6 stale. Motion states: 0 idle, 1 moving, 2 complete, 3 stopped,
  4 failed. Sensor indices: 0 front, 1 back, 2 left, 3 right.

**Keep a physical motor-power emergency disconnect available.** This cooperative
software cannot stop outputs while the CPU is hung or code outside the loop is
blocking. Do not insert the old HTTP/Wi-Fi routines into this loop.

## Manual bring-up checklist (not yet performed)

Use a current-limited supply, lift the wheels, keep hands clear and keep the
physical power disconnect within reach. Never test stall conditions at unrestricted
current or rely on serial input as a physical emergency stop.

- [ ] With the delivered unconfigured settings, verify `CONFIG`, no motor drive,
      and safe outputs throughout power-up, reset and upload.
- [ ] Configure wiring, edge mode and confirmations. Leave CPR/turn values zero
      initially. Manually rotate each wheel group several full revolutions and
      use logged total counts to determine each effective counts/revolution.
      Confirm the other side does not count and noise does not add edges.
- [ ] Check every sensor against measured targets inside the configured range.
      Check sensor ordering, offsets, threshold+hysteresis and cross-talk.
      Disconnect an echo input or remove targets: readings must become invalid,
      never a synthetic maximum-range clear reading. Test stuck-high and stale
      cases with protected, correctly levelled signals.
- [ ] Set measured CPR values. Wait for all four clear readings and explicitly
      reset after the configuration latch if necessary. Set `HardwareTestMode`
      true and use the configured baud. No motion is automatic.
- [ ] Send **one command at a time**: `f` / `b` move `TestDistanceCm`; `s` cancels;
      `x` latches emergency stop; `r` explicitly requests reset. Newline bytes are
      ignored. Motion commands are refused if unsafe, pending a checkpoint, uncalibrated
      or within direction-change dwell. Do not paste or queue command sequences.
- [ ] With wheels still lifted and clear sensor targets, verify all four motor
      polarities in `f` and `b`, and each encoder's count changes. Correct per-side
      inversion or individual motor wiring before floor tests. Verify immediate
      drive removal with `s` and latch/no-resume with `x` then `r`.
- [ ] At low speed in a clear bounded test area, measure forward/backward distance
      against a tape, overshoot and stopping distance at several loads/speeds.
      Tune CPR, tolerance, ramps and duty limits; independent distance stopping
      is not straight-line heading control.
- [ ] Measure track and calibrate each turn direction under controlled conditions.
      Enter explicit provisional scales only for calibration, then send `c` for
      clockwise or `a` for counterclockwise 90°. Measure actual angle and adjust
      its scale by `90 / measuredAngle`. Repeat on the intended floor and load;
      verify both directions and four-turn accumulated error.
- [ ] Approach a target slowly in forward/backward travel and during a turn.
      Confirm the appropriate obstacle stops, hysteresis, latch and refusal to
      reset until all readings are clear. Repeat with failed sensor readings.
- [ ] With current safely limited, simulate missing encoder pulses per side and
      verify the corresponding stall latch. Verify a movement timeout with a
      deliberately shorter test setting, and verify a simulated scheduling gap
      causes `CONTROL_GAP` when the loop next runs. Remove test modifications.
- [ ] Verify reset cannot resume the previous move and cannot clear unsafe
      readings. Recheck boot/reset behaviour with the final wiring and stop mode.
      Disable hardware-test mode before integrating future layers.

## Layer 2 hardware validation checklist (not yet performed)

Complete the Layer 1 checks first. Optional serial commands require
`HardwareTestMode=true`; no command runs automatically. Diagnostics include
navigation state/error, pause reason, route index/count and checkpoint retries.

- [ ] Send `n` to start a new recording, then individual `f`/`b`/`c`/`a` commands,
      waiting for checkpoints between them; `e` finishes. Verify empty recordings
      and commands beyond capacity are rejected. `s` discards unfinished drafts.
- [ ] Manually restore the recorded start pose/orientation; `p` replays. Compare
      physical distances and CW/CCW angles with the recorded path under intended
      load/floor conditions. Verify completion only after the final checkpoint.
- [ ] Use `h`/`u` to pause/resume straight travel and a partial 90° turn. Check each
      wheel's remaining travel including coast; verify no full-turn restart and
      no automatic movement after `r` alone. `s` must cancel immediately.
- [ ] Introduce/remove an obstacle during manual travel and replay. Brief clearance
      must not resume; unknown/stale readings must reset the clearance window.
      With default latching, verify clear scene then `r`, sustained clearance,
      then `u`. Repeat with `h` during obstacle waiting and with an obstacle that
      appears during a user pause; neither may skip the full clearance interval.
- [ ] Hold the obstruction or withhold acknowledgement beyond the wait deadline;
      verify safe abort. Verify interruption limits across repeated stops, and
      route/segment deadlines including user pauses. Use controlled test settings
      when necessary and restore them afterward.
- [ ] Change endpoint sensor evidence beyond tolerance; verify bounded observation
      retries then abort without corrective movement. Check wheel mismatch and
      excessive coasting overshoot also abort. Test failed sensor and E-stop paths
      while moving, paused and checkpointing; reset must never resume by itself.
- [ ] Repeat power/reset tests: no automatic replay, no persisted RAM route, safe
      unconfigured outputs. Disable hardware-test mode after validation.

## Layer 3: provisioning and backend integration

### Configuration and credentials

1. Copy `src/AtlasSecrets.example.h` to `src/AtlasSecrets.local.h` (git-ignored).
   Provision a unique `ATLAS_DEVICE_KEY` matching the backend's `ATLAS_DEVICE_KEYS`
   entry for this robot, and the issuing trusted root PEM in `ATLAS_TLS_CA`.
   Never commit credentials. An empty key/CA disables backend requests; TLS
   verification is never disabled. Certificate rotation requires reprovisioning.
2. Review `BackendBaseUrl` (default `https://libot-8okb.onrender.com`), `RobotId`
   (`robot1`) and all network tunables in `AtlasConfig.h`. No browser session is
   used. SNTP starts asynchronously; HTTPS/results wait for a plausible real UTC
   clock, with no fabricated timestamps or blocking clock-wait loop.
3. `BackendStepCm=0` deliberately leaves forward/backward **unconfigured**. Choose
   and physically validate an explicit bounded distance before enabling them.
   Each distinct command ID executes at most once during the current boot.
   `left`/`left_turn` and `right`/`right_turn` map to one calibrated 90° turn,
   not continuous joystick driving. All existing hardware defaults remain unsafe
   to drive until wiring, calibration and confirmations are supplied.
4. On boot the firmware tries saved profiles without waiting in the control loop.
   When disconnected, join the open **Libot_Setup** AP and visit
    `http://192.168.4.1`. The Atlas-01 Wi-Fi page supports async scan, manual entry,
    password Show/Hide, separate Save and Connect actions, saved history and forget.
    It has no external resources, fonts, frameworks or runtime dependencies.
   Captive redirects may require opening that exact IP before submitting a form.

The EEPROM layout remains 512 bytes, five 96-byte profiles (31-character SSID and
63-character password limits plus terminators), with the legacy flag at byte 500.
Saved credentials are plaintext EEPROM, as before: use setup only in a trusted
physical environment. Passwords/device keys are never returned by history/status
or logged. The AP closes three seconds after station connection. Profile writes,
forget and reset first request main-loop cancellation; EEPROM commits run only
after motor outputs have been stopped, since flash writes may stall both cores.
Reset clears Wi-Fi storage only: no reboot, device-key wipe or safety reset.
The backend has no applied-configuration acknowledgement, so none is invented.

### Local connection page

The design follows `tasks.txt` and the Atlas-01 Connectivity Dashboard reference:
obsidian surfaces, muted amethyst, a restrained Libris header, and mobile-first
credentials/available/saved sections. The displayed product identity is Atlas-01;
the backend robot ID remains independently configured. No Vue route was added.

Edit `portal/index.html`, then run `python portal/generate.py` from this firmware
directory. Commit both the HTML and `src/SetupPage.generated.h`; the generator uses
only Python's standard library. `python portal/generate.py --check` detects a stale
header. PlatformIO does not run the generator automatically. The checked-in page
is gzip-compressed (4,838 bytes) in flash and served with Content-Encoding/Length;
the ESP32 does not decompress it or require a filesystem. JSON replies stay plain.

Existing local `/status`, `/history`, `/scan`, `/connect` and `/forget` routes remain.
The user-authorized save-only API addition is:

- `POST /save` with JSON `{ "ssid": "...", "pass": "..." }`: save credentials
  without starting a connection or interrupting an existing one. After success,
  further automatic reconnect attempts are held for this setup session until an
  explicit Connect (or reset/reboot); boot-time saved-profile reconnect is unchanged.
- `/connect` still accepts either `{ "slot": 0 }` or `{ "ssid": "...", "pass": "..." }`.
  New credentials are saved and connected; a saved slot reuses its stored password.
- Successful mutation replies retain `ok` and add `operation` and `pending`.
  `/status` adds matching `operation`, `operationStatus` (`pending`, `complete`,
  `failed`), `connecting`, `connectionFailed`, and `attemptSsid`. The page waits
  for the approved EEPROM commit, not merely HTTP acceptance, before showing Saved.
  Saved-slot Connect does not require a new EEPROM operation.
- `/scan` adds a readable `error` when an asynchronous scan fails. Simultaneous
  scan/connection may fail; manual network entry remains available.

These are local provisioning routes, not new FastAPI endpoints. AP-only access,
Host/Origin checks, JSON framing/body limits, existing EEPROM layout and the main
loop's stop-before-flash approval are preserved. No passwords are returned. An
unconfirmed portal disconnect is not reported as a successful connection.

### Data flow and supported endpoints

One low-priority core-0 worker owns Wi-Fi, DNS, portal sockets and HTTP/TLS. These
libraries may block despite timeouts; placing them in the motor loop would violate
its 50 ms gap budget. The worker stack is 16,384 **bytes**; the existing Arduino
loop retains every navigation/safety/motor operation. Queues hold one latest
command, eight results, eight acknowledgements and one latest sensor sample.

| Endpoint (relative to backend base) | Firmware use |
| --- | --- |
| `GET /robot/{id}/command` | Validated command, IDs/sequences, execute flag and Wi-Fi/reset directives. |
| `POST /robot/{id}/command/result` | `started`, `completed`, `failed`; real UTC, measured independent encoder deltas, commanded wheel directions and valid sensor ranges when available. |
| `POST /robot/{id}/sensors` | Recording-only samples with required command/recording IDs, sequence, real UTC and valid ranges. |
| `POST /robot/{id}/telemetry` | Only `wifi_ssid`, `signal_strength`, `uptime_seconds`, `firmware_version`, as accepted by the current backend. |

Every request uses `X-Atlas-Key`, verified CA/hostname TLS and no redirects.
HTTP/1.0 responses must have finite JSON Content-Length of at most 4,096 bytes;
chunked/oversized/malformed responses fail closed. Request buffers are 1,536 bytes,
JSON nesting is bounded, connect/read/body limits are 800 ms and TLS handshake
timeout is three seconds. DNS/library timing can still exceed individual limits;
worker isolation protects local control, not an end-to-end network deadline.

Polls target 250 ms. Responses older than 500 ms are rejected; a movement lease
expires 900 ms after the last successful poll's **request start**. Loss of link,
poll failure, epoch change, stop/supersession, result-delivery failure, safety
fault or navigation pause cancels network movement without reset or auto-resume.
Boot/reconnect's first observed command establishes a baseline and cannot start
motion; submit a new command afterward. A 16-ID recent ring and transport sequence
high-water mark suppress duplicates; this is RAM-only, not persistent exactly-once
execution. A new movement waits for the backend's `started` acknowledgement before
Navigation is called. Local RAM recording/replay cannot be mixed with remote drive.

Results are retried in order with the identical payload/timestamp, at most three
attempts 500 ms apart; permanent HTTP 4xx (except 408/429) are not retried.
Result/ack overflow, missing result UTC or exhausted delivery retries inhibit
remote movement until restart. Reconnect uses bounded backoff: Wi-Fi 1–30 seconds,
backend 0.5–8 seconds. Slow acknowledgements/polls can intentionally prevent or
cancel motion: fail-safe behaviour takes priority over availability.

Optional sensor uploads and five-second telemetry run only while remote movement
is inactive and no result is pending. During a recording move the main loop captures
at most one latest one-second sensor sample for later upload; intermediate samples
may be overwritten. Completion/failure results carry fresh valid ranges themselves.
Unsigned encoders measure independent counts, **not physical direction**; result
directions come from the commanded move, with zero ticks represented as stopped.

There is no supported API for general navigation/progress/route/event/battery
telemetry, so these fields/endpoints are not invented. Faults are exposed through
execution failures and existing local diagnostics. There is no battery interface.
Backend recording/save/checkpoint attestation remains staff-managed; local RAM
routes are not uploaded. **Guidance protocol 2, OTP/display, exact independent
wheel-tick replay, arrival confirmation and SLAM/vision are not implemented.**

### Layer 3 validation checklist (not yet performed)

- [ ] Provision trusted CA/key and verify missing/wrong key, wrong hostname/CA,
      unavailable UTC, malformed/chunked/oversized JSON and slow server fail closed.
- [ ] Verify legacy saved profiles survive upgrade; test scan/connect/forget/reset,
      failed AP attempts, reconnect backoff and no credentials in responses/logs.
      Confirm every profile commit occurs with motor outputs stopped.
- [ ] With a safe bench setup, verify baseline rejection, fresh command execution,
      started acknowledgement before drive, duplicate suppression and measured
      independent tick deltas matching backend recording expectations.
- [ ] Cut Wi-Fi/backend access mid-move, delay polls/results and send stop; verify
      bounded local cancellation, latched safety behaviour and no reconnect resume.
- [ ] Check maximum loop gap, worker stack margin, heap/TLS stability and sensor
      timing under portal/scan/network load. Rebuild with real credentials/CA and
      verify flash capacity before upload. Complete Layer 1/2 physical checks too.

## Official documentation consulted before implementation

Espressif Documentation MCP was available and used for these topics; official
versioned source was additionally inspected where the project version was absent.

- [Arduino-ESP32 GPIO](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html):
  `pinMode`, digital I/O, `attachInterruptArg`, edge modes and short ISR callbacks.
- [ESP32 datasheet](https://documentation.espressif.com/esp32_datasheet_en.html):
  input-only GPIOs, pull-up limits and restricted pin policy.
- [LEDC API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html)
  and [2.x → 3.0 migration](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html):
  version-gated attach/write/detach adapter. Core 2 uses explicit channels; core 3
  uses pins. Checked the official
  [2.0.17 LEDC header](https://github.com/espressif/arduino-esp32/blob/2.0.17/cores/esp32/esp32-hal-ledc.h)
  for legacy signatures; the legacy write function cannot report failure.
- [ESP Timer](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html#obtaining-current-time):
  `esp_timer_get_time()` microsecond timestamps in short ISRs, without timer tasks
  or callbacks. The official
  [2.0.17 timing implementation](https://github.com/espressif/arduino-esp32/blob/2.0.17/cores/esp32/esp32-hal-misc.c)
  also uses this API for Arduino timing.
- [ESP-IDF critical sections](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos_idf.html#critical-sections)
  and [4.4.7 FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/freertos.html):
  `portMUX_TYPE`, task/ISR critical-section pairs and short snapshots; no custom
  tasks or blocking calls inside critical sections.
- [ESP32 partition tables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html):
  single factory application without OTA metadata, partition alignment and the
  default table offset at `0x8000`. Consulted through Espressif Documentation MCP
  for the V1 partition migration; the installed partition generator also validated
  the CSV and generated binary.

## V1 flash layout and USB-only servicing

The existing `esp32dev` environment selects `partitions_atlas_v1.csv` through
`board_build.partitions`. It targets the configured **4 MiB** flash device:

| Partition | Type / subtype | Offset | Size | End (exclusive) |
| --- | --- | --- | --- | --- |
| `nvs` | data / nvs | `0x9000` | `0x5000` (20 KiB) | `0xE000` |
| `factory` | app / factory | `0x10000` | `0x300000` (3 MiB) | `0x310000` |
| `spiffs` | data / spiffs | `0x310000` | `0xE0000` (896 KiB) | `0x3F0000` |
| `coredump` | data / coredump | `0x3F0000` | `0x10000` (64 KiB) | `0x400000` |

The bootloader and partition table retain the framework's standard locations;
the table starts at `0x8000`. The `0xE000`–`0xFFFF` gap is not an OTA-data
partition. The unchanged Arduino upload recipe includes its 8 KiB `boot_app0.bin`
there; this does not add an OTA update workflow or a second application slot.

**V1 servicing is USB-only.** There are no OTA application slots or OTA rollback.
No firmware source, framework upload recipe, TLS/networking, provisioning or
motion/safety logic was changed for this migration.

### Migration and data preservation

This change updates project/build artifacts only: **no device was flashed or
erased**. A future migration must install the matching partition table and
application, not only replace the application under the old table. Before any
device servicing, confirm the physical unit has at least 4 MiB flash, stop and
secure the robot, and arrange a recoverable backup/reprovisioning plan.

- NVS stays at `0x9000`, size `0x5000`. Arduino EEPROM stores the existing 512-byte
  `eeprom` blob in NVS, including the five saved Wi-Fi profiles. Keeping this range
  unchanged permits preservation during selective servicing, **but does not
  guarantee it**: full-chip/NVS erasure or NVS initialization recovery can delete
  credentials. Back up device data securely and be prepared to reprovision;
  source-file backups are not device-data backups.
- SPIFFS moves from `0x290000` to `0x310000` and shrinks from `0x160000` to
  `0xE0000`. Treat old filesystem contents as lost unless separately backed up
  and restored/recreated for the new layout. The enlarged factory slot overlaps
  former application/filesystem space. The portal itself remains compiled into
  the application, not SPIFFS.
- The previous OTA metadata/second slot no longer have a role; old images are
  not a rollback mechanism. Coredump stays at the same offset/size, but old dumps
  should be archived separately rather than relied on after migration.
- The current build has no `AtlasSecrets.local.h`; CA/key defaults remain empty
  and backend access fails closed. Provision the intended trusted CA/device key
  separately, then rebuild and recheck the complete image before deployment.

Pre-edit configuration, README, default CSV, previous artifacts and a project
SHA-256 manifest were backed up outside the project at
`C:\Users\Dell\AppData\Local\Temp\opencode\atlas-v1-migration-20261021-221609`.
These backups contain no device flash readout.

## Build and validation status

PlatformIO MCP compiled the original Layer 1 baseline and the integrated Layers 2/3
using the existing `platformio.ini`, environment `esp32dev`. The resolved toolchain
is Espressif32 8.0.0, Arduino-ESP32 3.3.12 and Xtensa GCC 14.2.0. The environment
is not newly pinned or changed. Reproduce locally with `pio run -e esp32dev`.
Latest V1 partition-migration build (2026-10-21), including the current Atlas-01
portal: **SUCCESS**, no reported warnings, through PlatformIO MCP with the existing
`esp32dev` environment and unchanged dependencies (ArduinoJson 7.4.3 resolved).

| Measurement | Actual bytes | Limit / remaining |
| --- | --- | --- |
| Static RAM | 59,404 | 327,680 total (18.1%) |
| PlatformIO application section total | 1,311,319 | 3,145,728 total (41.7%); 1,834,409 free |
| Complete fresh `firmware.bin` | 1,344,720 | 3,145,728 factory slot; 1,801,008 free |

The prior layout failed its size check by 599 bytes (1,311,319 against 1,310,720).
The application section total is unchanged by this migration; capacity increased,
not code-size optimization. The complete image is larger than that section total,
so both were checked. The new image was generated at 22:21:27 local time after
the ELF at 22:21:19, replacing the stale binary from an earlier build.

The installed Espressif partition generator validated alignment, non-overlap and
the 4 MiB boundary. Generated `partitions.bin` matches the CSV **byte-for-byte**,
with exactly the four partitions documented above and no OTA slots/metadata.
PlatformIO's 3,145,728-byte application limit confirms factory-slot selection.
Firmware SHA-256:
`6fc330872cb4c9074d53ec75134b081c064197622b9cf7104953b6a840825016`.

The previous flash-capacity blocker is resolved **for this unprovisioned build
under the new layout**, not for an application-only upload using an old table.
Hardware compatibility, migration/data retention and a build with the intended
CA/key remain unverified. No upload or device-data modification was performed.
Firmware artifacts are
`.pio/build/esp32dev/firmware.elf` and `firmware.bin`.
Arduino-ESP32 2.x guards remain, but this verification covers only the resolved
3.3.12 environment, not a separate 2.x build.

Espressif Documentation MCP was used again for Layer 2 timing/LEDC assumptions and
Layer 3 [Wi-Fi threading](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html#common-api),
[FreeRTOS tasks](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos_additions.html),
and official Arduino-ESP32 HTTPClient/NetworkClientSecure sources (timeouts,
CA verification, redirect handling). Installed asynchronous `configTime` source
was inspected; no blocking `getLocalTime` wait is used. Encoder snapshots and
motor permission remain in Layer 1.

Portal browser checks used mocked local API responses, not production mocks:
320/375/390/414/430/1024 px without horizontal overflow; equal-height action buttons;
password Show/Hide; scan loading/empty/error and input preservation; SSID HTML
injection safety; separate save-only then saved-slot connect; pending/failed
storage commits, connection failures/retry, and Manage/Forget. No page errors were
observed. Real Android captive-browser/gzip delivery and Wi-Fi/EEPROM tests remain
unperformed; browser checks do not validate firmware concurrency or physical safety.

**Not uploaded or physically tested; no live-backend integration tests.** Earlier
Layer 1/2 discovery found only COM1, not an identifiable ESP32; discovery was not
repeated for Layer 3. No native firmware runtime/simulation tests were run. Wiring, encoder counts per wheel
revolution, track width, turn scales, electrical confirmations and actual stopping
performance remain missing/unverified. Delivered fail-safe configuration is
unchanged. Complete all three hardware checklists before use: compilation and API
reference checks do not validate physical safety or navigation accuracy.
