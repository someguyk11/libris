#pragma once

#include "MotionController.h"
#include "SensorManager.h"

class SafetyController {
public:
    SafetyController(MotionController& motion, SensorManager& sensors)
        : motion_(motion), sensors_(sensors) {}
    void begin(uint32_t nowMs);
    void update(uint32_t nowMs);
    void emergencyStop();
    bool reset(uint32_t nowMs); // explicit only; never resumes the previous move.
    bool canMove() const { return permitted_; }
    AtlasFault fault() const { return fault_; }
    bool latched() const { return latched_; }

private:
    AtlasFault inspectSensors(uint32_t nowMs, bool requireAll);
    void trip(AtlasFault fault, bool latch);
    MotionController& motion_;
    SensorManager& sensors_;
    AtlasFault fault_ = AtlasFault::None;
    bool latched_ = false;
    bool permitted_ = false;
    bool blocked_[AtlasConfig::SensorCount] = {true, true, true, true};
    bool wasMoving_ = false;
    uint32_t lastUpdateMs_ = 0;
    uint32_t previousCounts_[2] = {0, 0};
    uint32_t lastPulseMs_[2] = {0, 0};
};
