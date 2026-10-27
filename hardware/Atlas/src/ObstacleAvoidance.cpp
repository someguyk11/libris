#include "ObstacleAvoidance.h"

using namespace AtlasConfig;

void ObstacleAvoidance::reset() { interruptions_ = 0; finish(); }

void ObstacleAvoidance::finish() { state_ = State::Idle; clearing_ = false; }

void ObstacleAvoidance::begin(uint32_t nowMs) {
    if (!ObstacleAvoidanceEnabled) return; // obstacle avoidance disabled for now
    if (interruptions_ >= MaximumObstacleInterruptions) { state_ = State::Aborted; return; }
    ++interruptions_;
    startedAtMs_ = nowMs;
    clearing_ = false;
    state_ = State::Waiting;
}

ObstacleAvoidance::State ObstacleAvoidance::update(const SensorManager& sensors, uint32_t nowMs) {
    if (!ObstacleAvoidanceEnabled) return State::Clear; // obstacle avoidance disabled for now
    if (state_ == State::Idle || state_ == State::Aborted) return state_;
    // Timeout includes operator acknowledgement of a latched stop.
    if (nowMs - startedAtMs_ >= ObstacleWaitTimeoutMs) { state_ = State::Aborted; return state_; }
    bool clear = true;
    for (uint8_t i = 0; i < SensorCount; ++i) {
        const auto r = sensors.reading(static_cast<SensorId>(i), nowMs);
        if (!r.valid || r.faulted || r.distanceCm < ObstacleStopCm[i] + ObstacleHysteresisCm)
            clear = false;
    }
    if (!clear) { clearing_ = false; state_ = State::Waiting; }
    else {
        if (!clearing_) { clearing_ = true; clearAtMs_ = nowMs; }
        state_ = nowMs - clearAtMs_ >= ObstacleClearanceMs ? State::Clear : State::Waiting;
    }
    return state_;
}
