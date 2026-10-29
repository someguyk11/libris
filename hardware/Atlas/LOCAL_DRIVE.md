# ESP32 AP local drive

## Supervised encoder-only on-ground test

Use the **`atlas-encoder-ground-test`** PlatformIO environment, not
`atlas-local-drive` or `atlas-motor-bench`, for the explicitly requested
ultrasonic-free straight recording/replay test:

```powershell
pio run -e atlas-encoder-ground-test
# After checking wiring and clearing the test area, upload explicitly:
pio run -e atlas-encoder-ground-test -t upload
```

- Motors: left GPIO26/25, right GPIO27/33; input fanout operator-confirmed.
  Driver nSLEEP pins remain GPIO19/18.
- Photointerrupters: left GPIO34, right GPIO35, `CHANGE` interrupts, **40 edges
  per wheel revolution** (20 slots, disc-to-wheel ratio 1:1). No internal pulls:
  provide defined 3.3 V-safe signals/external pull-ups if required by the outputs.
- Wheel diameter remains 6.5 cm (~0.5105 cm/edge). The existing distance
  calibration guard compares edge size to the **2 cm checkpoint tolerance**,
  not the 0.5 cm movement-stop tolerance; neither tolerance was widened.
- **No obstacle detection, ultrasonic GPIO activity, sensor start inhibition,
  or ultrasonic endpoint comparison in this build.** Readings stay invalid,
  never fabricated as clear. Independent encoder endpoint/mismatch checks and
  the 200 ms settling interval remain. Coast can affect stopping distance.
- Only forward/backward 20 cm-per-press steps; **turns are rejected** even if
  turn geometry is configured later. Normal build still requires measured track
  width and CW/CCW calibration for 90-degree turns.
- PWM/ramp limits, per-wheel stall checks, control/movement/route timeouts,
  explicit E-stop/reset, and 800 ms browser deadman remain enabled. The firmware
  does not command movement automatically after upload, boot, save or selection.
- Test routes use NVS namespace **`atlasedgetest`**, three slots of 32 segments,
  schema v3 `[kind, measuredCm]`, and the unchanged 3000-byte route limit. Normal
  routes remain in `atlasroutes`; neither build accepts the other's schema.
  Both namespaces share the existing 20 KiB NVS partition, so either can fill it.
- The local page and serial boot message identify the sensor-free test mode.
  Backend commands remain excluded by the existing exclusive local-drive mode.

### Ground-test checklist (requires physical verification)

1. Confirm each encoder produces ~40 edges per full wheel revolution; check
   output voltage/pull-ups and motor direction before recording a route.
2. Keep an immediate **physical motor-power cutoff** available, clear the area,
   and keep Atlas in sight. No software stop can detect an unseen collision.
3. Join `Libot_Setup`, open `/drive`, select Movement/enable drive if Wi-Fi setup
   mode is active, and check the encoder-only warning and safety-ready status.
4. Test one 20 cm forward and backward step; measure real distance and coast.
   Check STOP/E-stop, explicit reset without resume, and heartbeat-loss stopping.
5. Start recording, hold Front/Back until validated, then release. Repeat a few
   steps, name the path and save; wait for confirmed successful storage.
6. Move Atlas manually back to the original start **and heading**. Select Replay,
   confirm starting placement, click Replay, and keep the page foregrounded.
   Compare real travel with the recording; pulse agreement does not detect slip.
7. Verify saved routes survive a power cycle, full slots replace oldest only
   when requested, and interrupted replay never auto-resumes. Reposition before
   restarting an interrupted route.

Host validation: `python tests/host/run_encoder_ground.py --cxx <compiler>` runs
real control, safety, navigation, local command and route-store code with fake
GPIO/PWM/time/queues and in-memory Preferences. It is **not** a flash endurance,
power-loss, electrical, physical stopping-distance or ground-replay test.

## Connect

1. Build/upload the **atlas-local-drive** PlatformIO environment from this directory.
   No dependencies or framework changes are required. This target excludes only
   the existing unfinished `ConfidenceIntegration.cpp`; its source is untouched.
   The original `esp32dev` target currently fails because that file references
   missing motion/navigation APIs.
