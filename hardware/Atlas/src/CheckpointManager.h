#pragma once

#include "MotionController.h"
#include "SensorManager.h"

// A coarse endpoint consistency check, NOT position/heading localization.
class CheckpointManager {
public:
    struct Evidence {
        float rangesCm[AtlasConfig::SensorCount] = {};
        uint8_t validMask = 0;
    };
    enum class Result : uint8_t { Waiting, Valid, Failed };

    void reset();
    void begin(uint32_t nowMs, const Evidence* expected = nullptr);
    Result update(const MotionController::Progress& progress,
                  const SensorManager& sensors, uint32_t nowMs);
    const Evidence& evidence() const { return observed_; }
    uint8_t retries() const { return retries_; }
    uint8_t validatedCount() const { return validatedCount_; }

private:
    Evidence expected_;
    Evidence observed_;
    Result result_ = Result::Waiting;
    bool active_ = false;
    uint8_t retries_ = 0;
    uint8_t validatedCount_ = 0;
    uint32_t attemptAtMs_ = 0;
};
