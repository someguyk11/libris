#include "CheckpointManager.h"

using namespace AtlasConfig;

void CheckpointManager::reset() {
    active_ = false;
    result_ = Result::Waiting;
    retries_ = validatedCount_ = 0;
    expected_ = Evidence{};
    observed_ = Evidence{};
}

void CheckpointManager::begin(uint32_t nowMs, const Evidence* expected) {
    expected_ = expected ? *expected : Evidence{};
    observed_ = Evidence{};
    attemptAtMs_ = nowMs;
    retries_ = 0;
    result_ = Result::Waiting;
    active_ = true;
}

CheckpointManager::Result CheckpointManager::update(
    const MotionController::Progress& p, const SensorManager& sensors, uint32_t nowMs) {
    if (!active_ || result_ != Result::Waiting) return result_;
    // Reject an overrun or unequal wheel travel; re-observing cannot fix odometry.
    if (!positive(p.targetCm) || !isfinite(p.leftCm) || !isfinite(p.rightCm) ||
        fabsf(p.leftCm - p.targetCm) > CheckpointDistanceToleranceCm ||
        fabsf(p.rightCm - p.targetCm) > CheckpointDistanceToleranceCm ||
        fabsf(p.leftCm - p.rightCm) > CheckpointWheelMismatchCm) {
        result_ = Result::Failed;
        return result_;
    }
    bool consistent = p.state == MotionController::State::Complete;
    observed_ = Evidence{};
    for (uint8_t i = 0; i < SensorCount; ++i) {
        const auto r = sensors.reading(static_cast<SensorId>(i), nowMs);
        // Require a full fresh scan after this attempt's endpoint settling time.
        // Signed subtraction is safe across millis rollover for these short ages.
        const bool fresh = r.valid && !r.faulted &&
            static_cast<int32_t>(r.timestampMs - attemptAtMs_) >= static_cast<int32_t>(CheckpointSettleMs);
        const bool clear = fresh && r.distanceCm >= ObstacleStopCm[i] + ObstacleHysteresisCm;
        if (clear) {
            observed_.rangesCm[i] = r.distanceCm;
            observed_.validMask |= static_cast<uint8_t>(1U << i);
        } else consistent = false;
        if ((expected_.validMask & (1U << i)) &&
            (!clear || fabsf(r.distanceCm - expected_.rangesCm[i]) > CheckpointSensorToleranceCm))
            consistent = false;
    }
    if (consistent) {
        result_ = Result::Valid;
        if (validatedCount_ < UINT8_MAX) ++validatedCount_;
    } else if (nowMs - attemptAtMs_ >= CheckpointAttemptTimeoutMs) {
        if (retries_ >= CheckpointMaxRetries) result_ = Result::Failed;
        else { ++retries_; attemptAtMs_ = nowMs; }
    }
    return result_;
}