2. The `Libot_Setup` AP uses the fixed temporary password **`AtlasDrive123`**.
   It stays the same after reboot. Change `SetupPassword` in `src/AtlasConfig.h`
   before deployment; anyone with this shared bench password can reach the controls.
   The serial monitor at **115200 baud** prints the AP name and page address.
3. Join that AP (one Wi-Fi station allowed). Stay connected despite the phone's
   “no internet” warning. Open **http://192.168.4.1/drive** in a normal browser.
4. Hold Front/Back for one bounded encoder move (default **20 cm**); Left/Right
   requests one calibrated **90°** turn. Release early to cancel. Holding after
   completion does not issue another movement. Release and press again.

No backend, Supabase, router, TLS provisioning, or synchronized clock is needed.
The AP stays available. Local mode disables station reconnection, scanning,
Wi-Fi profile writes **while driving**, and disables serial motion commands to
avoid competing owners. Use the visible **Wi-Fi connection** option to enable
setup while motors are inhibited, then **Drive & replay** to return to control.
Set `LocalDriveEnabled = false` in `src/AtlasConfig.h` to restore backend operation.

## Switch between Wi-Fi connection and drive/replay

The drive page has **Wi-Fi connection**, **Movement** and **Replay** options.
The connectivity page at **http://192.168.4.1/** has **Wi-Fi connection** and
**Drive & replay** options. No reflashing is required to switch these local modes.

- Selecting **Wi-Fi connection** requests a main-loop stop before the network
  worker enables station setup. An active unsaved recording is discarded;
  saved JSON routes are unaffected. Scanning, profile saving/forgetting and
  connecting are available only in this mode. Motion/record/replay requests are
  rejected until you switch back, even if another browser tab sends them.
- In the connectivity page, enter/select a network and use **Save** or **Connect**.
  The existing five-profile EEPROM layout remains unchanged. At boot, no station
  profile is attempted automatically in local mode; choose Connect explicitly.
- Selecting **Drive & replay** waits for any scan to finish, cancels an unfinished
  connection attempt, and stops further automatic station reconnects/scans.
  A router connection that already succeeded may remain connected; it does not
  enable backend or Supabase command dispatch. Switching back never starts or
  resumes a movement. Every route replay still requires its own start confirmation.
- The password-protected AP remains available after a successful router
  connection. ESP32 AP/STA share a radio; connecting/scanning may briefly disturb
  the AP link. Rejoin `Libot_Setup` if needed. The HTTP server remains AP-local,
  not exposed through the router-facing station interface.
- Mode switches use the existing stationary configuration-approval handshake.
  Neither a GET request nor simply viewing a page changes the operating mode.
  A new mode is confirmed through status before navigating between pages. If you
  open `/drive` directly while Wi-Fi setup is selected, choose **Enable drive &
  replay** (or Movement/Replay) to request the switch explicitly.

## Hardware configuration still required

The page does **not** bypass the existing safety interlocks. Configure the actual
motor/photointerrupter/ultrasonic GPIOs and electrical-interface confirmations,
driver nSLEEP management, single-signal encoder edge convention, effective counts
per wheel revolution, wheel track and measured CW/CCW calibration in
`src/AtlasConfig.h`. No assignments or calibration values were invented.
Ultrasonic echo inputs must be 3.3 V safe.

`LocalDriveStepCm` and `LocalDriveLeaseMs` control step distance and heartbeat
timeout. The browser sends heartbeats every 200 ms and polls status every 300 ms.
The main loop stops and latches E-stop after **800 ms** without a valid heartbeat.
HTTP runs on the existing network worker; queues hand commands/status to/from
the motor-owning main loop without blocking it. Request sequence numbers reject
out-of-order submissions, and an AP-local token plus existing Host/Origin/JSON
checks prevent cross-origin browser commands. This is a single-operator test UI,
not a multi-user authenticated control system.

Obstacle avoidance is now **enabled**. Local movement also refuses to start if
it is disabled. Existing sensor validity/age checks, stall detection, acceleration
ramps and movement timeouts remain active. STOP cancels; **Latch E-stop** inhibits
future starts. **Reset safety** explicitly clears a fault only if the existing
configuration/sensor conditions permit it, and never resumes a move.

HTTP `202 queued` means mailbox acceptance, not physical execution. Read the
subsequent status message and encoder readings. A failed/expired request or lost
status is not proof that wheels stopped; keep a physical power cut available.
Direction changes while moving may be rejected by the existing stopped dwell:
release, wait, and press again. There is no automatic retry or resume.

