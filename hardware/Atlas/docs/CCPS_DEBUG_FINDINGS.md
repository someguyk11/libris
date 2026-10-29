# CCPS debug findings — 2026-10-27

**Audit only; no repairs applied.** References below are relative to `hardware/Atlas`
unless explicitly prefixed with `backend/` or frontend `src/views/` (repository root).
See [the full report](CCPS_FULL_DEBUG_AUDIT.md) for scope, test limitations and readiness.

## Register summary

| ID | Severity | Evidence label | Finding |
| --- | --- | --- | --- |
| CCPS-A01 | P1 | PROVEN DEFECT | Settled coast beyond an exact tick target can be reported as completed. |
| CCPS-A02 | P1 | PROVEN DEFECT | Asymmetric reverse targets use a forward-sign expected yaw. |
| CCPS-A03 | P2 | PROVEN DEFECT | Persistent required-sensor failure lacks a direct human-assistance escalation path. |
| CCPS-A04 | P1 | PROVEN DEFECT | Guidance failure handling discards the firmware's detailed confidence error. |
| CCPS-A05 | P2 | PROVEN DEFECT | Both measured complete images exceed the hard 1,000,000-byte gate. |
| CCPS-A06 | P2 | PROVEN DEFECT | A non-finite pre-assessed range confidence can be ignored by central aggregation. |
| CCPS-S01 | P1 potential | SUSPECTED DEFECT | Lost raw IMU observations within the configured gap allowance may be bridged undetected. |
| CCPS-S02 | P2 potential | SUSPECTED DEFECT | Repeated post-encoder IMU publications may starve timestamp bracketing under adverse scheduling. |

**Confirmed:** P0 **0**, P1 **3**, P2 **3**, P3 **0**; **two suspected issues**,
not included in confirmed totals. Multiple failing tests of A01/A02 are not separate defects.
No supported movement-origin safety bypass was demonstrated. This is not a proof
of absence of every unsafe behavior or a hardware-safety certification.

## CCPS-A01 — Exact-tick coast accepted as successful completion

- **Subsystem/severity:** motion, checkpoint, guidance result; **P1**.
- **References:** `src/MotionController.cpp:250-270`, `src/ConfidenceSupervisor.cpp:65-125`,
  `src/CheckpointManager.cpp:30-35`, `src/TelemetryManager.cpp:179-185,358-363`;
  repository `backend/guidance.py:593-596`.
- **Expected:** an exact 50/50 tick command must not successfully complete with
  measured 51/51 ticks. Preserve actual counts; do not clip/fabricate 50/50.
- **Actual/trigger:** reach 50/50, enter `Complete`, add one coast edge per side
  during settling. Navigation becomes `Completed` with no local fault; history
  records 51/51. The coordinator emits `Completed` with measured 51/51.
- **Evidence:** `exact_tick_coast_rejection` and `backend_coast_reject` fail at
  both optimization levels. The backend source independently requires exact
  equality and would reject this completion, rather than advancing the route.
- **Root cause:** the exact-target comparison runs only while motion is `Moving`.
  Subsequent terminal CCPS/checkpoint checks use centimetre tolerances and do not
  retain/enforce the exact-target contract through coast/settling.
- **Recommendation:** carry exact-target semantics through terminal validation,
  checking each side after settling and before completion/advancement. Overshoot
  must remain a failed measured execution. Do not weaken backend equality.
- **Complexity:** medium, spanning motion/lifecycle/endpoint/result coordination.
- **Regression tests:** one-sided/both-sided post-completion coast, pause/resume
  coast, counter wrap, duplicate terminal events, ordinary distance tolerance
  unchanged, failed result identity/count preservation, no route advancement.

## CCPS-A02 — Reverse expected-yaw sign

- **Subsystem/severity:** supervisory motion mathematics; **P1**.
- **References:** `src/ConfidenceSupervisor.cpp:75-86` and
  `src/MotionConfidence.cpp:17-22`; permitted targets at
  `src/MotionController.cpp:224-240`.
- **Expected:** signed reverse wheel travel and relative gyro yaw should agree
  with the command's own differential targets.
- **Actual/trigger:** `expectedYaw=(rightTarget-leftTarget)/track` has no reverse
  sign, although measured encoder yaw uses signed reverse travel. For targets
  10 and 11.5 cm, track 10 cm, physically consistent reverse yaw is -0.15 rad;
  expected yaw is +0.15 rad. The 0.30 rad discrepancy creates `Heading` (0x400).
