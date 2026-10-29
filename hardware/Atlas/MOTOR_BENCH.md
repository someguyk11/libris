# Isolated motor bench test

This build intentionally permits the supplied GPIO12 wiring and does not initialize
ultrasonics or depend on encoders. It does **not** change the normal firmware's
pin policy, safety checks, navigation, recording or replay. Do not use it on the floor.

## Build / install

From `hardware/Atlas`:

```powershell
pio run -e atlas-motor-bench
pio run -e atlas-motor-bench -t upload
```

Upload replaces the running application. To return to normal firmware, build/upload
`atlas-local-drive` again; its incomplete hardware configuration still inhibits motion.

1. Disconnect motor power while checking the wiring. Secure the chassis with **all
   wheels lifted**, and keep a physical motor-power disconnect accessible.
2. Inputs: left IN1/3 = GPIO27, IN2/4 = GPIO33; right IN1/3 = GPIO26,
   IN2/4 = GPIO12. Separate nSLEEP controls = GPIO19 and GPIO18, driven together.
   Only matching bridge **inputs** are fanned out; never connect bridge outputs together.
3. Confirm a common ground, correct motor supply/current capacity, and hardware
   pull-downs for safe reset. ESP32 signals are 3.3 V, not 5 V tolerant.
4. GPIO12 still selects the flash voltage at reset: on a 3.3 V-flash module it must
   not be externally pulled HIGH during reset. Software cannot correct its reset
   level. No eFuses are changed by this build.
5. Join `Libot_Setup`, password `AtlasDrive123`, then open
   `http://192.168.4.1/drive`. The page MUST say **Atlas motor bench ONLY**.
6. Confirm wheels lifted, then arm one pulse. Press a direction. Release stops
   early; otherwise the requested cutoff is 300 ms. Re-arm for each new test.
   Low PWM duty is 70/255 (~27%). No guaranteed wheel motion at this duty.
7. Check each side's physical direction, STOP, E-stop/reset, release, and AP loss.
   Left/right are uncalibrated direction pulses, **not 90-degree turns**.
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