## Record, save and replay locally

1. In the **Movement** tab choose **Start new recording**. Starting a new recording
   replaces the previous unsaved RAM path, not any saved flash route.
2. Hold each direction until the step finishes **and** `validated steps` increases;
   then release. The existing Navigation recording code measures the completed
   distance from the wheel encoders and captures four ultrasonic endpoint ranges.
   Turns remain calibrated 90° segments. Nothing is inferred from press duration.
3. Repeat for up to **32 segments**. If you release during actual motion, hit
   E-stop, or encounter a fault, the incomplete recording is invalidated. Rebuild
   the path from its original start; never append a disconnected fragment.
4. With all steps validated and controls released, enter a name and select
   **Save path on ESP32**. The default **Automatic** destination fills an empty
   slot first; when all **3 slots** are full, it replaces the **oldest saved
   route** without another confirmation. A successful overwrite becomes the
   newest route. Save order is persisted in the JSON, so rebooting does not reset
   rotation. Routes saved by older firmware without age metadata are considered
   oldest (ties use the lowest slot number). Invalid/unusable slots are filled
   before valid routes are replaced. You can still select a particular slot
   manually; replacing an occupied manual slot asks for confirmation.
   Wait for **Path saved as JSON on ESP32 in slot …** in status.
   `202 queued` alone is not a successful save. A failed write leaves the RAM path
   available for retry; the firmware never erases NVS to make space.
5. Open **Replay**, select a saved route, and use **View selected route JSON** to
   inspect its contents. Selection/viewing alone never moves the robot.
6. Physically return Atlas to that route's **original starting position and
   heading**, confirm the checkbox, and click **Replay selected route**. There is
   no localization or automatic return-to-start. Existing navigation checks
   encoder completion and ultrasonic endpoint consistency between segments.
7. Keep the browser in the foreground. **STOP replay**, tab switches, lost browser
   heartbeats, faults and obstacles cancel the replay; no automatic resume is
   offered. Re-position at the original start before trying again.

Routes are stored as bounded **JSON strings in ESP32 NVS**, namespace
`atlasroutes`, keys `route0`–`route2`. They survive normal reboot/power cycling
and firmware uploads that preserve NVS. No filesystem upload, extra dependency,
partition change, backend, or Supabase route is involved. The existing 20 KB NVS
partition is shared with other settings, so storage can fill before all slots
are available; failures are reported, not silently claimed as saves.

New JSON schema: `version: 2`, `name`, `savedOrder` (persistent save age),
`calibration` (configuration fingerprint), and `segments`. Each segment is a
compact array `[kind, cm, frontCm, backCm, leftCm, rightCm]`. Kind values are
1=forward, 2=backward, 3=clockwise 90°, 4=counterclockwise 90°. Straight `cm` is
the measured magnitude; turn `cm` is zero. All four valid sensor readings are
mandatory. No extra measurement quantization is introduced. Existing version-1
object-based JSON (`kind`, `cm`, `mask`, `ranges`) remains readable without an
automatic flash rewrite. Loading validates schema, numeric ranges, count limits
and the encoder/geometry calibration fingerprint. Corrupt, incompatible or
missing routes cannot replay. Routes are produced only by validated local
recording; JSON importing/editing is not part of this basic page.

Flash commits run on the existing network worker only after the main loop has
issued a job while stationary, with no pending movement. Until the result comes
back, local motion commands are rejected. Saving/loading never happens in an
encoder ISR or in a moving control loop. The selected slot is not automatically
replayed after boot.

### Capacity without pushing the board

The **32-segment** local cap now matches the existing navigation cap; navigation's
route buffer was not expanded. Three slots, the **3000-byte JSON limit**, NVS
partition, worker task count and all control/safety timeouts remain unchanged.
Compact JSON removes repeated per-segment field names instead of pushing NVS
strings toward their 4000-byte absolute limit. The two job queues together need
approximately **1.1 KB more payload RAM** than at 12 segments; larger temporary
route copies also use some additional stack space. Only one storage job is active
at a time and writing still occurs only while stationary.

