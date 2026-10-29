# Atlas CCPS — full debugging, verification and engineering audit

Date: **2026-10-27**. Scope: the actual uncommitted Atlas workspace, not only its
implementation report. **Audit only: no production repairs or threshold changes.**

Evidence labels used: **VERIFIED BY SOURCE**, **VERIFIED BY TEST**, **VERIFIED BY
BUILD**, **PROVEN DEFECT**, **SUSPECTED DEFECT**, **NOT VERIFIED**, **REQUIRES HARDWARE**.
These labels qualify specific claims, not blanket certification of the robot.

## 1. Executive Summary

**Not ready for controlled motor/navigation validation or release.** Existing
tests and builds reproduce, but additional diagnostics expose functional defects
missed by the original suite. Local safety remains independently authoritative in
the supported movement paths inspected; hard confidence loss stops/inhibits motion
in the tested cases. That does not establish physical stopping performance.

- **Six confirmed findings:** **0 P0, 3 P1, 3 P2, 0 P3**; **two suspected issues**.
- Original ten suites: **220/220 reported items at `-O0` and 220/220 at `-Os`**.
  The total mixes **162 pure CHECK executions and 58 scenario cases**, not 220
  independent end-to-end tests. Scenario assertions are additional and not counted
  by that total. Shared fixtures/model assumptions reduce independence further.
- Final audit C++ diagnostics: **21 pass / 6 fail out of 27 scenarios at each
  optimization**. One additional in-memory backend diagnostic: **0 pass / 1 fail**.
  Duplicate reproductions of the same defect are not separate defect counts.
- Main image **1,053,072 bytes**, **53,072 over** the 1,000,000-byte gate; feature
  probe **1,190,000 bytes**, **190,000 over**. Static RAM **61,604 / 63,108 bytes**.
- An independent double-precision/numerical-Jacobian covariance oracle matched the
  production result within **4.22761425e-08 absolute error** on an 80-step path.
  This verifies that experiment, not every trajectory or physical uncertainty model.
- Exact-tick coast completion and reverse-yaw sign are broken. Persistent sensor
  failure can stop without a direct Human escalation. Guidance accepts a failed
  result but loses its detailed CCPS reason; a specific admin assistance alert is
  not implemented. Live receipt/display has not been tested.

Full defect details, recommendations, complexity and required regressions are in
[CCPS_DEBUG_FINDINGS.md](CCPS_DEBUG_FINDINGS.md). No fix in that register was applied.

## 2. Audit Environment

Repository: `C:\Users\admin\OneDrive\Documents\Libris software\libris`;
firmware: `hardware/Atlas`. Git HEAD:
`3261d89e17039aab74faff41ef9ca31ffaf01d0a`. HEAD is **not** the audited implementation:
the existing modified/untracked CCPS files are part of the audited working tree.

`audit-output/2026-10-27/pre-audit-manifest.json` records initial status and SHA-256
for 72 existing source/test/config/documentation/evidence files;
`pre-audit-diff.patch` preserves the tracked Atlas diff. Audit artifacts are newly
created under `docs/`; existing firmware, tests, README, implementation/resources
documents and their size logs/reports were not rewritten. Builds update generated
artifacts/cache, not production calibration. No commit/push/reset/delete of user
changes, firmware upload, device readout, motor operation or live backend request
was performed. Private provisioning was absent; no credentials were read.

Environment independently cross-checked using build logs, installed version headers
and compiler invocations: Espressif32 **8.0.0**, Arduino-ESP32 **3.3.12**, ESP-IDF
**5.5.5**, Xtensa GCC **14.2.0**, ArduinoJson **7.4.3**, PlatformIO Core **6.2.0**.
Host testing uses existing temporary **Zig 0.13.0**, C++17 with
`-Wall -Wextra -Werror`, at `-O0` and `-Os`. Production compilation uses its actual
framework language flags and `-Os`; host C++17 is not an identical ABI/toolchain.

The existing `esp32dev` targets classic ESP32, 240 MHz, configured 4 MB flash.
`platformio.ini:5,8-10` selects the existing partition CSV and `-Os`. No framework
migration, dependency addition or optimization experiment was applied in this audit.
Builds ran through PlatformIO MCP with explicit forward-slash project paths and
`forceExecution=true`; a verbose follow-up confirms `-Os` on the actual compiler
command. These were normal incremental builds, **not clean-room toolchain installs
or full clean rebuilds**. Generated image hashes and sizes reproduce exactly.

Official references consulted during this audit:

- Espressif Documentation MCP retrieved [IDF FreeRTOS task/queue documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos_idf.html):
  copied single-slot overwrite, non-ISR queue use, zero-wait peek and byte-based
  pinned-task stacks. This supports the ownership review, not measured deadlines.
- Installed ESP-IDF version header and SDK config confirm **5.5.5**, **1,000 Hz**
  FreeRTOS tick and Arduino loop **core 1** for the inspected DIO SDK configuration.