- **Evidence:** pure `reverse_unequal_target_sign` and integrated 50/58 reverse-tick
  replay both fail at `-O0` and `-Os`. The integrated case remains paused instead
  of completing. This is false rejection, not demonstrated unsafe motor drive.
- **Root cause:** expected straight-segment yaw is calculated from unsigned target
  lengths while measured yaw is calculated from signed travel.
- **Recommendation:** make expected/actual coordinate and sign conventions
  consistent. If the intended straight-command contract instead requires zero
  heading change, specify and apply that consistently in both directions.
- **Complexity:** small-to-medium, including resume/logical-command mathematics.
- **Regression tests:** equal/unequal targets forward and reverse, either wheel
  already complete on resume, partial interruption, CW/CCW and unchanged exact ticks.

## CCPS-A03 — Missing direct assistance escalation for persistent sensor failure

- **Subsystem/severity:** critical failure state/reporting; **P2**.
- **References:** `src/ConfidenceSupervisor.cpp:164-179,226-239,245-248`;
  `src/SafetyController.cpp:85-96`; `src/Navigation.cpp:247-249`.
- **Expected:** distinguish transient missing evidence from a continuing fault
  that prevents safe continuation, with a defined human-assistance outcome.
- **Actual/trigger:** fail a required ultrasonic sensor during motion and keep it
  invalid. Motors stop and local sensor safety latches correctly. After 2.5 seconds
  of diagnostic simulation, trust is `Verify`, permissions **0**, reasons **0x4042**
  (`LocalSafety|Discontinuity|Missing`), and `Human` is absent.
- **Evidence:** `persistent_sensor_human` fails identically at both optimizations.
  Source requires `motion_.usable` for the relevant terminal assistance branch;
  failed sensor confidence makes that assessment unusable. There is no dedicated
  persistence/escalation deadline. Later unrelated uncertainty or other failures
  could request assistance; this finding does **not** claim Human can never appear.
- **Root cause:** unavailable evidence is excluded from the terminal branch that
  requests human help; stopping and assistance escalation are different predicates.
- **Recommendation:** explicitly define/escalate unrecoverable or persistent missing
  evidence, without inventing a timeout in this audit or allowing automatic resume.
- **Complexity:** medium policy/state/diagnostic integration.
- **Regression tests:** temporary versus persistent sensor absence, valid recovery,
  reset/verification ordering, no implicit resume, human flag and report identity.

## CCPS-A04 — Detailed guidance failure is lost before administrative display

- **Subsystem/severity:** firmware/backend/admin warning integration; **P1**.
- **References:** `src/BackendClient.cpp:287-305`; repository
  `backend/guidance.py:78-92,210-213,546-549,581-582`;
  frontend `src/views/admin/components/GuidanceMonitor.vue:19-21,30-39`.
- **Expected:** preserve a meaningful confidence-loss reason and assistance request
  through acceptance, persistent task failure and administrative presentation.
- **Actual/trigger:** firmware can send an error including CCPS reason hex, trust
  and clearance for a failed active remote command. The guidance endpoint accepts
  `error` but replaces it with **"Robot reported an obstacle, fault or execution
  failure."** It commits that generic task reason. The monitor can show generic
  failure history, but does not decode CCPS reasons or raise a dedicated confidence
  assistance notification. Local/idle CCPS loss has no unsolicited result route.
- **Evidence:** source inspection plus `backend_reporting_probe.py`, which executes
  the actual AST-extracted handler/fail functions against in-memory doubles. It
  acknowledges the supplied detailed confidence error, then discards that detail.
  No real server, HTTP request or database was used; actual delivery is NOT VERIFIED.
- **Root cause:** accepted error data is ignored by the guidance failure branches;
  no end-to-end typed assistance/notification mapping exists.
- **Recommendation:** separately authorize/design bounded reason preservation,
  durable assistance state and visible administrative escalation. Preserve safety
  and command authentication; do not interpret Wi-Fi/restart recovery as localization.
- **Complexity:** medium-to-large, cross-component work requiring separate approval.
- **Regression tests:** authenticated detailed failures, malicious/oversized text,
  duplicate/late failure, offline retry/exhaustion, persistent storage and meaningful
  admin presentation. Include local/idle failures if they are to be supported.

## CCPS-A05 — Binary-size release failure

- **Subsystem/severity:** release resources; **P2**.
- **References:** `platformio.ini:1-14`, `partitions_atlas_v1.csv:3`,
  `tests/host/analyze_size.py:69-71`, `tests/host/resources.py:17-28`;
  raw audit `build-measurements.json`, `main-build.log`, `probe-build.log`.
