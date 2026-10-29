# Atlas Central Confidence Processing System

## Status — 2026-10-27

The approved supervisory CCPS is implemented, including enforcement. Milestones
0–11 have compilation/host-test evidence; Milestone 12 resource measurements and
documentation are complete. **Release acceptance is blocked**, not complete:
the final `firmware.bin` is **1,053,072 bytes**, exceeding the hard **1,000,000-byte**
gate by **53,072 bytes**. See [CCPS_RESOURCES.md](CCPS_RESOURCES.md).

The ESP32 build succeeds. All ten host suites pass at both `-O0` and `-Os`
(220 reported checks/cases per configuration). No upload, motor activation,
physical calibration, live-backend test or database change was performed.
Delivered pins/calibration remain unconfigured and movement is inhibited.

## Ownership and enforcement

CCPS supervises the existing architecture; it does not own PWM, sensor capture,
route execution, network transport or local safety resets.

| Files in `src/` | Responsibility |
| --- | --- |
| `ConfidenceTypes.h` | Fixed-size evidence, decisions, reason/permission masks, execution hooks and movement records; finite-value, normalization, wrapping and freshness helpers. |
| `SensorConfidence.h/.cpp` | Separate validity, quality and freshness assessments for ultrasonic, encoder and IMU evidence. |
| `ImuIntegration.h/.cpp` | Proper mounting rotation, stationary bias/noise calibration, relative-yaw integration, continuity and finite calibration lifetime. |
| `MotionConfidence.h/.cpp` | Independent wheel progress, distance/yaw residuals, encoder/IMU agreement, overshoot and interruption-aware acceptance. |
| `MovementHistory.h/.cpp` | Exactly ten fixed movement records; weighted confidence, signed residual bias and residual trend. |
| `PoseEstimator.h/.cpp` | Relative planar pose, random covariance and persistent bias/scale sensitivities. |
| `ConfidenceSupervisor.h/.cpp` | Timestamp alignment, lifecycle accounting, trust/clearance/health, permissions, duty limits, checkpoint and recovery gates. |
| `ConfidenceIntegration.h/.cpp` | Adapter for existing motion/sensor/IMU/safety/navigation objects; no hardware ownership or new task. |

Integration changes are limited to `AtlasConfig.h`, `Atlas.ino`, `ImuManager`,
`MotionController`, `SafetyController`, `Navigation`, `Routes`, `CheckpointManager`,
`TelemetryManager` and `BackendClient`, plus host tests/resource tooling and the
explicit `-Os` build flag. `SensorManager`, `ObstacleAvoidance`, `NetworkManager`,
the portal, backend application and database were not rewritten for CCPS.

Before motor output, authorization is the conjunction of:

1. **Local safety:** configuration, required sensor clearance, latch state,
   stalls, movement timeout and control-gap checks remain authoritative.
2. **CCPS:** a fresh movement-bound constraint, acceptable evidence and sufficient
   duty ceiling. Missing/stale decisions, settling, holds or Lost deny motion.
3. **Execution/context:** existing navigation state and execution gate; remote
   commands additionally retain their identity, acknowledgement and lease checks.

`SafetyController::localCanMove()` exposes only the local result;
`canMove()` exposes the conjunction. CCPS can reduce the existing duty ceiling or
stop motion, never grant permission over a local fault. Lifecycle hard gates also
apply between periodic evaluations; an old ALLOW diagnostic cannot mask a new
hold, settling interval or Lost condition. Missing production execution hooks deny
movement. `ConfidenceObservationOnly=false` is the delivered setting; shadow mode
was validated before enforcement and is **not** an autonomous release mode.

## Evidence and confidence meaning

- Scores are **engineering indices, not calibrated probabilities**. Finite-value
  checks, explicit score-validity bits and hard gates accompany them. Do not read
  a displayed high score as proof of physical position or clearance.
- Sensor confidence is validity × quality × freshness. Freshness has a bounded
  grace interval followed by exponential decay and a hard expiry. Invalid/stale
  range data never becomes synthetic clearance. Sensor aggregation uses the
  weakest required evidence, not a vote that hides a failed sensor.
- Encoder checks include plausible edge rate, elapsed time, no progress and
  normalized independent-wheel progress mismatch. Unexpected edges can be valid
  measurements of **uncommanded motion** and still cause a navigation hard stop.