Host tests use the actual ArduinoJson serializer to exercise 32-step routes,
escaped names, old-schema compatibility, changed calibration, invalid readings,
33-step rejection and 1000 varied full-length JSON payloads. These tests do not
measure live ESP32 heap/stack peaks or NVS fragmentation. A full/fragmented store
can still reject a save safely; physical endurance, free-heap and task-stack
headroom must be checked on the configured robot before increasing beyond this
cap. The existing 10-minute total-route timeout still applies.

## Bench checklist (not yet physically tested)

- [ ] Start with wheels raised; confirm boot/reset cannot energize motors.
- [ ] Switch Wi-Fi → Drive/replay and back; confirm motion stops before setup, never resumes on return, and both pages remain reachable after connecting to a router.
- [ ] While Wi-Fi setup or a mode transition is active, verify a second browser tab/API client cannot start recording, movement or replay. Switching modes must preserve saved routes.
- [ ] Confirm the password works, `/drive` loads offline, and backend commands cannot drive in local mode.
- [ ] With configuration missing, controls stay inhibited and report configuration faults.
- [ ] Turn each wheel by hand; verify independent photointerrupter edges and calibrate counts per wheel revolution.
- [ ] Verify all four ultrasonic directions, distances and level shifting; disconnected/stale readings must inhibit movement.
- [ ] Confirm Front/Back motor polarity at conservative duty; check release, pointer cancellation, STOP and E-stop.
- [ ] Disconnect Wi-Fi/close the browser during a step: confirm stop within the lease plus control-loop response, latch, and no automatic resume.
- [ ] Verify sensor obstacle stops in every direction and stalled-wheel detection. Do not test by placing hands near powered wheels.
- [ ] Measure straight distance on the floor, then calibrate CW and CCW 90° turns separately.
- [ ] Hold beyond completion: exactly one move. Reset must not start movement. Repeated status polling must never start movement.
- [ ] Record forward + 90° turn + forward; verify the JSON contains measured distances and all four endpoint ranges.
- [ ] Save into different slots, power-cycle, reconnect with the same AP password, and confirm both routes remain selectable with correct names/content.
- [ ] With Automatic selected, save A/B/C/D: D replaces A, leaving B/C/D. Reboot and save E: E replaces B, leaving C/D/E.
- [ ] Selecting/viewing a route must not move motors. Replay each route only from its original start/heading; verify segment order and endpoint checks.
- [ ] Interrupt recording mid-step: the draft must not be saveable. Interrupt replay with STOP, obstacle, Wi-Fi loss and browser backgrounding; verify no automatic continuation.
- [ ] Verify save/load cannot overlap motion, occupied-slot replacement asks for confirmation, and changed calibration/corrupt JSON is rejected.

## Verification

Official Espressif documentation consulted through the configured documentation
MCP: Arduino Wi-Fi SoftAP/configuration, TCP server `accept`, Arduino 2→3 network
compatibility, and FreeRTOS single-item queue overwrite/peek/zero-wait receive.
These informed the retained Arduino networking and task-safe mailbox design.
The official ESP-IDF v5.5 RNG reference additionally confirmed `esp_fill_random`
and its Wi-Fi entropy prerequisite; control tokens are generated after Wi-Fi
activation. Existing GPIO interrupts and LEDC motor code are reused unchanged.
For local route storage, the Espressif documentation MCP also verified Arduino
Preferences/NVS namespaces, bounded string reads, `putString` persistence and
flash-write timing limitations. The official Arduino-ESP32 **3.3.12**
`Preferences.cpp` source was checked for exact get/put length/commit behavior.
For the capacity increase, the documentation MCP verified NVS's 4000-byte string
ceiling, 4096-byte page geometry, contiguous-entry requirement and the extra free
space required for replacement writes. These informed compact JSON and retaining
the smaller 3000-byte application limit rather than expanding the flash partition.
For the page/mode switch, the documentation MCP verified Arduino AP+STA operation,
`WiFi.mode`, asynchronous `scanNetworks`, `scanComplete` and `scanDelete`.
These informed keeping the AP enabled and waiting cooperatively for scans before
restoring drive permission, rather than scanning or reconfiguring during motion.

`atlas-local-drive` compiles with the installed Arduino-ESP32 **3.3.12** framework.
Browser logic is tested with `node --test tests/local-drive-page.test.js` from the
repository root. Compilation and simulated browser tests are **not** hardware
validation. No firmware was uploaded and no physical tests were performed.