- **Expected:** complete required firmware image at most 1,000,000 decimal bytes.
- **Actual:** main **1,053,072** (+**53,072**); inert feature probe **1,190,000**
  (+**190,000**). Application sections and static RAM reproduce as 1,037,976 /
  61,604 and 1,172,171 / 63,108 bytes respectively. Both ordinary builds succeed
  against the 3,145,728-byte factory slot; that is not project-gate compliance.
- **Evidence:** actual file lengths, SHA-256, maps, symbols and repeated builds.
- **Root cause:** retained application/framework/network/security footprint exceeds
  the separate project limit; unconfigured main removes important reachable paths.
- **Recommendation:** measured semantics-preserving reduction, then rerun production
  and feature-retaining measurements with intended provisioning/calibration.
- **Complexity:** medium-to-large; savings not guaranteed by this audit.
- **Regression tests:** all host tests plus diagnostic regressions, feature-retention
  checks, provisioned complete-image gate, target timing and security verification.

## CCPS-A06 — Central aggregation trusts malformed pre-assessment

- **Subsystem/severity:** central numerical input validation; **P2**.
- **References:** `src/ConfidenceSupervisor.cpp:161-166,205-230`,
  `src/ConfidenceTypes.h:49-53`; compare normal producer
  `src/SensorConfidence.cpp:3-9,41-43`.
- **Expected:** a NaN confidence at the central evidence boundary must not result
  in a healthy aggregate and movement permission merely because `usable=true`.
- **Actual/trigger:** supply `ranges[0].confidence=NAN`, `usable=true` in otherwise
  healthy public `Evidence`. `fminf(1,NAN)` returns 1, and the Boolean readiness
  check trusts `usable`. Observed sensor=1, navigation=1, permissions=5 (`Start|Verify`).
- **Evidence:** `nonfinite_public_assessment` fails at both optimizations.
- **Root cause:** no independent finite/range validation of supplied assessments.
- **Scope limit:** normal `SensorConfidence` producers clamp/reject invalid input;
  no ordinary hardware-origin route producing this inconsistent assessment was
  demonstrated. This is a proven public-boundary robustness defect, **not** a
  demonstrated GPIO/sensor failure bypass or reason to rate it P0.
- **Recommendation:** validate central assessment numbers and flag consistency;
  deny malformed inputs rather than letting floating-point min hide them.
- **Complexity:** small, with careful invalid/missing-score semantics.
- **Regression tests:** NaN/±infinity/out-of-range scores, inconsistent validity flags,
  all sensor types, normal low quality and current hard faults dominating history.

## Suspected issues — not counted as proven defects

### CCPS-S01 — IMU acquisition loss below gap threshold (potential P1)

`src/ImuManager.cpp:90-132` polls data-ready, reads the latest output registers and
timestamps after the read; no FIFO/sample-counter/sensor-timestamp processing is
implemented. `src/ImuIntegration.cpp:24-25,41-49` accepts any forward interval up to
the configured gap, integrating adjacent observed rates. At 104 Hz, a delayed read
can miss intervening physical samples while remaining within that interval. The
main-loop mailbox also overwrites intermediate *integrated* evidence, which is a
different issue: cumulative yaw generally survives that overwrite.

**NOT VERIFIED / REQUIRES HARDWARE:** no measured lost raw sample, yaw error or
target scheduling trace establishes the frequency/severity here. Do not claim all
missing physical observations are detected. Validate data-ready-to-read latency,
missed observations, I2C timing and angular transients under worker/network load;
then define a justified acquisition/continuity policy. Estimated investigation/fix:
medium-to-large; regressions need raw-gap/overrun and known-rate rotation traces.

### CCPS-S02 — Bracketing liveness under adverse publication ordering (potential P2)

`src/ConfidenceIntegration.cpp:50-66` captures encoders before peeking the latest
IMU. `src/ConfidenceSupervisor.cpp:15-46` requires a bracketed timestamp, but does
not retain an unbracketed IMU observation for the next encoder snapshot. Repeated
publication just after encoder capture, combined with mailbox replacement before
the next evaluation, could starve alignment and cause a conservative false inhibit
or Lost after the alignment window. Sampling the clock after the peek fixes a
future-clock underflow, not necessarily this liveness condition.

**SUSPECTED DEFECT / NOT VERIFIED:** no concurrent ESP32 trace or complete worker
scheduling reproduction was run. Normal 5 ms main-loop cadence versus 104 Hz IMU
may give the next snapshot a bracket. Stress period/phase/jitter and cross-core
handoffs before deciding whether buffering/order changes are needed. Estimated
complexity: medium. This concern does not demonstrate continued unsafe motion.
