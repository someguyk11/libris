#include "AtlasConfig.h"
#include "MotionController.h"
#include "SensorManager.h"
#include "SafetyController.h"
#include "Navigation.h"
#include "TelemetryManager.h"
#include "ImuManager.h"

MotionController motion;
SensorManager sensors;
ImuManager imu;
SafetyController safety(motion, sensors);
CheckpointManager checkpoints;
Routes routes(motion, sensors, checkpoints);
ObstacleAvoidance obstacles;
Navigation navigation(motion, sensors, safety, routes, checkpoints, obstacles);
TelemetryManager telemetry(motion, sensors, safety, navigation);

// Fixed buffers and availableForWrite avoid waiting for serial TX room in the
// real-time loop. Diagnostics may be dropped; safety checks must not be.
void logLine(const char* message) {
    if (!AtlasConfig::DiagnosticsEnabled) return;
    const size_t length = strlen(message);
    if (Serial.availableForWrite() >= static_cast<int>(length))
        Serial.write(reinterpret_cast<const uint8_t*>(message), length);
}

void handleTestInput() {
    if (!AtlasConfig::HardwareTestMode || !Serial.available()) return;
    // One byte per loop, no readString, timeout or unbounded serial-drain loop.
    const char command = Serial.read();
    if (command == 'x') { safety.emergencyStop(); return; }
    if (command == 'r') {
        logLine(navigation.resetSafety(millis()) ? "Reset accepted; no automatic resume.\n" : "Reset denied.\n");
        return;
    }
    if (command == 's') { navigation.cancel(); return; }
    bool accepted = false;
    switch (command) {
        case 'f': accepted = navigation.driveDistance(AtlasConfig::TestDistanceCm); break;
        case 'b': accepted = navigation.driveDistance(-AtlasConfig::TestDistanceCm); break;
        case 'c': accepted = navigation.turn90(true); break;
        case 'a': accepted = navigation.turn90(false); break;
        case 'n': accepted = navigation.beginRecording(); break;
        case 'e': accepted = navigation.finishRecording(); break;
        case 'p': accepted = navigation.replay(); break;
        case 'h': accepted = navigation.pause(); break;
        case 'u': accepted = navigation.resume(); break;
        default: return;
    }
    logLine(accepted ? "Command accepted.\n" : "Command rejected: check state/safety/calibration/dwell.\n");
}

void diagnostics(uint32_t nowMs) {
    static uint32_t lastLogMs = 0;
    static uint8_t page = 0;
    if (!AtlasConfig::DiagnosticsEnabled || nowMs - lastLogMs < AtlasConfig::DiagnosticIntervalMs) return;
    lastLogMs = nowMs;
    char line[112];
    if (page == 0) {
        const auto p = motion.progress();
        snprintf(line, sizeof(line), "safety=%s latch=%u motion=%u progress=%.0f%% L=%.1f R=%.1f cm\n",
            atlasFaultName(safety.fault()), static_cast<unsigned>(safety.latched()),
            static_cast<unsigned>(p.state), p.fraction * 100.0f, p.leftCm, p.rightCm);
    } else if (page == 1) {
        uint32_t left, right;
        motion.encoderCounts(left, right);
        snprintf(line, sizeof(line), "encoder unsigned edges L=%lu R=%lu; distance=%u turns=%u calibrated\n",
            static_cast<unsigned long>(left), static_cast<unsigned long>(right),
            static_cast<unsigned>(AtlasConfig::distanceCalibrated()),
            static_cast<unsigned>(AtlasConfig::turnsCalibrated()));
    } else if (page == 2) {
        snprintf(line, sizeof(line), "nav=%s err=%u pause=%u route=%u/%u err=%u checkpoints=%u retries=%u\n",
            Navigation::stateName(navigation.state()), static_cast<unsigned>(navigation.error()),
            static_cast<unsigned>(navigation.pauseReason()), static_cast<unsigned>(routes.index()),
            static_cast<unsigned>(routes.count()), static_cast<unsigned>(routes.error()),
            static_cast<unsigned>(checkpoints.validatedCount()), static_cast<unsigned>(checkpoints.retries()));
    } else if (page == AtlasConfig::SensorCount + 3) {
        const auto value = imu.reading(nowMs);
        snprintf(line, sizeof(line), "imu diagnostic valid=%u error=%u age=%lu ms (not a navigation/safety input)\n",
            static_cast<unsigned>(value.valid), static_cast<unsigned>(value.error),
            static_cast<unsigned long>(nowMs - value.timestampMs));
    } else {
        const uint8_t index = page - 3;
        const auto reading = sensors.reading(static_cast<AtlasConfig::SensorId>(index), nowMs);
        snprintf(line, sizeof(line), "sensor=%u valid=%u cm=%.1f error=%u failures=%u fault=%u\n",
            static_cast<unsigned>(index), static_cast<unsigned>(reading.valid), reading.distanceCm,
            static_cast<unsigned>(reading.error), static_cast<unsigned>(reading.consecutiveFailures),
            static_cast<unsigned>(reading.faulted));
    }
    page = (page + 1) % (AtlasConfig::SensorCount + 4);
    logLine(line);
}

void setup() {
    if (AtlasConfig::DiagnosticsEnabled || AtlasConfig::HardwareTestMode) Serial.begin(AtlasConfig::SerialBaud);
    motion.begin();
    sensors.begin();
    const bool imuReady = imu.begin();
    safety.begin(millis());
    navigation.begin(millis());
    const bool networkWorkerReady = telemetry.begin();
    if (AtlasConfig::DiagnosticsEnabled) {
        // Startup only: there is no motion permission here.
        Serial.println("Atlas Layers 1+2+3; output disabled until configuration and sensors are safe.");
        if (!networkWorkerReady) Serial.println("Network worker unavailable; backend movement disabled.");
        if (!imuReady) Serial.println("IMU diagnostics unavailable: check explicit bus configuration.");
        if (!BackendClient::configured()) Serial.println("Backend disabled: provision private device key and trusted TLS CA.");
        const char* error = AtlasConfig::pinConfigurationError();
        if (error) Serial.println(error);
        error = AtlasConfig::tuningError();
        if (error) Serial.println(error);
        error = AtlasConfig::navigationTuningError();
        if (error) Serial.println(error);
        if (!AtlasConfig::distanceCalibrated()) Serial.println("Encoder convention/CPR/tolerance unconfigured.");
        if (!AtlasConfig::turnsCalibrated()) Serial.println("90-degree turns require track and CW/CCW calibration.");
        if (AtlasConfig::HardwareTestMode) {
            Serial.println("Tests: f/b distance, c/a turn, s cancel, x E-stop, r reset.");
            Serial.println("Layer 2: n record, e finish, p replay, h pause, u resume (after reset if latched).");
        }
    }
}

void loop() {
    // Check loop latency BEFORE accepting a newly captured sensor reading.
    safety.update(millis());
    sensors.update(millis());
    safety.update(millis());
    handleTestInput();
    safety.update(millis());
    navigation.update(millis());
    telemetry.update(millis()); // bounded queue handoff only; no Wi-Fi/HTTP in this task.
    safety.update(millis()); // check the newly selected motion's required directions before output.
    motion.update(millis());
    safety.update(millis()); // latch driver/control failures immediately.
    navigation.update(millis()); // observe completion or safety stop without delaying checks.
    diagnostics(millis());
}