- Motion checks distinguish in-progress, complete and interrupted segments.
  An obstacle interruption is not automatically a distance-execution failure;
  wheel/yaw discontinuity or excessive residuals still deny acceptance.
- Navigation trust (`Initializing`, `Trusted`, `Acceptable`, `Degraded`, `Verify`,
  `Lost`), clearance (`Unknown`, `Clear`, `Blocked`) and health (`Ready`, `Inhibited`,
  `Fault`) are orthogonal. An obstacle or Wi-Fi loss alone does not establish Lost.
- `Start`, `Continue` and stopped `Verify` are independently gated. Warning
  persistence can reduce duty to a calibrated caution limit. Human intervention
  is requested for Lost, unacceptable terminal evidence or exhausted recovery
  accounting. `Correct` and `Recover` are never authorized by this implementation.

## Movement lifecycle and ten-record history

The existing motion controller emits start/resume/complete/stop/fault/reset-boundary
events containing monotonic encoder snapshots, movement ID, attempt and target.
CCPS captures the command context before starting and checks continuity before
allowing progress to count toward that command.

- Completion, interruption and failure settle before a record is finalized.
  Coast counts belong to the old command. The old aligned interval is flushed
  before changing direction or declaring rest, including intervals shorter than
  the normal pose cadence.
- Resume retains the logical movement ID and original target, increments the
  attempt, and uses measured remaining travel. Both the resumed portion and the
  complete logical movement are checked at completion.
- Each energized/observed movement attempt contributes at most one record.
  A cancellation before output with no edges does not invent a movement.
- The ring has **exactly ten slots**, each statically asserted to **64 bytes**.
  It overwrites the oldest record when full. Records include identity/context,
  attempt/outcome, independent tick deltas, expected/measured travel and yaw,
  residuals, confidence and diagnostic flags.
- Bias/trend calculations use valid completed residuals, count only the latest
  eligible attempt for a movement ID, and report interruptions/obstacles/encoder
  disagreement separately. An unexecuted remainder is not scored as endpoint drift.

These records are bounded RAM diagnostics, not persistent storage or a replacement
for full Protocol-3 command identities. Context hashes are diagnostic tags only.

## Relative pose, uncertainty and checkpoints

The pose frame starts at the current boot's origin, with no absolute heading.
Gyro samples are transformed into the body frame, calibrated only while stationary,
and integrated trapezoidally. Wrapped yaw plus turn accounting preserves multi-turn
deltas. Saturation, gaps, calibration generation changes and expired bias lifetime
invalidate continuity; reconnects do not silently restore it.

Eight fixed encoder snapshots bracket IMU timestamps. Fractional ticks are retained
without converting long-lived absolute counters to float. Each usable aligned
interval is consumed once. Acquisition/queue timestamps are checked rather than
assuming that the main loop and IMU worker sampled simultaneously.

Pose uses signed command direction and midpoint integration of encoder travel and
relative gyro yaw. Covariance retains the six independent planar terms, process
noise and persistent gyro-bias/wheel-scale sensitivities. Correlation/finite checks
reject invalid covariance; configured position/yaw uncertainty limits are hard
gates. **Checkpoint acceptance, route changes, cancellations, safety resets and
network reconnects do not reset covariance or pose.** Lost invalidates the estimator
and freezes its last estimate; subsequent unobserved motion is not invented.

Nominal command progression is separate from estimated motion. Endpoint acceptance
requires existing wheel/range checkpoint checks **and** CCPS terminal evidence.
An accepted checkpoint stores the estimated and nominal pose, covariance and
context without reducing uncertainty. It is a virtual observation, not independent
physical localization. No physical attestation source is wired into recovery;
`physicalRecoveryEligible()` remains false for these virtual checkpoints. There is
no autonomous corrective turn, reverse, retrace or obstacle bypass.

Single-signal unsigned encoders cannot establish physical direction, slip or
rollback. IMU yaw cannot establish absolute position. A reboot starts a new unknown
relative frame, not a validated recovery or proof that the robot is at a route start.

## Existing network protocol preserved

CCPS does not replace Protocol-3 task/session/route/step identities, command and
control IDs, independent exact tick targets, start acknowledgement, duplicate
suppression, leases, reconnect baseline handling or operator-attested arrival.
Remote completion waits for confidence settling/endpoint acceptance; settling
does not exempt a command from local safety, identity or lease checks.

