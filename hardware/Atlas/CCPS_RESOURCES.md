# CCPS resource and release evidence

## Release verdict — FAIL (2026-10-27)

The hard gate is the **complete application `firmware.bin` ≤ 1,000,000 bytes**
(decimal, not 1 MiB, ELF size or partition capacity).

- Final main image: **1,053,072 bytes**, **53,072 bytes over**.
- Feature-retaining inert probe: **1,190,000 bytes**, **190,000 bytes over**.
- The ordinary ESP32 build succeeds because the existing factory partition is
  3,145,728 bytes. That capacity check does **not** satisfy the separate 1 MB gate.
- Unknown hardware/calibration and empty credentials allow dead-code elimination
  in the main image. It is not a provisioned production-size guarantee.
- No image was uploaded. Probes must **never** be deployed or hardware-tested.

## Measurements

| Build/profile | Complete image | Application sections | Static RAM | 1 MB gate |
| --- | ---: | ---: | ---: | --- |
| Unchanged pre-CCPS baseline, original flags | 1,403,536 | 1,368,215 | 61,140 | FAIL |
| CCPS Milestone 11 snapshot, original flags | 1,450,256 | 1,414,031 | 63,156 | FAIL |
| Same pre-CCPS baseline, explicit `-Os` | 1,028,320 | 1,013,932 | 59,588 | FAIL |
| Final CCPS main, explicit `-Os` | **1,053,072** | **1,037,976** | **61,604** | **FAIL** |
| Final CCPS inert feature-retaining probe, `-Os` | **1,190,000** | **1,172,171** | **63,108** | **FAIL** |

All values are bytes. Baseline source revision:
`3261d89e17039aab74faff41ef9ca31ffaf01d0a`.
The Milestone 11 row is a historical snapshot, not the final source rebuilt without
optimization. Same-optimization comparison is the baseline `-Os` row versus final
main: **+24,752 image bytes**, **+24,044 application bytes**, **+2,016 static RAM
bytes**. The optimized baseline itself exceeds the gate by **28,320 bytes**.
The probe adds **136,928 image bytes** over the final unconfigured main image.

The complete image contains headers/alignment/metadata not represented identically
by PlatformIO's application section total. Static RAM excludes runtime worker
stacks, queue allocations, Wi-Fi/TLS heap and peak scratch space. Neither number
establishes safe runtime memory margins.

### Artifact identity

| Artifact | SHA-256 |
| --- | --- |
| Original baseline `firmware.bin` | `f771b0717e0cb477c39289c2df239dcc0379cea18c89d832203da516d5040cab` |
| Optimized baseline `firmware.bin` | `2d060c77c712738ae434ed2acf24599b73929856f794b6e94218a9356bbbbf4b` |
| Final main `firmware.bin` | `b5b70ce15b53a6cd7a0349c252efc4076a7a34c2aa1ed885e0b48645edf322c8` |
| Final inert probe `firmware.bin` | `03972f5376931552d30bc23f4fdcaefc18d9a467d566551d4af2fd588954f55f` |

Main artifacts: `.pio/build/esp32dev/firmware.bin`, `firmware.elf`, `firmware.map`.
Final main build log:
`.pio-mcp-workspace/logs/build/build-2026-10-27T08-58-14-765Z-10f75a0f.log`.

Probe project:
`C:\Users\admin\AppData\Local\Temp\opencode\atlas-ccps-probe-features-release-review`.
Its final build log is
`.pio-mcp-workspace/logs/build/build-2026-10-27T08-59-57-668Z-098b8363.log`.
The optimized baseline project is alongside it at `atlas-ccps-probe-baseline`.
Temporary artifacts/logs are local evidence, not a committed firmware distribution.

Reviewable evidence files (pending commit with the implementation):

- `ccps_size_log.csv`: milestone image/application/RAM measurements and hashes;
  `12-final` is the final main row. Earlier `12-Os` is intentionally retained history.
- `ccps_resource_report.json`: final main image hash/size and unique map attribution.
- `ccps_feature_probe_report.json`: corresponding final inert-probe attribution.

Hashes identify the measured local binaries, not a promise of bit-identical future
builds: build timestamps, paths, provisioning and resolved packages may differ.

## Build environment and experiments

Measured with PlatformIO **6.2.0**, existing `esp32dev`, Espressif32 **8.0.0**,
Arduino-ESP32 **3.3.12**, ESP-IDF **5.5.5**, Xtensa GCC **14.2.0** and resolved
ArduinoJson **7.4.3**. The project retains its board/framework/dependency selection;
this work did not migrate frameworks or replace prebuilt SDK libraries.

**Accepted: explicit `-Os`.** The installed resolved Arduino build flags lacked an
optimization level. An isolated experiment demonstrated a large reduction without
removing features, TLS verification or safety checks. Only `-Os` was added to the
production flags. Full host suites pass at both `-O0` and `-Os` (220 reported
checks/cases each), and final main/probe ESP32 builds succeed. Optimization is not
a substitute for physical timing/accuracy tests.