- ST's official [LSM6DSO register header](https://github.com/STMicroelectronics/lsm6dso-pid/blob/master/lsm6dso_reg.h)
  and [conversion implementation](https://github.com/STMicroelectronics/lsm6dso-pid/blob/master/lsm6dso_reg.c)
  corroborate WHO_AM_I **0x6C**, register fields, ODR encoding 4 = **104 Hz**, and
  conversion factors **8.750 mdps/LSB** and **0.061 mg/LSB**. These are upstream
  reference sources, not a physical chip/mounting identification test.

## 3. Implementation Inventory

| Module | Actual responsibility and audit classification |
| --- | --- |
| `ConfidenceTypes.h:8-112` | Finite/clamp/wrap/freshness helpers, score/reason/permission enums, fixed records and execution hooks. VERIFIED WITH LIMITATIONS: 16-bit record reasons cannot represent every 32-bit decision reason; full decisions retain their mask. |
| `SensorConfidence.cpp:3-51` | Validity × quality × freshness; invalid/stale evidence rejected by normal producers; encoder rate/no-progress/mismatch. VERIFIED WITH LIMITATIONS; central malformed-assessment handling is A06. |
| `ImuIntegration.cpp:3-51` | Proper mounting rotation, stationary Welford bias/noise calibration, finite lifetime, trapezoidal relative yaw and turn accounting; permanent continuity break on invalidation. VERIFIED WITH LIMITATIONS; S01 acquisition concern. |
| `MotionConfidence.cpp:3-29` | Hard residual/continuity/fault checks plus diagnostic confidence; partial/interrupted semantics. DEFECT FOUND in upstream expected-yaw construction (A02). |
| `MovementHistory.cpp:3-31` | Exactly ten chronological records, replacement and weighted latest-attempt completed residual statistics. VERIFIED WITH LIMITATIONS; history is not an independent physical localization source. |
| `PoseEstimator.cpp:3-54` | Midpoint relative pose, random covariance, persistent bias/scale sensitivity covariance and PSD/finite checks. VERIFIED WITH LIMITATIONS by numerical experiment, not calibrated physical accuracy. |
| `ConfidenceSupervisor.cpp:3-272` | Evidence alignment, lifecycle/history, pose limits, trust/clearance/health, warnings, constraints and checkpoint/recovery policy. DEFECT FOUND: A01–A03/A06; other gates partially verified. |
| `ConfidenceIntegration.cpp:4-77` | Existing-controller adapter, snapshots, context/lifecycle hooks, permission application, slice instrumentation. PARTIALLY VERIFIED; no concurrent target test. |
| `MotionController.cpp:17-295` | GPIO/PWM, unsigned encoder ISRs, nonblocking travel/turn/exact ticks, resume and stop/event snapshots. DEFECT FOUND: terminal exact-tick contract A01. |
| `SensorManager.cpp:6-136` | Real interrupt echo capture and staggered acquisition; immediate invalidation, range/time checks. VERIFIED BY SOURCE/BUILD; real acquisition NOT TESTED by host suite. |
| `ImuManager.cpp:8-132` | Existing isolated Wire worker, identity/config/readback, DRDY poll, copied reading with integrated navigation evidence. VERIFIED BY SOURCE/BUILD; actual I2C acquisition NOT TESTED. |
| `SafetyController.cpp:5-142` | Independent local faults/latches and final conjunction with fresh supervisor constraint. VERIFIED WITH LIMITATIONS; electrical stopping REQUIRES HARDWARE. |
| `Navigation`, `Routes`, `CheckpointManager`, `ObstacleAvoidance` | Existing execution policy and local endpoints with new CCPS gates. DEFECT FOUND in exact-tick endpoint integration; original policy scenarios pass. |
| `TelemetryManager`, `BackendClient` | Worker/main copied handoffs, existing protocol checks, diagnostic result copies and bounded error text. PARTIALLY VERIFIED; HTTP/parser/worker execution not covered by host coordinator stubs. |
| `NetworkManager` | Existing core-0 Wi-Fi/portal/profile worker ownership and stop-before-flash approvals. VERIFIED BY SOURCE/BUILD only in this audit. |
| `Atlas.ino:114-162`, `AtlasConfig.h` | Real composition/scheduling and explicit unconfigured physical parameters; CCPS enforcement is wired, not just unused classes. VERIFIED BY SOURCE/BUILD. |

## 4. Architecture Verification

**VERIFIED BY SOURCE:** encoder ISR → locked snapshots → adapter; echo ISR →
`SensorManager::Reading` → local safety and sensor confidence; Wire worker →
`ImuIntegration` → one-slot copied queue → adapter → supervisor alignment/motion/
history/pose → `Constraint` → `SafetyController` → motion permit/duty ceiling.

`Atlas.ino:152-159` actually calls supervision, copies diagnostics for telemetry,
updates navigation/remote dispatch, reapplies the constraint, checks local safety,
then calls the motor update. `ConfidenceIntegration::begin()` installs both motion
lifecycle observation and navigation/routes execution hooks. A missing required
execution hook denies starts. No new CCPS task is created.

The final motor permit is local safety **AND** a fresh, adequate supervisor
constraint, bound to the active movement ID. Starts additionally require the
navigation/routes gate, and remote starts require existing protocol identity,
acknowledgement and lease rules. The low-level public motion API is not a new
autonomous command source in the inspected sketch. Unsupported arbitrary external
calls that reorder these APIs were not certified safe.

Sensor, motion, history, relative pose, uncertainty, checkpoint status, clearance
and health all participate in actual processing. Scores are diagnostic engineering
indices, not probabilities. `NavigationScore` is not simply `SensorScore`: hard
sensor readiness gates are separate, and a displayed navigation index alone must
never be interpreted as movement permission. Malformed assessment handling A06
limits the claimed central finite-input contract.

## 5. Mathematical Verification

### Sensor confidence

`C=V·Q·F` is explicitly implemented. Normal producer scores are clamped into [0,1];
negative/non-finite quality is unusable. Freshness is 1 through grace, exponential
after grace, and 0 when `age > hardMs`; the exact hard-age boundary is inclusive.
Invalid freshness parameters return 0. IMU future timestamps are rejected before
age subtraction. Ultrasonic ages use modular milliseconds; required intervals are
short relative to rollover. Encoder deltas use modular subtraction with plausible
rate and half-rollover interval checks.

**VERIFIED BY TEST:** existing sensor/math checks plus an audit sweep of eight
qualities across six age boundaries, and simultaneous timestamp/count wrap.
This sweep is one audit scenario, not 48 independent end-to-end tests. Normal
minimum aggregation cannot let healthy ranges compensate for a failed required
range: `usable=false` independently clears readiness. A06 shows NaN can be hidden
if that pre-assessment contract itself is inconsistent.

There is no detailed central enum distinguishing every requested sensor category.
For example, an idle encoder can be plausibly valid without having recently
produced an edge; this is not proof that a disconnected encoder works. No-progress
and the independent local stall policy cover commanded motion, with finite latency.
Range changes are not automatically classified as noise or localization failure.

### Motion and signs

Distances are centimetres; yaw is radians. Clockwise is negative yaw, CCW positive;
forward/backward signs are command-derived, never inferred independently from
unsigned pulses. Distance residual is deferred to completed, non-interrupted
motion; in-progress yaw target scales with observed phase. Hard unwrapped excursion
and encoder/gyro disagreement reject whole-turn aliases, even when wrapped yaw
alone matches. Active checks run on the nominal 10 ms evaluation cadence.

**VERIFIED BY TEST:** correct nominal forward/turn/resume scenarios and deliberate
multi-turn disagreement rejection. **PROVEN DEFECT A02:** reverse asymmetry uses
unsigned expected-yaw construction. Equal reverse targets do not expose that bug.
Both misleading equal encoder counts with unchanged gyro yaw can still represent
slip or wrong-way straight travel; this sensor arrangement cannot disambiguate it.

### IMU integration and uncertainty meaning

The mounting matrix must be finite, orthonormal and determinant +1. Calibration
requires the supplied stationary hint, rate limits and acceleration magnitude near
1 g. Welford sample variance gates calibration; body-Z mean is subtracted for yaw.
The integration uses `(previousRate+rate)/2·dt`. Repeated/nonmonotonic sample times,
large gaps, saturation, invalid values and expired lifetime break continuity.
`broken_` prevents silent recovery; there is no autonomous reinitialization.
Wrapped yaw plus an integer turn count preserves multi-turn deltas. IMU-derived
bias/yaw variances are validated, but pose uncertainty uses separately configured
model/process bounds; measured variance is not automatically a physical error guarantee.

**VERIFIED BY TEST:** the existing 19 checks cover identity mounting, bias/rate
integration, repeated time, invalid mounting, gaps, noise, saturation and expiry.
They do not execute the real Wire worker or establish mounting/temperature drift.
Overwritten queue observations are not reintegrated: worker evidence is cumulative,
and supervisor pose consumption uses sequence/time. Lost *raw acquisition* before
the worker can integrate it remains the separate S01 concern.

### History

`Capacity=10`; modulo indexing returns oldest-to-newest, with the eleventh push
replacing the oldest. Empty/no-eligible history is invalid, not ten successes.
For eligible latest completed attempts, weights are chronological `i+1`, biases
are weighted signed tolerance-normalized residuals, and trend is a weighted
least-squares slope of residual magnitude for at least three eligible records.
Interruption/obstacle/disagreement counts are separate. A newer attempt, even an
invalid/interrupted one, suppresses an older same-ID residual in the window.

**VERIFIED BY SOURCE/TEST:** fixed storage, 64-byte records, rollover, invalid record
exclusion, signed under/overshoot biases, heading trend, latest-attempt suppression.
Statistics intentionally do not make a high history index override a hard current
fault. History overwriting does not touch the separate pose estimator.

### Pose, Jacobians and covariance

For signed incremental distance `d`, relative yaw `a` and heading `θ`:

`x'=x+d cos(θ+a/2)`, `y'=y+d sin(θ+a/2)`, `θ'=wrap(θ+a)`.

The state Jacobian has `F02=-d sin(φ)`, `F12=d cos(φ)` and unit diagonal;
the distance/yaw input Jacobian has columns `(cosφ,sinφ,0)` and
`(-d sinφ/2,d cosφ/2,1)`, where `φ=θ+a/2`.
`PoseEstimator.cpp:33-49` propagates random `F P Fᵀ + G Q Gᵀ`, adds lateral noise,
and propagates persistent gyro-bias/wheel-scale sensitivities. Final covariance
adds `J diag(variance) Jᵀ`. Bias sensitivity accumulates elapsed time, so it is
not reset to independent white noise at each checkpoint. Wheel-scale uncertainty
affects translation in this gyro-heading model; it is not a second encoder-heading
measurement update. There is no independent absolute localization update.

**VERIFIED BY TEST:** audit double-precision oracle numerically differentiates the
state/input model and the entire calibration-perturbed trajectory independently
of the production recurrences. Across 80 varied signed-wheel/yaw steps, all six
covariance components match within **4.22761425e-08** maximum absolute difference
at both optimizations. Stationary equal-duration 20/40 ms propagation agrees within
the diagnostic tolerance. Repeating the consumed sequence leaves covariance unchanged.
Original tests cover straight/reverse/in-place turns, wrap, invalid covariance and
persistent bias growth. This is not exhaustive proof for extreme float ranges.

**VERIFIED BY SOURCE:** covariance checks finite values and approximate PSD via
scaled correlations/principal minors. Position ceiling uses the largest 2-D
position eigenvalue; heading ceiling uses yaw variance. Checkpoints, route changes,
pause/resume, safety reset and network reconnect have no pose/covariance reset call.
Lost invalidates/freezes the estimator. Source verification covers those reset
boundaries; separate complete runtime tests for every boundary were not executed.

## 6. Safety Verification

`SafetyController.cpp:41-65,68-142` keeps critical local latches/reset authority.
`applyPermit()` uses `localPermit_ && !latched_ && supervisorAllows(now)`, caps duty,
stops before withdrawing the interlock, and does not relabel a CCPS stop as E-stop.
Decision age, movement ID and minimum usable duty are checked. Critical latch
handling cannot be reversed by a high score.

| Movement origin | Inspected gating |
| --- | --- |
| Manual forward/reverse/90° | `Navigation.cpp:26-39`: safety, valid segment, required execution gate, motion start. |
| Local recording | Same manual start gate; capacity/checkpoint acceptance before record. |
| Local replay | `Routes.cpp:80-113`: context/index gate before launch/resume, endpoint gate before validation/advance; low-level motion still requires safety. |
| Remote/manual backend | Start ACK/lease/identity then the same Navigation APIs. |
| Protocol-3 exact replay | `Navigation.cpp:77-88` plus remote identity and exact targets; terminal coast contract is A01. |
| Resume | `Navigation.cpp:138-158` / `Routes.cpp:85-96`: safety, dwell and renewed execution permission. |

**VERIFIED BY TEST:** E-stop wins over allow, sensor/stall/target/PWM/control-gap
stops, duty limiting, missing hooks, stale constraints and Lost-reset refusal in
existing/additional scenarios. No supported start path bypassing both authority
layers was demonstrated. Public APIs and cross-core memory corruption are not
certified by those tests. nSLEEP, bridge polarity/current and actual stop distance
remain **REQUIRES HARDWARE**.

## 7. Failure and Warning System

The following distinguishes internal detection, physical output intent, diagnostics,
server handling and actual administrative notification. A numeric `Human` bit is
not an end-to-end alarm.

| Failure class | Actual source/test response |
| --- | --- |
| Confidence collapses to zero | Required evidence becomes unusable; active IMU-quality collapse enters Lost/denies motion. Low-but-positive scores instead warn/cap duty according to policy, not a universal probability threshold. |
| Required range fails | Local `Sensor` stop/latch, CCPS inhibit/hold; not automatically Lost. Persistent assistance gap A03. |
| IMU unavailable | Missing/discontinuity, Lost, Human, fresh constraints denied; existing and audit coordinator cases stop. |
| Encoder inconsistent/implausible | PulseRate/Discontinuity/UnexpectedMotion hard gates; tested excessive rate and stopped push enter Lost. No pulses is bounded no-progress/stall, not instantaneous direction proof. |
| Uncertainty exceeds limits | Uncertainty, Lost, Human; pose freezes and starts remain denied. Tested with a tight synthetic ceiling. |
| Motion/gyro disagreement | EncoderImu/Heading hold, stop; terminal unacceptable usable evidence can require Human. Reverse false-positive A02 remains. |
| Repeated checkpoint rejection | Existing bounded range re-observation eventually fails navigation; supervisor holds Checkpoint/Verify. It does not automatically fabricate absolute location or universally set Lost/Human. |
| Persistent history errors | Weighted bias/trend and motion warning persistence can reduce duty and label History. There is no independent history-only physical recovery or universal history-only stop/human deadline. |
| Stale evidence | Readiness/permission denied; active IMU continuity loss becomes Lost. Stale constraints independently inhibit output. |
| Invalid configuration | Startup fails closed; active evidence invalidity inhibits/holds, while independent critical configuration faults remain local safety. Tested synthetic central invalidity; real defaults unconfigured. |

The additional `failure_matrix` runs ten explicit failure-response scenarios plus
signed-history checks; all **11 pass** at both optimizations. That verifies the
asserted responses, not an assertion that every desired escalation exists.

### Warning chain, verified separately

1. **Internal detection — VERIFIED BY SOURCE/TEST with limitations.** Lost sets
   Human and denies verification/restart. A03 demonstrates incomplete escalation
   for persistently unavailable required-sensor evidence.
2. **Local action — VERIFIED BY SOURCE/TEST, REQUIRES HARDWARE physically.** Hard
   confidence loss stops simulated outputs before network delivery. A safety reset
   cannot clear Lost. Wi-Fi reconnect alone does not restore position.
3. **Diagnostic reporting — VERIFIED BY SOURCE, partly BY TEST.** Serial pages expose
   numeric trust/clearance/health, permissions, reasons, scores and pose. Pages rotate
   at one nominal second per page; a particular page can recur only about every
   eleven seconds, and lines can be dropped for TX capacity. This is not a latched
   immediate human-readable alarm. Active command results freeze a decision copy;
   failed serialization adds bounded reason hex/trust/clearance to existing `error`.
   The coordinator's immutable Lost/Human result copy is tested; actual HTTP
   serialization/transport is not executed by those coordinator tests.
4. **Backend receipt/persistence — VERIFIED BY SOURCE only; live receipt NOT VERIFIED.**
   Guidance accepts failed results and commits a **generic** task failure reason;
   A04's AST-extracted in-memory test proves detail is discarded in that branch.
   Manual `recordings.py:39-83` stores results in process-local runtime structures,
   not a durable general CCPS event store. Current default credentials disable
   real backend requests. There was no live delivery or database test.
5. **Admin notification — incomplete / actual display NOT VERIFIED.**
   `GuidanceMonitor.vue:21` can show failure history text while its 2-second polling
   page is open. There is no inspected CCPS reason decoder, dedicated assistance
   state/banner/push notification, or guaranteed acknowledgement workflow. The
   requested meaningful "confidence lost / route suspended / human required /
   pose uncertainty" warning is **not implemented end to end**. Generic failure
   history is not equivalent to such a notification.

No active remote command means there is no CCPS failure result automatically queued;
ordinary telemetry sends only its existing supported fields. Failure queue handoff
is zero-wait; HTTP/retry work stays in the network worker. A terminal result may
wait for settling even though output has already stopped. Queue overflow, missing
UTC or exhausted delivery is an inhibit, not a delivery guarantee.

## 8. State Machine Verification

Trust, clearance and health are distinct enums. **BLOCKED ≠ LOST** is borne out by
the normal obstacle and sustained obstacle tests: outputs stop, clearance is Blocked,
and trust is not automatically Lost. Pose uncertainty is retained, not cleared.
An obstacle stop can remain latched and need explicit reset plus separate authorized
resume. `ObstacleAvoidance.cpp:17-31` requires sustained all-sensor clearance and
has a bounded wait/interruption budget; it commands no bypass motion.

Lost is internally sticky: `lose()` invalidates the pose; `verify()` explicitly
refuses Lost. There is no reset method that fresh ranges or reconnect can invoke
to restore physical localization. For non-Lost holds, stopped verification requires
fresh admissible samples/local safety, and never commands movement itself. The
sensor-failure case exposes a Verify state with no currently permitted verification
and no direct Human flag (A03), so state names alone are insufficient.

Recovery eligibility checks an independently verified checkpoint, but that flag
is never set true by the virtual-checkpoint path. Recover/Correct permissions are
never granted; the recovery counter has accounting methods, not an autonomous
backtracking executor. This is intentionally **NOT IMPLEMENTED** physical recovery,
not a claim that ten records justify retracing. Network recovery/restart is separate
and cancels motion; reboot establishes a new unknown relative frame.

## 9. Integration Verification

Original navigation scenarios and real-control/coordinator fixtures reproduce
recording/replay, pause/resume/cancel, reset ordering, local checkpoint progression,
start ACK, lease expiry, duplicates, changed target identity and Wi-Fi cancellation.
The new diagnostics additionally reproduce A01/A02 and verify Lost survives reset
and reconnection. They do not exercise a live route delivery/arrival exchange.

**VERIFIED BY SOURCE:** Protocol-3 UUID/task/session/route/sequence/control checks
remain. Exact independent targets are passed unchanged to motion; CCPS caps duty
or denies movement rather than rewriting requested ticks. A01 is an acceptance
failure after coast, not target mutation. The backend compares the measured result
against the stored movement exactly and would fault that inconsistent completion.

`BackendClient.cpp:179-245` parses guidance motion/tick/control metadata. It does
**not** receive a per-step expected ultrasonic signature or absolute pose/covariance
reference. Remote replay's manual checkpoint path checks local settled ranges and
odometry, not a fabricated backend destination fingerprint. Local recorded replay
does carry its own saved endpoint ranges (`Routes.cpp:103-110`).

`pollGuidance()` posts final arrival only for stopped, operator-attested matching
task/session/route/checkpoint metadata. It never infers arrival from confidence.
`backend/guidance.py:603-641` additionally checks completed progress, recent stopped
poll and current attestation. Those endpoints were read, not called. No automatic
OTP/display hardware or absolute localization capability was established.

## 10. Timing and Concurrency

**VERIFIED BY SOURCE:** main-loop ownership for motion/navigation/supervision;
short encoder/echo ISR locks; no confidence math in an ISR. Each encoder is locked
separately, so left/right snapshots are not perfectly simultaneous; captured span
is tracked and alignment rejects oversized spans. Lock sections do not wait on
queues or perform networking. No inspected lock-order cycle establishes a deadlock.
That source review is not a thread-race detector or RTOS stress test.

IMU and network workers are both priority 1/core 0, with requested **4,096-byte**
and **16,384-byte** stacks. Arduino loop is configured core 1. IMU uses one copied
slot and an atomic stationary hint; network handoffs use copied queues/atomic flags.
`xQueuePeek(...,0)`/nonblocking sends bound the main-loop handoff, not total wall
clock latency. Preemption, cache/flash stalls, Wire and framework locks still matter.
Profile writes wait for main-loop cancellation/approval before EEPROM commit.

Nominal control/adapter 5 ms, motion confidence 10 ms, pose 20 ms with terminal
flush, 200 ms settling, 50 ms constraint/control-gap bound, ultrasonic 60 ms slot
(240 ms scan) and IMU 104 Hz ODR/5 ms polling are **configuration/intended cadence**,
not measured execution deadlines. Current SDK tick is 1 ms, but actual jitter is
unknown. Stale DRDY is not refreshed as if it were a new measurement. Raw dropped
sample and adverse alignment ordering concerns are S01/S02.

`maximumSliceUs()` uses monotonic ESP Timer instrumentation, but no ESP32 serial
runtime was collected. **NOT VERIFIED:** CPU time, worst-case latency, heap/largest
block, both worker stack margins, ISR latency, network-load jitter and bus dropout
recovery. Host timing is not substituted. No sanitizer/coverage/race-instrumented
target run or exhaustive model checker was executed; static analysis here is
manual source/contract review plus compiler warning checks.

## 11. Test Results

All commands below run from `hardware/Atlas`; compiler path used:
`C:\Users\admin\AppData\Local\Temp\opencode\atlas-host-tools\ziglang\zig.exe`.

```powershell
python tests/host/run.py --optimization O0 --cxx "C:\Users\admin\AppData\Local\Temp\opencode\atlas-host-tools\ziglang\zig.exe"
python tests/host/run.py --optimization Os --cxx "C:\Users\admin\AppData\Local\Temp\opencode\atlas-host-tools\ziglang\zig.exe"
python docs/audit-only/run_diagnostics.py --optimization O0 --cxx "C:\Users\admin\AppData\Local\Temp\opencode\atlas-host-tools\ziglang\zig.exe"
python docs/audit-only/run_diagnostics.py --optimization Os --cxx "C:\Users\admin\AppData\Local\Temp\opencode\atlas-host-tools\ziglang\zig.exe"
python docs/audit-only/backend_reporting_probe.py
```

| Original suite | Unit of reported total | O0 | Os |
| --- | --- | ---: | ---: |
| Navigation | scenarios | 28/28 | 28/28 |
| Confidence math | CHECK executions | 24/24 | 24/24 |
| Supervisor | CHECK executions | 43/43 | 43/43 |
| IMU integration | CHECK executions | 19/19 | 19/19 |
| Motion confidence | CHECK executions | 12/12 | 12/12 |
| History | CHECK executions | 14/14 | 14/14 |
| Pose | CHECK executions | 29/29 | 29/29 |
| Sensor confidence | CHECK executions | 21/21 | 21/21 |
| Hardware-adapter integration | scenarios | 22/22 | 22/22 |
| Telemetry coordinator | scenarios | 8/8 | 8/8 |
| **Mixed total** | **162 checks + 58 scenarios** | **220/220** | **220/220** |

Original tests exit **0**. The name "hardware integration" does not mean physical
hardware: GPIO/PWM/interrupt delivery, sensor access and IMU access are test doubles.
Telemetry uses the real coordinator but fake Wi-Fi, queues/task creation, UTC and
BackendClient functions. Its network worker never runs. `ATLAS_HOST_TEST` exposes
a friend harness, not an extra production executable gate. Navigation-only tests
also substitute the motion/safety collaborators. Synthetic calibration and bypassed
pin predicates exist only in disposable test copies; they do not validate real
configuration predicates/electrical behavior.

| Final audit suite | O0 pass / fail | Os pass / fail |
| --- | ---: | ---: |
| Pure diagnostic scenarios | 7 / 2 | 7 / 2 |
| Failure matrix/history scenarios | 11 / 0 | 11 / 0 |
| Real-stack adapter scenarios with original hardware doubles | 2 / 3 | 2 / 3 |
| Coordinator scenarios with original transport doubles | 1 / 1 | 1 / 1 |
| **C++ total (27 scenarios per configuration)** | **21 / 6** | **21 / 6** |

Backend AST diagnostic: **0 pass / 1 fail**, run once, optimization not applicable.
Audit runners intentionally exit **1** because invariant violations remain. The
six C++ failing scenario names are `reverse_unequal_target_sign`,
`nonfinite_public_assessment`, `exact_tick_coast_rejection`,
`reverse_asymmetric_ticks`, `persistent_sensor_human`, `backend_coast_reject`.
Both optimization levels produce the same outcomes. No optimized-only defect was
observed; undefined behavior and concurrent races are not thereby ruled out.

Raw evidence: `audit-output/2026-10-27/host-O0.log`, `host-Os.log`,
`final-*-O0.log`, `final-*-Os.log`, `final-diagnostics-O0.json`,
`final-diagnostics-Os.json`, `backend-reporting.log`. Full compiler commands are
inside each final diagnostic log. Initial attempts are preserved: an audit-helper
`main` rename caused a harness-only missing-return compile error, and an initial
coordinator assertion read a failure result before its terminal settling interval.
Only audit files were corrected; neither is counted as a production defect, and
no existing assertion was weakened.

Missing original coverage exposed by this audit: post-completion exact-tick coast,
asymmetric reverse sign, malformed pre-assessments, persistent-sensor assistance,
and backend reason preservation. The original pose finite-difference checks only
compare two derivative expressions, rather than the full production covariance;
the new oracle adds that check. Remaining gaps include real acquisition, parser/
TLS framing, queue allocation failures/overflow/retry exhaustion, exhaustive reset/
route-boundary covariance tests, real concurrency and admin rendering/notification.

## 12. Memory and Binary Size

**VERIFIED BY BUILD and actual file inspection**, not just historical report:

| Artifact | Complete binary | Application sections | Static RAM | Excess >1,000,000 |
| --- | ---: | ---: | ---: | ---: |
| Main | **1,053,072** | **1,037,976** | **61,604** | **53,072** |
| Existing inert feature-retaining probe | **1,190,000** | **1,172,171** | **63,108** | **190,000** |

Main SHA-256: `b5b70ce15b53a6cd7a0349c252efc4076a7a34c2aa1ed885e0b48645edf322c8`.
Probe SHA-256: `03972f5376931552d30bc23f4fdcaefc18d9a467d566551d4af2fd588954f55f`.
Both hashes reproduce the reported artifacts. The probe is the existing temporary
`atlas-ccps-probe-features-release-review`, not a new Atlas project. Thirty-eight
non-configuration source files match main byte-for-byte. Synthetic pins/calibration/
invalid dummy provisioning and explicit original-entry-point linker roots retain
paths; actual probe entry points only delay. Never deploy that probe.

Main is **not a provisioned production-functionality image**. Symbol inspection
shows CCPS update, pose advance and IMU integration/worker code are linked in both
images; their presence does not mean main can acquire valid hardware evidence.
Main `MotionController::begin` is only **29 bytes** versus **280** in the probe;
main `BackendClient::configured` is **10 bytes**, and `HTTPClient::GET/POST` and
`BackendClient::request` are absent from its symbol list but present in the probe.
Allocated HTTPClient/TLS/X509 groups are absent in the main attribution. Thus the
size difference demonstrably includes reachable motor/HTTPS feature retention,
not merely added credentials or an assertion that all IMU code was removed.

Ranked unique allocated map inputs (approximate attribution, not removable budgets):

| Rank | Group | Main bytes | Probe bytes |
| ---: | --- | ---: | ---: |
| 1 | Wi-Fi `libnet80211.a` | 144,725 | 144,223 |
| 2 | lwIP | 119,286 | 119,504 |
| 3 | Shared merged strings | 103,906 | 133,284 |
| 4 | libc | 84,457 | 88,930 |
| 5 | mbedcrypto | 69,693 | 88,994 |
| 6 | Wi-Fi `libpp.a` | 68,430 | 67,690 |
| 7 | WPA supplicant | 61,839 | 62,167 |
| 8 | Entire Atlas application | 61,496 | 75,670 |
| 9 | PHY | 43,526 | 43,558 |
| 10 | Arduino framework | 26,112 | 29,425 |

Ranks above are by main image; probe strings/crypto reorder its ranking. Probe-only
or larger groups include mbedTLS **32,714**, HTTPClient **13,009**, X509 **7,238**,
NetworkClientSecure **5,257** bytes and LEDC growth **4,360** bytes. Shared strings
increase **29,378** bytes. Exact complete-image difference is **136,928** bytes;
do not sum approximate overlapping/archive attribution as a substitute for that.

`analyze_size.py` excludes debug/noload inputs and padding, deduplicates overlaps
and separates merged strings; it charges other overlaps to their first map input.
The map is suitable for ranked investigation, not perfect original string ownership.
There is no evidence to attribute the whole shared pool to GPIO. `--gc-sections`
is present in framework linker flags, function/data sections are enabled, and
feature-dependent symbols/archives disappearing demonstrate effective elimination.
No whole-archive or unused-code removal guarantee was established.

The 3 MiB factory partition is unchanged and explains ordinary build success; it
does not satisfy A05. Size scripts use actual binary length. Existing milestone
CSV/reports were left untouched; fresh audit measurements are separate JSON files.
Static RAM excludes runtime stacks/queues/Wi-Fi/TLS allocations. Ten records occupy
640 payload bytes; decision is statically 44 bytes and host supervisor 1,760 bytes.
No peak heap/stack/CPU/flash-runtime measurement was fabricated.

## 13. Defect Register

Authoritative detailed register: [CCPS_DEBUG_FINDINGS.md](CCPS_DEBUG_FINDINGS.md),
with IDs, severity, exact references, expected/actual behavior, triggers, reproductions,
root cause, recommendation, complexity and regressions for every confirmed finding.
Machine summary: [CCPS_DEBUG_FINDINGS.json](CCPS_DEBUG_FINDINGS.json).

Confirmed **A01/A02/A04 are P1**; **A03/A05/A06 are P2**. Suspected **S01/S02** have
potential P1/P2 impact respectively and remain unconfirmed as target defects.
A06 is specifically a public-evidence boundary defect; the production producer's
normal finite-value protection was not shown bypassed by an actual sensor event.
No priority is inflated merely because a synthetic test failed.

## 14. Missing Functionality

**Implementation/integration omissions:** reliable detailed CCPS assistance
preservation/admin notification, unsolicited local/idle confidence-failure event
delivery, direct persistent-missing-sensor escalation, exact tick validation through
settling, complete malformed-assessment rejection, and independent localization.
The first items are defects/gaps; independent localization, display/OTP hardware
and physical backtracking are not claimed implemented and must not be inferred.

**Hardware validation, not missing software calibration values to invent:** actual
wiring, edge convention/effective CPR, track/turn calibration, mounting, noise/bias
drift lifetime, process-noise limits, stopping behavior and runtime budgets. Delivered
unknowns remain zero/unconfigured; changing them was outside the audit.

### Documentation accuracy — suggested corrections, not applied

- Reported image/RAM hashes, test mixed total, physical-test disclaimers and
  no-new-CCPS-task statements reproduce. Resource gate remains accurately failed.
- "Exact independent wheel-tick replay preserved" needs the terminal-coast A01
  exception; correct nominal tests alone do not justify the unqualified claim.
- Expected signed motion needs the A02 asymmetric reverse limitation.
- Human-required descriptions need A03's unavailable-evidence exception and a clear
  distinction between internal flags, generic stored failure and real admin alerts.
- "No bridging missing intervals" must be limited to **detected** timing/bus/invalid
  continuity breaks; S01 raw acquisition loss remains unverified.
- The full finite-value claim needs A06's central pre-assessment qualification.
- README's firmware success/27-test-era material is explicitly historical; preserve
  that distinction. Backend `robot.py:151-152` still reports `imu_status='not_implemented'`;
  that API value is not a reliable statement of new firmware IMU readiness.
- "Milestones complete" should mean delivered code/evidence, not defect-free,
  physically validated or release accepted. No document was silently rewritten.

## 15. Optimization Opportunities

No optimization or repair was applied. Rankings are investigation priorities, not
guaranteed savings or permission to remove mandatory behavior.

| Category | Candidate and supporting evidence |
| --- | --- |
| Safe direction / likely useful | Inspect duplicate application literals, bounded error/JSON formatting and linked formatting helpers while preserving diagnostics. Main shared pool is 103,906 bytes; not all strings are application-owned or safely removable. Re-measure each change. |
| Worth experimenting with | Compare equivalent serialization/HTTP use and linker reachability with real provisioning; probe restores 45,209 attributed TLS/X509/secure-client bytes, adds 19,301 crypto bytes, and restores 13,009 HTTPClient bytes. Keep CA/hostname validation and protocol behavior intact. |
| Worth experimenting with | Audit optional prebuilt debugging/coredump/unused SDK components for actual call reachability. No claim that apparent "debug" libraries are unused or can be removed safely; data preservation/diagnosability matters. |
| Risky / separate approval | Compatible LTO toolchain or rebuilt SDK libraries/configuration. Existing documentation records a failed LTO experiment; it was not rerun or repaired here. Binary/ABI/security/real-time regression work is required. |
| Unacceptable | Disable TLS verification, remove safety/CCPS gates, weaken exact targets/identity/lease checks, leave credentials/calibration disabled merely to claim size compliance, or equate partition capacity with the project gate. |

Current lower bound on required reduction is **53,072 bytes for main**, **190,000
for the probe**. Intended production provisioning/calibration may change both;
neither is a certified upper bound. Fixing correctness can also increase size.

## 16. Hardware Validation Requirements

**REQUIRES HARDWARE — none performed in this audit.** Before controlled motor tests,
repair/retest high-priority findings, establish the intended release/bench policy,
resolve configuration, and approve a current-limited wheels-lifted procedure with
a physical motor-power disconnect. This report grants no flashing/motor authority.

- Identify actual module/GPIO/nSLEEP/pull-down behavior and safe boot/reset/PWM
  failure outputs; verify independent bridge outputs, motor polarity and current.
- Measure effective independent CPR and edge filtering/noise with the selected
  convention; verify missing pulses, one-wheel stall, coast and exact-target braking.
- Validate ultrasonic electrical levels, ordering, timing, cross-talk, stale/no-echo/
  stuck-high, hysteresis and swept-footprint clearance at intended speed/load.
- Confirm LSM6DSO identity/mounting, proper axis/sign, stationary/noise calibration,
  warm-up/thermal drift, saturation and DRDY/read/queue timestamps under network load.
- Measure physical distance, forward/reverse, both calibrated 90° directions,
  partial-turn resume, asymmetric exact ticks, stopping distance and repeatability.
- Stress main/worker timing, flash-write stop approval, I2C dropouts, packet/result
  delays, heap/largest-block and all relevant stack margins. Resolve S01/S02 with traces.
- Only in a separately authorized test environment, validate backend persistence,
  retries/duplicates/late faults, final attestation and administrative assistance UI.

## 17. Release Readiness

| Dimension | Verdict |
| --- | --- |
| Software completeness | **Not complete for dependable operation**: six confirmed defects/gaps; physical recovery intentionally disabled. |
| Mathematical correctness | **Partially verified**: tested covariance/Jacobians and normal primitives pass; reverse expected-yaw sign fails. Physical noise model uncalibrated. |
| Host-test verification | Original tests reproduced; additional tests fail reproducibly. Not proof of safety or concurrent correctness. |
| Firmware compilation | **VERIFIED BY BUILD**, main/probe success using installed environment; not a clean-room rebuild or hardware run. |
| Binary size compliance | **FAIL**, +53,072 / +190,000 bytes. Production-provisioned size unknown. |
| Runtime timing verification | **NOT VERIFIED / REQUIRES HARDWARE**; instrumentation exists, no profiling measurements. |
| Physical navigation validation | **NOT VERIFIED / REQUIRES HARDWARE**; wiring/calibration incomplete. |
| Safety validation | Source/host gate behavior supported; no inspected supported-origin bypass demonstrated. Electrical/physical safety not established. |
| Backend warning integration | Generic failure paths exist; detailed CCPS assistance chain incomplete. Actual server receipt/admin notification NOT VERIFIED. |
| Controlled physical motor/navigation tests | **NO, not ready as-is.** Do not treat successful compilation or disabled-default firmware as a test authorization. |

## 18. Recommended Fix Order

These are recommendations only, **not repairs performed by the audit**.

1. Preserve this evidence and promote the failing diagnostics into explicit regression
   requirements. Keep existing expectations; do not mask failures or clip measurements.
2. **A01:** enforce exact independent targets through settling/coast before terminal
   success or route advancement. This is prerequisite to trustworthy Protocol-3
   completion and controlled exact-tick motor tests.
3. **A02:** correct signed expected-yaw semantics, then test forward/reverse asymmetric
   targets, partial resume and CW/CCW. Recheck nominal route-pose assumptions.
4. **A06:** harden central pre-assessment validation; retain normal missing/quality
   behavior and independent local authority. This is small and can be independently reviewed.
5. **A03:** define transient versus unrecoverable evidence policy and bounded Human
   escalation without auto-resume. Couple its tests to explicit reset/verification.
6. **A04:** separately authorize backend/admin work: preserve detailed bounded reason,
   durable assistance state and meaningful visibility. Test it offline/in staging
   before claiming receipt or notification. Add local/idle reporting only by explicit scope.
7. Resolve **S01/S02** through acquisition/scheduling diagnostics; validate continuity
   limits against the intended sensor/worker timing before any navigation validation.
8. **A05:** optimize the corrected, feature-retaining configuration; rerun complete
   image gates with intended provisioning. No security or safety shortcuts.
9. Rebuild and rerun originals, audit regressions and newly added transport/worker
   failure tests; verify protected functionality and diagnostics under optimization.
10. Only after review/authorization, perform non-actuating bring-up, current-limited
    lifted-wheel tests, runtime profiling, then bounded floor/navigation tests with
    independently measured tolerances and the physical emergency disconnect.

Evidence directory: [audit-output/2026-10-27](audit-output/2026-10-27/README.md).
Production implementation, calibration, thresholds, backend and existing documents
were left unchanged; all recommendations require a subsequent authorized repair task.
