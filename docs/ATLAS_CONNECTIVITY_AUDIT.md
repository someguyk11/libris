# Atlas recording, replay and recovery audit

Date: 2026-10-24. Scope: current firmware, Python backend and Atlas monitoring UI.

## Conclusion

**Backend saved-route replay is not end-to-end connected.** Firmware `BackendClient` / `TelemetryManager` poll the manual command mailbox, not the protocol-2 guidance endpoints implemented in `backend/guidance.py`. The local `Navigation` / `Routes` replay engine is separate: it uses RAM-resident, centimetre-based segments, rather than loading the backend's encoder-tick routes. Backend guidance tests simulate a device; they do not establish firmware integration.

**LSM6DSO support is absent.** No IMU driver, startup self-test, low-pass-filter configuration or IMU health reporting was found. Encoder/ultrasonic safety is not evidence of an IMU self-test. Adding this safely needs the actual bus/pins, device identity/address, library/version and intended sample rate/filter bandwidth; no such settings were invented.

The new recovery path is connected across the backend, firmware and UI and compiles. It has not been physically exercised.

## Recording checklist

“Pass” below means code inspection and automated coverage where available, not a physical robot test.

| Requirement | Finding |
| --- | --- |
| Valid start request required | Pass for backend recording: registered robot, valid active checkpoint, no guidance, existing recording or pending movement. Pending recovery now also blocks recording. Starting a buffer does not itself prove the robot is online or physically at the checkpoint. |
| Only valid movements recorded | Manual API/firmware reject unsupported commands; successful measured results are required before save. Imported route tick patterns do not prove a supported physical movement. |
| Consistent sequence numbers | Recording sequences and transport sequences are distinct; commands/results are matched by identity and sequence. Imported routes require increasing unique sequences, not necessarily contiguous ones. |
| Consistent distance/encoder units | Firmware motion and ultrasonic distances use cm; backend movement records use unsigned encoder-edge counts per side plus direction. These are distinct representations, not an implemented route conversion. Calibration must be supplied. |
| V1 90-degree turns only | Local motion/route engine and manual left/right commands use configured 90-degree turns. Backend imported tick movements have no angle/calibration provenance, so this constraint is not fully enforced for stored routes. |
| Stop and safety interruptions | Firmware cancels motion and reports failure on interruptions. Recording failures prevent successful save. Recovery/reboot now fails interrupted recording and rejects late successful results. |
| Recording stops on request | Finalization requests a stop and waits for command completion before saving. Discard also waits for the stop path. |
| Failed save not silently successful | Pass: database failures retain the buffer; release happens after successful persistence. |
| Repeated save does not duplicate | Pass: recording UUID is reused as route UUID, with retry/idempotency checks. |
| ESP32 restart does not resume recording | Firmware state is volatile. New boot identity now fails backend recording/guidance and invalidates checkpoint proof; no automatic resume. Server restart also loses temporary recording buffers. |

## Replay checklist

| Requirement | Finding |
| --- | --- |
| Commands execute sequentially | Local engine and backend protocol each enforce ordering independently. **Missing firmware/backend replay bridge prevents end-to-end validation.** |
| Next command waits for completion | Local engine waits for motion completion and checkpoint validation; backend waits for matching terminal result. Same integration gap applies. |
| Encoder targets/completion consistent | Unresolved across implementations: local route units differ from backend tick routes; backend compares measured movement exactly against target. Physical overshoot/tolerance and calibration provenance need a defined contract. |
| Failed movement not counted completed | Local faults/timeouts fail the route; backend rejects failed/mismatched results rather than advancing normally. |
| Pause/stop/resume transitions | Local engine has pause/resume guards and cancellation. Backend guidance has stop/cancel/failure but no equivalent pause/resume protocol. Do not expose backend resume as implemented. |
| Safety stop interrupts execution | Local safety overrides motion independently of networking. Backend guidance leases/failures exist, but the firmware guidance bridge is absent. |
| Invalid/incomplete routes rejected | Active routes require valid structure, nonempty segments and confirmed, connected checkpoint endpoints. Drafts may be incomplete. Tick payload validity is not proof of physical calibration or a 90-degree turn. |
| Checkpoint mismatch not ignored | Local replay requires checkpoint validation before advancing; backend arrival requires stopped-device completion plus operator checkpoint confirmation. Operator confirmation is not autonomous physical localization. |

## Recovery implementation

