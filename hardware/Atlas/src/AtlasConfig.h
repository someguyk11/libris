#pragma once

#include <Arduino.h>
#include <math.h>
#include <esp_arduino_version.h>

// Optional private provisioning; never commit this file or print its contents.
#if __has_include("AtlasSecrets.local.h")
#include "AtlasSecrets.local.h"
#endif
#ifndef ATLAS_DEVICE_KEY
#define ATLAS_DEVICE_KEY ""
#endif
#ifndef ATLAS_TLS_CA
#define ATLAS_TLS_CA ""
#endif

#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "Atlas V1 requires the classic ESP32 (DevKit V1), not another ESP32 variant."
#endif
#if ESP_ARDUINO_VERSION_MAJOR != 2 && ESP_ARDUINO_VERSION_MAJOR != 3
#error "Atlas LEDC adapter supports Arduino-ESP32 2.x and 3.x only."
#endif

namespace AtlasConfig {
constexpr int UnconfiguredPin = -1;
enum class EncoderEdges : uint8_t { Unconfigured, Rising, Falling, Both };
enum class StopMode : uint8_t { Coast, Brake };
enum class SensorId : uint8_t { Front, Back, Left, Right, Count };
constexpr uint8_t SensorCount = static_cast<uint8_t>(SensorId::Count);

struct MotorPins { int in1; int in2; bool inverted; };
struct SensorPins { int trigger; int echo; };

// Each side has TWO separate H-bridges, one per motor. Fan out each GPIO
// to the corresponding input on both bridges. NEVER tie bridge outputs
// together. Confirm motor polarity, driver current capacity and nSLEEP wiring.
constexpr MotorPins LeftMotor = {UnconfiguredPin, UnconfiguredPin, false};
constexpr MotorPins RightMotor = {UnconfiguredPin, UnconfiguredPin, false};
constexpr bool MotorInputFanoutConfirmed = false;
// Optional common nSLEEP control for both drivers; otherwise explicitly confirm
// that both nSLEEP inputs are externally held awake. Hardware pull-downs on
// bridge inputs / nSLEEP are required for safe reset and unconfigured firmware.
constexpr int DriverSleepPin = UnconfiguredPin;
constexpr bool DriverSleepExternallyManaged = false;
constexpr int LeftEncoderPin = UnconfiguredPin;
constexpr int RightEncoderPin = UnconfiguredPin;
constexpr EncoderEdges CountEdges = EncoderEdges::Unconfigured;
constexpr bool EncoderInternalPullup = false;
constexpr uint32_t EncoderMinEdgeIntervalUs = 0; // 0 disables filtering; measure first.
// This implementation counts ONE signal per side, unsigned edges, not quadrature.
// Set true only after confirming this matches the installed encoder interface.
constexpr bool SingleSignalEncodersConfirmed = false;
constexpr SensorPins Sensors[SensorCount] = {
    {UnconfiguredPin, UnconfiguredPin}, // front
    {UnconfiguredPin, UnconfiguredPin}, // back
    {UnconfiguredPin, UnconfiguredPin}, // left
    {UnconfiguredPin, UnconfiguredPin}  // right
};
// ESP32 inputs are not 5 V tolerant. Confirm echo level conversion and sensor
// trigger compatibility for the actual ultrasonic model before enabling GPIO.
constexpr bool SensorElectricalInterfaceConfirmed = false;

// Optional LSM6DSO diagnostics only: V1 safety/navigation uses encoders and
// ultrasonics, not attitude estimates. No inferred wiring or bus address.
constexpr bool ImuDiagnosticsEnabled = false;
constexpr int ImuSdaPin = UnconfiguredPin;
constexpr int ImuSclPin = UnconfiguredPin;
constexpr uint8_t ImuAddress = 0; // confirm SA0: 0x6A or 0x6B.
constexpr bool ImuElectricalInterfaceConfirmed = false;
constexpr uint32_t ImuBusHz = 100000;
constexpr uint16_t ImuBusTimeoutMs = 10;
constexpr uint32_t ImuSampleIntervalMs = 20;
constexpr uint32_t ImuMaximumAgeMs = 200;

constexpr uint32_t PwmFrequencyHz = 20000;
constexpr uint8_t PwmResolutionBits = 8;
constexpr uint8_t FirstPwmChannel = 0; // core 2.x: reserves four consecutive channels.
constexpr uint16_t MaxDuty = 100;      // duty counts, not percent; conservative starting limit.
constexpr uint16_t CruiseDuty = 70;
constexpr uint16_t TurnDuty = 60;
constexpr uint16_t MinimumDriveDuty = 40; // tune on blocks; must reliably rotate wheels.
constexpr float AccelerationDutyPerSecond = 160.0f;
constexpr float DecelerationDutyPerSecond = 240.0f;
constexpr StopMode NormalStopMode = StopMode::Coast;
constexpr StopMode EmergencyStopMode = StopMode::Coast;

constexpr float WheelDiameterCm = 6.5f;
// EFFECTIVE edge counts per WHEEL revolution using CountEdges, not the encoder
// manufacturer's ambiguous PPR / motor-shaft counts. Zero means unconfigured.
constexpr float LeftCountsPerWheelRevolution = 0.0f;
constexpr float RightCountsPerWheelRevolution = 0.0f;
constexpr float WheelTrackCm = 0.0f; // measure contact-patch centre to centre.
constexpr float ClockwiseTurnScale = 0.0f; // measure skid-steer correction; no assumed calibration.
constexpr float CounterclockwiseTurnScale = 0.0f;
constexpr float DistanceToleranceCm = 0.5f;
constexpr float SlowdownDistanceCm = 8.0f;
constexpr float MaximumMoveCm = 200.0f;
constexpr uint32_t DirectionSettleMs = 100; // stopped dwell before a new/reversed command.

constexpr float SensorMinCm = 2.0f;
constexpr float SensorMaxCm = 300.0f;
constexpr float SoundSpeedCmPerUs = 0.0343f; // tune for measured ambient conditions.
constexpr float ObstacleStopCm[SensorCount] = {25.0f, 25.0f, 20.0f, 20.0f};
constexpr float ObstacleHysteresisCm = 5.0f;
constexpr uint32_t EchoTimeoutUs = 25000;
constexpr uint32_t TriggerPulseUs = 10; // only bounded microsecond delay in the control path.
constexpr uint32_t SensorSlotIntervalMs = 60; // one sensor per slot, not four simultaneous pings.
constexpr uint32_t MaximumReadingAgeMs = 400;
constexpr uint16_t SensorFaultAfterFailures = 3;
// All four readings must be valid even for straight travel by default. Turning
// always requires all four. A bad required reading stops at the FIRST failure;
// the failure count is diagnostic, never a grace period for driving blind.
constexpr bool RequireAllSensorsForStraightMotion = true;
constexpr bool LatchSensorFaults = true;
constexpr bool LatchObstacleStops = true;

constexpr uint32_t ControlIntervalMs = 5;
constexpr uint32_t MaximumControlGapMs = 50;
constexpr uint32_t StallTimeoutMs = 750;
constexpr uint32_t StallStartupGraceMs = 500;
constexpr uint16_t StallMinimumDuty = MinimumDriveDuty;
constexpr uint32_t MovementTimeoutMs = 20000;

// Layer 2 uses bounded RAM storage. Timeouts only bound waiting; they never
// determine distance or turn angle. Tune checkpoint/sensor tolerances on site.
constexpr uint8_t MaximumRouteSegments = 32;
constexpr uint32_t RouteStartTimeoutMs = 3000;
constexpr uint32_t RouteSegmentTimeoutMs = 60000; // includes stops and checkpoint checks.
constexpr uint32_t RouteTimeoutMs = 600000;       // includes user/obstacle pauses.
constexpr float CheckpointDistanceToleranceCm = 2.0f;
constexpr float CheckpointWheelMismatchCm = 2.0f;
constexpr float CheckpointSensorToleranceCm = 35.0f;
constexpr uint32_t CheckpointSettleMs = 200;
constexpr uint32_t CheckpointAttemptTimeoutMs = 1200;
constexpr uint8_t CheckpointMaxRetries = 2; // extra observations, NOT corrective moves.
constexpr uint32_t ObstacleClearanceMs = 1000;
constexpr uint32_t ObstacleWaitTimeoutMs = 15000;
constexpr uint8_t MaximumObstacleInterruptions = 3;
// Existing Layer 1 stop thresholds + hysteresis also define Layer 2 clearance.
// A latched stop ALWAYS requires explicit reset AND a separate resume request.
constexpr bool AutoResumeAfterObstacle = true; // only for non-latching stops.

// Layer 3: all network I/O runs in one low-priority worker, never the control loop.
constexpr char BackendBaseUrl[] = "https://libot-8okb.onrender.com";
constexpr char RobotId[] = "robot1";
constexpr char FirmwareVersion[] = "atlas-v1-layer3";
constexpr const char* DeviceKey = ATLAS_DEVICE_KEY;
constexpr const char* TlsCa = ATLAS_TLS_CA;
constexpr float BackendStepCm = 0.0f; // Intentionally UNCONFIGURED; no assumed distance.
constexpr uint32_t CommandPollIntervalMs = 250;
constexpr uint32_t CommandLeaseMs = 900; // conservative, below backend's 1 s movement TTL.
constexpr uint32_t MaximumCommandResponseMs = 500;
constexpr uint32_t HttpConnectTimeoutMs = 800;
constexpr uint16_t HttpReadTimeoutMs = 800;
constexpr uint32_t HttpBodyDeadlineMs = 800;
constexpr uint32_t TlsHandshakeTimeoutSeconds = 3;
constexpr size_t MaximumHttpResponseBytes = 4096;
constexpr size_t MaximumHttpRequestBytes = 1536;
constexpr uint32_t BackendRetryInitialMs = 500;
constexpr uint32_t BackendRetryMaximumMs = 8000;
constexpr uint32_t TelemetryIntervalMs = 5000;
constexpr uint32_t RecordingSensorIntervalMs = 1000;
constexpr uint8_t ResultQueueCapacity = 8;
constexpr uint8_t RecentCommandCapacity = 16;
constexpr uint8_t ResultRetryLimit = 3;
constexpr uint32_t ResultRetryMs = 500;
constexpr uint32_t NetworkWorkerStackBytes = 16384;
constexpr uint8_t NetworkWorkerPriority = 1;
constexpr uint32_t NetworkWorkerTickMs = 5;
constexpr int NetworkWorkerCore = 0; // Arduino control loop remains on the other core.
constexpr char WifiHostname[] = "atlas-v1";
constexpr char SetupSsid[] = "Libot_Setup";
constexpr uint8_t SetupIp[4] = {192, 168, 4, 1};
constexpr uint32_t WifiAttemptTimeoutMs = 10000;
constexpr uint32_t WifiRetryInitialMs = 1000;
constexpr uint32_t WifiRetryMaximumMs = 30000;
constexpr uint32_t PortalCloseGraceMs = 3000;
constexpr uint32_t PortalClientTimeoutMs = 1500;
constexpr uint32_t PortalScanIntervalMs = 5000;
constexpr size_t PortalRequestBytes = 1024;
constexpr size_t PortalMaximumBodyBytes = 256;
constexpr size_t PortalResponseBytes = 3072;
constexpr uint8_t PortalMaximumScanResults = 12;
constexpr uint32_t PortalWriteTimeoutMs = 100;
constexpr uint16_t WifiEepromBytes = 512;
constexpr uint8_t WifiProfileCount = 5;
constexpr uint8_t WifiProfileBytes = 96;
constexpr uint16_t LegacyOtaFlagAddress = 500;
constexpr char NtpServer[] = "pool.ntp.org";
constexpr time_t MinimumUtcEpoch = 1767225600; // 2026-01-01; never fabricate API timestamps.

constexpr bool DiagnosticsEnabled = true;
constexpr uint32_t SerialBaud = 115200;
constexpr uint32_t DiagnosticIntervalMs = 1000;
constexpr bool HardwareTestMode = false; // no automatic movement, even when true.
constexpr float TestDistanceCm = 20.0f;

inline bool positive(float value) { return isfinite(value) && value > 0.0f; }
// Conservative DevKit pin policy: exclude flash, boot straps, UART0 and
// GPIO16/17 (module-dependent PSRAM). No silent acceptance of hazardous pins.
inline bool inputPinAllowed(int pin) {
    switch (pin) {
        case 4: case 13: case 14: case 18: case 19: case 21: case 22:
        case 23: case 25: case 26: case 27: case 32: case 33:
        case 34: case 35: case 36: case 39: return true;
        default: return false;
    }
}
inline bool outputPinAllowed(int pin) { return inputPinAllowed(pin) && pin < 34; }
inline const char* pinConfigurationError() {
    const int pins[] = {LeftMotor.in1, LeftMotor.in2, RightMotor.in1, RightMotor.in2,
        LeftEncoderPin, RightEncoderPin, Sensors[0].trigger, Sensors[0].echo,
        Sensors[1].trigger, Sensors[1].echo, Sensors[2].trigger, Sensors[2].echo,
        Sensors[3].trigger, Sensors[3].echo, DriverSleepPin};
    for (uint8_t i = 0; i < 15; ++i) {
        if (i == 14 && pins[i] == UnconfiguredPin) continue;
        const bool output = i < 4 || (i >= 6 && i % 2 == 0);
        if (!(output ? outputPinAllowed(pins[i]) : inputPinAllowed(pins[i])))
            return "GPIO missing or excluded by DevKit pin policy";
        for (uint8_t j = 0; j < i; ++j)
            if (pins[i] == pins[j]) return "GPIO assignments overlap";
    }
    if (EncoderInternalPullup && (LeftEncoderPin >= 34 || RightEncoderPin >= 34))
        return "GPIO34-39 require external encoder pull-ups";
    if (!MotorInputFanoutConfirmed) return "Confirm separate H-bridge input fanout and motor wiring";
    if (!SensorElectricalInterfaceConfirmed) return "Confirm ultrasonic 3.3 V electrical interface";
    if (DriverSleepPin != UnconfiguredPin && !outputPinAllowed(DriverSleepPin))
        return "nSLEEP requires an output-capable GPIO";
    if (DriverSleepPin == UnconfiguredPin && !DriverSleepExternallyManaged)
        return "Configure nSLEEP or confirm external management";
    return nullptr;
}
inline const char* tuningError() {
    if (PwmResolutionBits < 1 || PwmResolutionBits > 14 || PwmFrequencyHz == 0 ||
        FirstPwmChannel > 12 || MaxDuty == 0 || MaxDuty >= (1UL << PwmResolutionBits) ||
        CruiseDuty < MinimumDriveDuty || CruiseDuty > MaxDuty ||
        TurnDuty < MinimumDriveDuty || TurnDuty > MaxDuty || MinimumDriveDuty == 0 ||
        !positive(AccelerationDutyPerSecond) || !positive(DecelerationDutyPerSecond))
        return "Invalid PWM, speed limit or ramp settings";
    if (!positive(WheelDiameterCm) || !positive(DistanceToleranceCm) ||
        !positive(SlowdownDistanceCm) || !positive(MaximumMoveCm) ||
        MaximumMoveCm <= DistanceToleranceCm ||
        !positive(SensorMinCm) || !positive(SensorMaxCm) || SensorMaxCm <= SensorMinCm ||
        !positive(SoundSpeedCmPerUs) || !positive(ObstacleHysteresisCm))
        return "Invalid distance/range settings";
    if (ControlIntervalMs == 0 || MaximumControlGapMs <= ControlIntervalMs ||
        StallTimeoutMs <= MaximumControlGapMs || MovementTimeoutMs <= StallStartupGraceMs ||
        StallMinimumDuty == 0 || StallMinimumDuty > MinimumDriveDuty ||
        EchoTimeoutUs < SensorMaxCm * 2.0f / SoundSpeedCmPerUs || EchoTimeoutUs > 50000 ||
        TriggerPulseUs < 10 || TriggerPulseUs > 20 ||
        SensorSlotIntervalMs * 1000ULL <= EchoTimeoutUs ||
        MaximumReadingAgeMs <= SensorCount * SensorSlotIntervalMs ||
        MaximumReadingAgeMs > 2000 || SensorFaultAfterFailures == 0 ||
        DirectionSettleMs == 0 || DiagnosticIntervalMs == 0 || SerialBaud == 0 ||
        !positive(TestDistanceCm) || TestDistanceCm <= DistanceToleranceCm || TestDistanceCm > MaximumMoveCm)
        return "Invalid control, stall, test or ultrasonic timing settings";
    for (uint8_t i = 0; i < SensorCount; ++i)
        if (!positive(ObstacleStopCm[i]) || ObstacleStopCm[i] < SensorMinCm ||
            ObstacleStopCm[i] + ObstacleHysteresisCm >= SensorMaxCm)
            return "Invalid obstacle threshold/hysteresis";
    return nullptr;
}
inline bool distanceCalibrated() {
    return SingleSignalEncodersConfirmed && CountEdges != EncoderEdges::Unconfigured &&
        positive(LeftCountsPerWheelRevolution) && positive(RightCountsPerWheelRevolution) &&
        // Distance commands stop at the first edge inside tolerance or beyond the target.
        // One-edge quantization must fit the EXISTING endpoint acceptance bound.
        (3.141592654f * WheelDiameterCm / LeftCountsPerWheelRevolution <= CheckpointDistanceToleranceCm) &&
        (3.141592654f * WheelDiameterCm / RightCountsPerWheelRevolution <= CheckpointDistanceToleranceCm);
}
inline const char* navigationTuningError() {
    // All elapsed-time comparisons are unsigned and bounded below half rollover.
    if (MaximumRouteSegments == 0 || RouteStartTimeoutMs <= DirectionSettleMs ||
        RouteSegmentTimeoutMs <= RouteStartTimeoutMs ||
        RouteTimeoutMs <= RouteSegmentTimeoutMs || RouteTimeoutMs >= 0x80000000UL ||
        !positive(CheckpointDistanceToleranceCm) ||
        CheckpointDistanceToleranceCm < DistanceToleranceCm ||
        !positive(CheckpointWheelMismatchCm) || !positive(CheckpointSensorToleranceCm) ||
        CheckpointSensorToleranceCm >= SensorMaxCm ||
        CheckpointSettleMs < DirectionSettleMs ||
        CheckpointAttemptTimeoutMs <= CheckpointSettleMs + MaximumReadingAgeMs ||
        (static_cast<uint64_t>(CheckpointMaxRetries) + 1) * CheckpointAttemptTimeoutMs >= RouteSegmentTimeoutMs ||
        ObstacleClearanceMs < MaximumReadingAgeMs ||
        ObstacleWaitTimeoutMs <= ObstacleClearanceMs ||
        ObstacleWaitTimeoutMs >= RouteSegmentTimeoutMs || MaximumObstacleInterruptions == 0)
        return "Invalid route, checkpoint or obstacle limits";
    return nullptr;
}
inline bool turnsCalibrated() {
    return distanceCalibrated() && positive(WheelTrackCm) &&
        positive(ClockwiseTurnScale) && positive(CounterclockwiseTurnScale);
}
} // namespace AtlasConfig

enum class AtlasFault : uint8_t {
    None, Configuration, Pwm, Obstacle, Sensor, StallLeft, StallRight,
    MovementTimeout, EmergencyStop, ControlOverrun, EncoderTarget
};
inline const char* atlasFaultName(AtlasFault fault) {
    switch (fault) {
        case AtlasFault::None: return "OK";
        case AtlasFault::Configuration: return "CONFIG";
        case AtlasFault::Pwm: return "PWM";
        case AtlasFault::Obstacle: return "OBSTACLE";
        case AtlasFault::Sensor: return "SENSOR";
        case AtlasFault::StallLeft: return "STALL_LEFT";
        case AtlasFault::StallRight: return "STALL_RIGHT";
        case AtlasFault::MovementTimeout: return "MOVE_TIMEOUT";
        case AtlasFault::EmergencyStop: return "E_STOP";
        case AtlasFault::ControlOverrun: return "CONTROL_GAP";
        case AtlasFault::EncoderTarget: return "ENCODER_TARGET";
    }
    return "UNKNOWN";
}
