#pragma once

#include "SensorManager.h"

// Stop/wait/clear/abort only. No obstacle bypass or motor permission here.
class ObstacleAvoidance {
public:
    enum class State : uint8_t { Idle, Waiting, Clear, Aborted };
    void reset();
    void begin(uint32_t nowMs);
    State update(const SensorManager& sensors, uint32_t nowMs);
    void finish(); // retain interruption budget for the current operation.
    State state() const { return state_; }
    uint8_t interruptions() const { return interruptions_; }

private:
    State state_ = State::Idle;
    uint8_t interruptions_ = 0;
    bool clearing_ = false;
    uint32_t startedAtMs_ = 0;
    uint32_t clearAtMs_ = 0;
};