- Existing application-managed Wi-Fi retries and fresh TLS connections are reused. No additional networking task or database schema was introduced.
- Staff queue `POST /robot/{id}/recovery` with a UUID `request_id`, action `reconnect`, `restart` or `auto_reconnect`, and a strict boolean `enabled` only for the latter.
- Compatible firmware must have polled recently. Requests expire after 60 seconds; one request may be pending. A bounded 64-request receipt history deduplicates retries within this runtime. This is not persistent exactly-once execution across backend restarts.
- Firmware polls using a random per-boot identity. Main-loop navigation cancellation and motor stop approval precede the device-authenticated `POST /robot/{id}/recovery/ack` and action. Recovery never resets a safety latch or resumes work.
- Acknowledgement means accepted, not finished. Reconnect/policy completion requires the matching `recovery_completed` marker on a subsequent poll. Restart completion requires a new boot identity after acceptance. An unrelated reboot can also satisfy this evidence; it proves reboot, not its cause.
- Recovery intentionally fails active recording/guidance and clears checkpoint location proof, including policy changes. Late movement success after interruption is rejected.
- Atlas monitoring reuses `AppButton`, `AdminDialog` and existing status polling. It shows live/stale command polling, policy, request state and boot identity, confirms restart/disabling retries, and explicitly reports the replay/IMU gaps.
- Automatic retry policy and recovery state are volatile backend runtime state; firmware starts with automatic retries enabled. This is not a persistent fleet configuration system.
- A disconnected robot cannot receive a backend recovery command. Local retry/setup or a power cycle is still needed. Disabling retries can therefore make remote recovery unavailable.
- Legacy factory-reset/network-reconfiguration delivery still uses consume-on-poll flags, not this acknowledged mailbox; it remains a separate reliability limitation.

## Verification

- Required **PlatformIO MCP** build, environment `esp32dev`: succeeded. Platform espressif32 8.0.0, Arduino ESP32 3.3.12 / ESP-IDF 5.5.5, ArduinoJson 7.4.3. RAM 59,516 / 327,680 bytes; flash 1,314,843 / 3,145,728 bytes.
- Backend: `python -m unittest test_api test_guidance test_atlas_recovery`: **68 passed**. Includes interrupted recording/guidance, stale boots, explicit completion evidence, expiry, deduplication, device/staff authorization and pending-recovery recording guards.
- Frontend: `npm run check`: **36 tests passed**, Vite production build succeeded.
- Existing host navigation simulator: **27 passed** with Zig. Uses hardware stubs; does not validate real encoders, sensors or motors.
- PlatformIO MCP device inventory showed only a non-upload-candidate COM1 device, not an ESP32. **No upload, serial reset, remote restart, motor movement, physical IMU test or Wi-Fi outage test was performed.**
- Required **Espressif documentation MCP** consulted official Wi-Fi station reconnect/disconnect guidance and miscellaneous system `esp_restart()` documentation. These support application-owned retry, replacing sockets after disconnect, and stopping outputs before restart because restart does not reset every peripheral.

## Physical validation still required

1. Configure measured GPIO mappings, encoder resolution/count convention, track width, motor directions, turn calibration and backend movement step. Current unconfigured hardware settings intentionally prevent safe driving.
2. With wheels raised, verify stop-before-restart/reconnect, credential retention, new boot reporting, lost acknowledgement handling and no recording/guidance resume.
3. Exercise Wi-Fi loss with retries enabled/disabled; confirm local safety remains responsive and stale commands never replay after reconnect.
4. Calibrate distance/90-degree turns and verify encoder targets under load; then define and implement the backend guidance bridge and completion tolerance before claiming saved-route replay works.
5. Once actual LSM6DSO wiring/configuration is available, implement and physically validate its identity check, vendor-prescribed self-test and sample-rate-compatible filtering. Do not report these as present today.

## Files changed for this work

Created: `backend/atlas_recovery.py`, `backend/test_atlas_recovery.py`, `src/views/admin/components/AtlasRecovery.vue`, this report.

Modified: `backend/atlas_runtime.py`, `backend/guidance_access.py`, `backend/recordings.py`, `backend/robot.py`, `backend/test_guidance.py`; `hardware/Atlas/src/BackendClient.h/.cpp`, `NetworkManager.h/.cpp`, `TelemetryManager.h/.cpp`; `src/services/api.js`, `src/views/admin/AtlasDetailView.vue`.

Unrelated existing changes were preserved. No IMU settings or hardware calibration values were assumed, and no disconnected replay path was labelled complete.