Results carry an immutable 44-byte decision copy through the existing result
queues. Local diagnostics expose trust, reasons, permissions, validity bits,
relative pose, uncertainty, history count, engineering indices and maximum CCPS
slice time. Failures can include a bounded CCPS summary in the existing `error`
field. No rich confidence telemetry schema, endpoint or backend/database mutation
was introduced. HTTPS CA/hostname checks remain enabled.

## Configuration and timing limits

All production settings remain centralized in `src/AtlasConfig.h`. In addition to
the existing GPIO/electrical confirmations, effective independent counts per wheel
revolution and edge convention, track width and CW/CCW turn scales, CCPS needs:

| Configuration | Required measurement/validation |
| --- | --- |
| IMU bus pins/address/electrical confirmation | Actual LSM6DSO wiring, SA0 address, power/logic levels and non-overlapping pins. |
| `ImuNavigationParameters` | Proper sensor-to-body rotation, stationary rate/acceleration tolerance, noise limit, calibration sample count, maximum gap and measured bias-drift validity lifetime. |
| `ConfidenceCalibration.motion` | Distance, yaw, normalized wheel-progress mismatch, encoder-yaw disagreement and unwrapped excursion tolerances. |
| Encoder limits | Maximum plausible edge rate and normalized progress-mismatch scale. |
| `ConfidenceCalibration.pose` | Distance/yaw/lateral/time process noise, gyro-bias variance, left/right scale variances and maximum aligned interval. |
| Supervisor limits | Position/yaw uncertainty ceilings, warning threshold/persistence, caution duty, history bias/slope limits, route residual tolerances, alignment window, stopped verification count and recovery-attempt bound. |

The adapter derives cm/edge, track/turn calibration, local duty limits, sensor
freshness bounds, settling time, decision lifetime and stall interval from existing
configuration rather than introducing competing copies. Zero/unconfigured physical
parameters fail closed; **the synthetic test/probe values are not calibration**.
With enforcement, even a straight autonomous move requires valid turn/IMU/CCPS
configuration for the supervisor. The low-level straight-move API alone is not a
way around this gate.

- Supervisor adapter cadence: nominal **5 ms**, using the existing loop.
- Active motion assessment: nominal **10 ms** minimum interval.
- Pose propagation: normally at least **20 ms** between aligned inputs, with a
  forced terminal flush when needed. IMU acquisition/integration remains in the
  existing IMU worker; navigation polling requests **5 ms**, subject to RTOS ticks,
  bus latency and sensor data-ready. This is not a guaranteed sample frequency.
- Settling uses the existing **200 ms** checkpoint interval and requires evidence
  covering the stop, not merely a timer expiration. Decision age uses the existing
  **50 ms** maximum control gap. Local safety remains checked throughout the loop.
- The supervisor creates no task and allocates no heap in its update/history/pose
  paths. Existing worker/queue initialization and networking still allocate memory;
  a static-RAM report does not bound those allocations.
- `maximumSliceUs()` instruments the adapter with `esp_timer_get_time()`. **No
  ESP32 runtime maximum, loop jitter, heap margin or stack high-water mark has been
  measured.** Host timing is not substituted for target timing.

## Software validation

Run from `hardware/Atlas` with Python 3 and an existing C++17 compiler:

```sh
python tests/host/run.py --optimization O0 --cxx clang++
python tests/host/run.py --optimization Os --cxx clang++
```

`g++` or the path to `zig` is also supported. Verification here used temporary
Zig 0.13.0 tooling. The runner installs nothing and never connects to hardware.

| Suite | Reported checks/cases per run |
| --- | ---: |
| Navigation | 28 |
| Confidence math | 24 |
| Confidence supervisor | 43 |
| IMU integration | 19 |
| Motion confidence | 12 |
| Movement history | 14 |
| Pose estimator | 29 |
| Sensor confidence | 21 |
| Hardware-adapter integration | 22 |
| Telemetry-coordinator integration | 8 |
| **Total at each of `-O0` and `-Os`** | **220** |