**Rejected: LTO.** The isolated `-Os -flto` experiment failed to link, reporting
`plugin needed to handle lto object` and undefined `app_main` /
`__wrap_esp_panic_handler`. The installed framework linker response flags include
`-fno-lto`; simple project `build_unflags` did not produce a compatible LTO link.
There is no usable LTO image/size result and no LTO flag in production. Toolchain
or framework changes to enable it require a separate validated effort.

### Why retain features in a separate probe?

Compilers can eliminate unreachable motion/network paths when pins, physical
parameters or credentials are compile-time unconfigured. The probe replaces those
settings **only in a temporary source copy**, using explicitly synthetic values
and a deliberately invalid test CA/key. It enables optional test paths to retain
code, not to demonstrate a valid robot configuration or working authentication.

Original sketch entry points are renamed and rooted through the volatile
`atlas_resource_roots` table plus linker `-u`. The probe's actual `setup()` and
`loop()` only delay; they never call the rooted firmware. This is an inert link
measurement, not a production binary, runtime simulation or upper bound on every
future provisioned build.

Final review confirmed 38 non-configuration/non-entry-point source files match
main byte-for-byte and original entry-point bodies match after renaming. ELF
symbols confirm retention of both original entry points, the root table,
`ConfidenceSupervisor::update`, `ImuIntegration::sample`, `PoseEstimator::advance`,
`MotionController::update`, `Navigation::replayTicks` and `BackendClient::sendResult`.
No private provisioning header was copied into a probe.

## Flash contributors

Selected **unique allocated map-input bytes** from the final main report:

| Input/group | Attributed bytes |
| --- | ---: |
| `libnet80211.a` | 144,725 |
| `liblwip.a` | 119,286 |
| Shared merged string pool | 103,906 |
| `libc.a` | 84,457 |
| `libmbedcrypto.a` | 69,693 |
| `libpp.a` | 68,430 |
| `libwpa_supplicant.a` | 61,839 |
| Entire Atlas application input group | 61,496 |
| `libphy.a` | 43,526 |
| `libFrameworkArduino.a` | 26,112 |

This is not a sum of symbol aliases, nor a claim that each archive can be removed.
The parser excludes debug/noload inputs and padding/generated data, counts
overlapping ranges once and separates merged strings from their incidental map
owner. In particular, the whole string pool is **not** charged to the GPIO driver.
Other overlaps are charged to the first map entry, so attribution is approximate;
the binary file length remains the gate authority. The Atlas group includes all
application modules, not only CCPS.

RAM invariants: `MovementRecord` is statically asserted at **64 bytes** (ten slots,
640-byte payload), `Decision` at **44 bytes**; the supervisor is **1,760 bytes on
the tested host ABI**, with a compile-time persistent-state ceiling below 4 KiB.
That host size is not a target stack/heap high-water measurement.

## Reproduction without hardware

From `hardware/Atlas`, build the main image without uploading:

```sh
pio run -e esp32dev
python tests/host/analyze_size.py --summary --output ccps_resource_report.json
python tests/host/resources.py review --ram 61604 --flash 1037976 --release-check
```

The last command appends a measurement row and intentionally exits **1** for the
measured oversized image. Supply application/RAM values from **your same build**;
the tool reads the actual complete image, not a supplied image-size number.

Create new isolated copies (existing directories are never overwritten):

```sh
python tests/host/prepare_resource_build.py baseline --label recheck --revision 3261d89e17039aab74faff41ef9ca31ffaf01d0a
python tests/host/prepare_resource_build.py features --label recheck
```

The script prints directories beneath `%LOCALAPPDATA%/Temp/opencode`. Baseline
defaults to `HEAD` unless `--revision` is supplied; the command above fixes the
measured revision. Build each printed directory with
`pio run -d "<directory>" -e esp32dev` (no upload target), or PlatformIO MCP
`build_project` using a forward-slash project path. Analyze the probe with:

```sh
python tests/host/analyze_size.py --build-dir "<directory>/.pio/build/esp32dev" --summary --output ccps_feature_probe_report.json
```

No resource script opens a serial port, flashes hardware or contacts the backend.
The feature profile must never be used as deployment configuration.

## Remaining release work

1. Find and validate at least **53,072 bytes** of further reduction for the current
   unconfigured image; the feature probe needs **190,000 bytes**, and intended
   provisioning/calibration can change the result. No production-size pass exists.
2. Prefer measured application/serialization/formatting reductions that preserve
   semantics. Investigate shared strings/library reachability before guessing.
   Rebuild, rerun tests and retain the feature probe for each candidate.
3. A custom SDK/prebuilt-library configuration or compatible LTO toolchain might
   offer larger savings, but is **not applied or approved by this report**. Treat
   it as separate work requiring framework/security/timing regression validation.
   Do not disable TLS, local safety, Protocol-3 checks, calibration gates or the
   confidence supervisor to achieve a cosmetic size pass.
4. Measure actual ESP32 CCPS/loop timing, worker/loop stacks, heap/largest-block
   margins and physical performance. These remain unknown, not inferred from the
   build or host tests. Follow [CCPS_IMPLEMENTATION.md](CCPS_IMPLEMENTATION.md).
