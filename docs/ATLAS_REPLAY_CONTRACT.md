# Atlas replay integration contract (2026-10-25)

## Audit, before implementation

The firmware currently speaks the enveloped `/robot/{id}/command` mailbox,
including boot/recovery UUIDs and immutable measured result retries. Saved routes
instead use **raw** JSON at `/robot/{id}/guidance/command?protocol=2&stopped=...`,
`guidance/result`, and `guidance/arrival`. These are separate protocols, not aliases.
Guidance binds UUID task/session/route/command identities and an increasing route
sequence. A started acknowledgement precedes execution; successful measured
completion advances exactly one segment. Arrival also requires independent staff
checkpoint attestation. Return is a separately recorded route, never an inversion.
Watchdogs: device manual lease 900 ms, guidance lease 1500 ms, segment 60 s,
task 15 min. Reboot/recovery invalidates authorization and physical checkpoint
proof. Idle polls are not explicit operator stops.

Missing contracts: saved segments have no kind/turn angle; the backend has no
pause/resume command; firmware does not consume guidance; no LSM6DSO code exists.
Encoder inputs are unsigned single-signal edges per side, NOT quadrature. The
configured effective wheel CPR is zero, not a verified 20. With 6.5 cm wheels,
20 **effective** counts means 1.021 cm/count, incompatible with the current
0.5 cm calibration gate. Pins, count convention, track width and turn scales must
remain unconfigured until measured. No IMU mounting, wiring or safety thresholds
are documented, so inertial tilt/impact/yaw control cannot safely be invented.

## Explicit additive changes proposed for implementation

* Guidance protocol **3**, negotiated explicitly; protocol 2 remains unchanged.
  Version 3 segments require `motion_kind` (forward/backward/left/right/stop)
  and `turn_degrees` (90 only for turns; null otherwise). Legacy untyped routes
  cannot execute under v3. New recordings derive these fields from the actual
  command only after validating its measured wheel directions and start result.
* Version 3 uses the existing independent unsigned tick targets. Both sides
  must finish; no timing-based distance or inferred turn angle. Completion remains
  exact-tick on the server: overshoot is a failure, never fabricated as target
  counts. Local endpoint/sensor checks and all safety deadlines still apply.
* New staff-only `POST /robots/{id}/guidance-control`: task/session/route/command
  UUIDs, sequence, request_id UUID, action pause/resume. A single pending control
  appears on the current v3 segment as `control`. Device results add statuses
  paused/resumed and `control_request_id`. Pause stops before acknowledging;
  resume acknowledges authorization before requesting local resume. Duplicate
  controls are idempotent; conflicting/stale context is rejected. Pause consumes
  existing segment/task time; disconnect/stop/safety/recovery cancels, not resumes.
  These are delivery substates, not new persistent robot/task states.
* Idle v3 guidance is explicitly `command: idle, execute: false`. A task stop
  cannot silently cancel unrelated local recording. Existing manual executable
  stop/recovery always wins. Context disappearance while guidance is active is
  a failure. No automatic motion on reconnect or reboot.
* Arrival uses the existing contextual endpoint and independent staff evidence;
  firmware must not claim operator attestation on its own.
* LSM6DSO is a diagnostic, non-navigation input. Optional explicit I2C pins,
  address and electrical confirmation default unconfigured. No guessed wiring,
  calibration or inertial stop thresholds. Missing/stale/failed samples are invalid.
  No automatic self-test actuation in the control loop; physical verification is
  required before using inertial evidence for safety or turn calibration.

All transport remains in the existing network worker with copied bounded queues;
only the main loop may operate navigation or motors. No database migration,
partition change, dependency or automatic hardware provisioning is required.