These totals mix individual pure checks and scenario tests, not 220 physical or
independent end-to-end tests. The unchanged baseline's original 27 navigation tests
also passed before CCPS implementation. Pure suites compile production confidence modules.
Navigation fixtures retain real navigation/routes/checkpoints/obstacle policy with
fake hardware-facing collaborators. Adapter scenarios additionally use real motion,
safety and CCPS with GPIO/PWM/ISR inputs, sensor access, IMU access and clocks stubbed.
Telemetry scenarios use the real coordinator, but stub HTTP/TLS, Wi-Fi, queues and
task scheduling. Disposable configurations simulate calibration; production settings
and private provisioning are not altered/read by the runner.

Regression scenarios cover missing hooks/calibration, stale decisions, immediate
fault gating, no-data-ready IMU gaps, stopped uncommanded motion, synchronized
relative 90° turns, independent wheel progress, interrupted/resumed exact ticks,
late coast across reversal, ring capacity, persistent bias/covariance, Lost freeze,
checkpoint non-reset, obstacle-vs-Lost semantics, explicit reset/verification ordering,
start ACK, duplicate/identity changes, expired lease, disconnect and settling.
They do not validate electrical behavior, concurrent RTOS execution, network parser
delivery against a live server, physical navigation accuracy or stopping distance.

## Physical validation still required — not performed

This checklist is future work, **not authorization to flash or energize motors**.
First resolve the release-size gate and establish an independently reviewed,
current-limited bench procedure with a physical motor-power disconnect.

- [ ] Confirm actual GPIOs, nSLEEP/pull-downs, separate bridge outputs, motor
      polarity, electrical levels and safe boot/reset behavior with wheels lifted.
- [ ] Measure each wheel's effective edge count and noise rate using the chosen
      convention; validate forward/backward distance, coast and stall detection.
- [ ] Validate all four ranges, cross-talk, invalid/stale/stuck echoes, hysteresis
      and stop latency at the intended maximum speed/load.
- [ ] Measure track and both 90° turn scales on the intended floor/load; verify
      individual and accumulated angle, gyro axis/sign and encoder/IMU residuals.
- [ ] Measure IMU stationary noise, mounting rotation, saturation/gaps, thermal/load
      bias drift and calibration lifetime; derive process-noise/uncertainty limits.
- [ ] Validate interrupted/resumed motion including coast, forced confidence stop,
      E-stop and safety reset; `r` never resumes, and stopped `v` never clears local
      faults, Lost or accumulated uncertainty. A new explicit command is required.
- [ ] Verify covariance persists across checkpoints, route boundaries, reconnects
      and resets; confirm pushed/rotated/unobserved motion is not accepted as progress.
- [ ] Measure CCPS slice time and complete loop gaps under IMU, portal, Wi-Fi and
      TLS/result load; measure heap/largest-block and both worker/loop stack margins.
- [ ] Test actual Protocol-3 ACKs, identity/duplicate/lease/pause handling and
      operator-attested arrival without adding autonomous arrival assumptions.
- [ ] Rebuild with intended provisioning/calibration, rerun tests and measure the
      complete image against the hard release gate before any deployment review.

## Official API verification

Espressif Documentation MCP was available and consulted during implementation;
the final review rechecked timer/performance and SMP/stack guidance through MCP.
Some MCP hits were too broad to verify individual APIs; these were **not** treated
as proof. Official API pages and installed framework headers supplied those details.

- [Arduino GPIO/interrupts](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html)
  and [LEDC](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html):
  retain short argument-bearing ISR callbacks and the version-guarded pin-based
  Arduino-ESP32 3.x PWM adapter with failure checks.
- [ESP Timer](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/api-reference/system/esp_timer.html)
  and [performance measurement](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/performance/speed.html):
  microsecond timestamps/instrumentation without adding timer callbacks; scheduling
  and instrumentation overhead preclude claiming a measured deadline from compilation.
- [IDF FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/api-reference/system/freertos_idf.html):
  task affinity, short critical sections, copied queue handoffs and byte-based task
  stacks. Installed `freertos/queue.h` confirms one-slot overwrite semantics and
  immediate `xQueuePeek(..., 0)` reads. Existing IMU I/O remains isolated in its worker.

Compilation verifies the installed Arduino-ESP32 **3.3.12 / ESP-IDF 5.5.5** API
combination, not every future version or the retained Arduino-ESP32 2.x branches.
Documentation/API verification and host execution are not physical hardware tests.
