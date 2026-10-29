# Isolated motor bench test

Current build is **RIGHT SIDE ONLY** for fault isolation. Right Front drives
GPIO26; Right Back drives GPIO25, each held digitally HIGH (no PWM) for up to 300 ms.
The opposite input stays LOW. This is full motor-supply drive, not reduced-duty drive.
Left motor inputs remain zero and left SLEEP remains LOW during motion.
The separate supply-disconnected SLEEP diagnostic still tests both SLEEP pins.

This build uses GPIO26/25 for the right driver instead of the original GPIO26/12 wiring and does not initialize
ultrasonics or depend on encoders. It does **not** change the normal firmware's
pin policy, safety checks, navigation, recording or replay. Do not use it on the floor.

## Build / install

From `hardware/Atlas`:

```powershell
pio run -e atlas-motor-bench
pio run -e atlas-motor-bench -t upload --upload-port COM3
```

Upload replaces the running application. To return to normal firmware, build/upload
`atlas-local-drive` again; its incomplete hardware configuration still inhibits motion.

1. Disconnect motor power while checking the wiring. Secure the chassis with **all
   wheels lifted**, and keep a physical motor-power disconnect accessible.
2. Inputs: left IN1/3 = GPIO27, IN2/4 = GPIO33; right IN1/3 = GPIO26,
   IN2/4 = GPIO25. Separate nSLEEP controls = GPIO19 and GPIO18, driven together;
   verify these match the actual wiring before applying motor power.
   Only matching bridge **inputs** are fanned out; never connect bridge outputs together.
3. Confirm a common ground, correct motor supply/current capacity, and hardware
   pull-downs for safe reset. ESP32 signals are 3.3 V, not 5 V tolerant.
4. With all power disconnected, connect right IN1/3 to GPIO26 and IN2/4 to GPIO25.
   Leave GPIO12 disconnected: it selects the flash voltage at reset. No eFuses
   are changed by this build.
5. Join `Libot_Setup`, password `AtlasDrive123`, then open
   `http://192.168.4.1/drive`. The page MUST say **Atlas motor bench ONLY**.
6. Confirm wheels lifted, then arm one pulse. Press a direction. Release stops
   early; otherwise the requested cutoff is 300 ms. Re-arm for each new test.
   There is no PWM: the active input is HIGH for full motor-supply drive.
7. Check the right side's physical direction, STOP, E-stop/reset, release, and AP loss.
   No turns or left-side motion are available in this isolated build.
   Reset does not arm or move. A held/repeated command cannot extend a pulse.

There is no obstacle or encoder-stall protection in this isolated test. A timer-task
callback disables both drivers independently of HTTP handling, with a second cutoff
in the main loop. Timer scheduling is not a certified hard-real-time safety mechanism;
hardware pull-downs and a physical power disconnect remain necessary.

## Verification

Official Espressif Documentation MCP topics consulted: Arduino GPIO OUTPUT/HIGH/LOW,
LEDC 3.x pin-based attach/write and return codes, SoftAP/NetworkServer acceptance,
GPIO12 flash-voltage strapping, and ESP Timer task-dispatched one-shot callbacks.
APIs must compile against the installed Arduino-ESP32 3.x framework. This document
is a bench procedure, not a claim of physical testing.

HTTP responses use bounded nonblocking socket sends, with partial writes retried
on later loop iterations. The installed NetworkClient inherits a zero-returning
Print::availableForWrite(), so that method must not gate response transmission.
Espressif's socket send/MSG_DONTWAIT and errno handling were consulted for this fix.

## Command and SLEEP diagnostics

- Authenticated STOP/E-stop requests are accepted even with an old sequence number.
  They disarm the next pulse; movement, arm, reset and diagnostic requests still
  require increasing sequence numbers. Invalid session tokens never authorize commands.
- The page polls software state every 300 ms: armed, active pulse, diagnostic,
  stopped, latched fault, and the last command/result. Serial also reports accepted
  or rejected commands and cutoff completion. Neither proves physical wheel motion
  or measures pin voltage. A 300 ms pulse may finish between status polls.
- A delayed arm response cannot re-enable browser controls after STOP. Network
  errors disable browser movement controls and mark state unverified.
- For a meter test, physically disconnect the motor supply, retain ESP32 USB power
  and common ground, and check the separate motor-power-disconnected confirmation.
  Select **Test SLEEP pins for 3 seconds**. All four motor inputs are first set LOW;
  GPIO18 (right) and GPIO19 (left) go HIGH for at most the requested 3 seconds.
  Measure each against ESP32 GND: approximately 3.3 V while enabled, then 0 V.
  STOP/E-stop, AP station loss, timer cutoff and loop cutoff disable both drivers.
  The checkbox is an operator confirmation, not electrical detection of supply power.
- No movement or re-arm is accepted during the diagnostic. It cannot extend itself.
  GPIO18/19 are not held permanently HIGH. Right motor inputs use digital HIGH/LOW
  without LEDC/PWM; normal movement remains limited to 300 ms.

Validation checklist (requires hardware; not performed by compilation):
1. With motor power disconnected, check both SLEEP voltages during the diagnostic
   and after expiry, STOP, E-stop, and AP disconnect. Verify no motor input is driven.
2. Check diagnostic refusal without confirmation, during an active test, or with
   E-stop latched. Reset must not arm or start anything.
3. With wheels secured off the ground, restore motor power and test right Front/Back,
   early release, 300 ms expiry, STOP/E-stop and explicit re-arm. Verify the left
   driver stays disabled throughout right-side motion.
4. Replay an old authenticated STOP request: it must stop/disarm, never return a
   stale-sequence rejection. An invalid token or stale movement must be rejected.

Official ESP Timer one-shot/start/stop and task callback documentation was consulted
for the bounded diagnostic. The callback only disables drivers; it never starts them.
