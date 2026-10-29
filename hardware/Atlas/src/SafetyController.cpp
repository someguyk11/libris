#include "SafetyController.h"

using namespace AtlasConfig;

void SafetyController::begin(uint32_t nowMs) {
    lastUpdateMs_ = nowMs;
    motion_.setSafetyPermit(false);
    if (motion_.fault() != AtlasFault::None) {
        trip(motion_.fault(), true);
        return;
    }
    if (!motion_.ready() || !sensors_.ready() || !distanceCalibrated() ||
        pinConfigurationError() || tuningError()) trip(AtlasFault::Configuration, true);
}

AtlasFault SafetyController::inspectSensors(uint32_t nowMs, bool requireAll) {
    if (EncoderOnlyGroundTest) return AtlasFault::None; // explicit test build only
    bool invalid = false;
    bool obstructed = false;
    const bool moving = motion_.moving();
    const auto kind = motion_.kind();
    const bool turning = kind == MotionController::Kind::Clockwise90 ||
        kind == MotionController::Kind::Counterclockwise90;
    for (uint8_t i = 0; i < SensorCount; ++i) {
        const SensorManager::Reading reading = sensors_.reading(static_cast<SensorId>(i), nowMs);
        const bool required = requireAll || !moving || turning || RequireAllSensorsForStraightMotion ||
            (kind == MotionController::Kind::Forward && i == static_cast<uint8_t>(SensorId::Front)) ||
            (kind == MotionController::Kind::Backward && i == static_cast<uint8_t>(SensorId::Back));
        if (!reading.valid) {
            blocked_[i] = true;
            if (required) invalid = true;
            continue;
        }
        if (reading.distanceCm <= ObstacleStopCm[i] && ObstacleAvoidanceEnabled) blocked_[i] = true;
        else if (reading.distanceCm >= ObstacleStopCm[i] + ObstacleHysteresisCm && ObstacleAvoidanceEnabled) blocked_[i] = false;
        if (required && blocked_[i]) obstructed = true;
    }
    // Invalid data takes precedence over interpreting an old obstacle state.
    return invalid ? AtlasFault::Sensor : (obstructed ? AtlasFault::Obstacle : AtlasFault::None);
}

void SafetyController::trip(AtlasFault fault, bool latch) {
    if (latched_) return; // preserve the first critical cause for diagnosis.
    fault_ = fault;
    latched_ = latch;
    permitted_ = false;
    motion_.emergencyStop(fault);
}

void SafetyController::emergencyStop() { trip(AtlasFault::EmergencyStop, true); }

bool SafetyController::reset(uint32_t nowMs) {
    permitted_ = false;
    if (motion_.moving() || !motion_.ready() || !sensors_.ready() ||
        pinConfigurationError() || tuningError() || !distanceCalibrated()) return false;
    const AtlasFault sensorFault = inspectSensors(nowMs, true);
    if (sensorFault != AtlasFault::None) return false;
    motion_.clearFailure();
    motion_.setSafetyPermit(false); // reset clears a latch, it does not issue a motion command.
    fault_ = AtlasFault::None;
    latched_ = false;
    wasMoving_ = false;
    lastUpdateMs_ = nowMs;
    return true;
}

void SafetyController::update(uint32_t nowMs) {
    const uint32_t gap = nowMs - lastUpdateMs_;
    lastUpdateMs_ = nowMs;
    if (latched_) { permitted_ = false; return; }
    // Sensor/obstacle stops are owned here and may be configured non-latching.
    // Never let such an inhibit mask a new critical motor/control failure.
    if (motion_.fault() != AtlasFault::None && motion_.fault() != AtlasFault::Sensor &&
        motion_.fault() != AtlasFault::Obstacle) {
        trip(motion_.fault(), true);
        return;
    }
    if (!motion_.ready() || !sensors_.ready() || !distanceCalibrated()) {
        trip(AtlasFault::Configuration, true);
        return;
    }
    const bool moving = motion_.moving();
    if (moving && gap > MaximumControlGapMs) { trip(AtlasFault::ControlOverrun, true); return; }
    const AtlasFault sensorFault = inspectSensors(nowMs, false);
    if (sensorFault != AtlasFault::None) {
        if (moving) trip(sensorFault, sensorFault == AtlasFault::Sensor ? LatchSensorFaults : LatchObstacleStops);
        else {
            // Startup warm-up or an obstruction at rest inhibits starts without
            // creating an automatic critical latch before the first four pings.
            fault_ = sensorFault;
            permitted_ = false;
            motion_.setSafetyPermit(false);
        }
        wasMoving_ = false;
        return;
    }
    if (fault_ != AtlasFault::None) {
        motion_.clearFailure(); // only non-latched sensor/obstacle inhibits reach here.
        fault_ = AtlasFault::None;
    }
    permitted_ = true;
    motion_.setSafetyPermit(true);
    if (!moving) { wasMoving_ = false; return; }
    uint32_t counts[2];
    motion_.encoderCounts(counts[0], counts[1]);
    if (!wasMoving_) {
        for (uint8_t i = 0; i < 2; ++i) {
            previousCounts_[i] = counts[i];
            lastPulseMs_[i] = nowMs;
        }
        wasMoving_ = true;
    }
    const uint32_t movementAge = nowMs - motion_.startedAtMs();
    if (movementAge >= MovementTimeoutMs) { trip(AtlasFault::MovementTimeout, true); return; }
    const float duties[] = {motion_.leftDuty(), motion_.rightDuty()};
    for (uint8_t i = 0; i < 2; ++i) {
        if (counts[i] != previousCounts_[i] || duties[i] < StallMinimumDuty) lastPulseMs_[i] = nowMs;
        previousCounts_[i] = counts[i];
        if (movementAge >= StallStartupGraceMs && duties[i] >= StallMinimumDuty &&
            nowMs - lastPulseMs_[i] >= StallTimeoutMs) {
            trip(i == 0 ? AtlasFault::StallLeft : AtlasFault::StallRight, true);
            return;
        }
    }
}
