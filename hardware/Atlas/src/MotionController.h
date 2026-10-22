#pragma once

#include "AtlasConfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

class SafetyController;

class MotionController {
public:
    enum class State : uint8_t { Idle, Moving, Complete, Stopped, Failed };
    enum class Kind : uint8_t { None, Forward, Backward, Clockwise90, Counterclockwise90 };
    struct Progress {
        State state;
        Kind kind;
        float leftCm;
        float rightCm;
        float targetCm;
        float fraction;
    };

    bool begin();
    void update(uint32_t nowMs);
    bool moveDistance(float signedCentimetres, uint16_t duty = AtlasConfig::CruiseDuty);
    bool turn90(bool clockwise, uint16_t duty = AtlasConfig::TurnDuty);
    // Explicit continuation only; preserve BOTH original encoder baselines and
    // target (including coast while stopped). Safety must have granted permission.
    bool resume();
    void stop(); // immediate, no deceleration ramp on a stop request.
    void emergencyStop(AtlasFault reason = AtlasFault::EmergencyStop);
    Progress progress();
    void encoderCounts(uint32_t& left, uint32_t& right);
    bool ready() const { return ready_; }
    bool moving() const { return state_ == State::Moving; }
    State state() const { return state_; }
    Kind kind() const { return kind_; }
    AtlasFault fault() const { return fault_; }
    uint32_t startedAtMs() const { return startedAtMs_; }
    float leftDuty() const { return duties_[0]; }
    float rightDuty() const { return duties_[1]; }

private:
    friend class SafetyController; // navigation cannot enable or clear the safety interlock.
    struct Encoder {
        portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
        volatile uint32_t counts = 0;
        volatile uint32_t lastEdgeUs = 0;
        volatile bool seenEdge = false;
    } encoders_[2];
    static void ARDUINO_ISR_ATTR encoderInterrupt(void* context);
    void setSafetyPermit(bool permitted);
    void clearFailure();
    bool start(float targetCm, Kind kind, uint16_t duty, bool resuming = false);
    bool writePwm(uint8_t index, uint32_t duty);
    bool drive(uint8_t side, float duty, bool forward);
    void stopOutputs(AtlasConfig::StopMode mode);
    float distanceFor(uint8_t side, uint32_t counts) const;
    bool ready_ = false;
    bool sleepInitialized_ = false;
    bool safetyPermit_ = false;
    bool resumable_ = false;
    bool attached_[4] = {false, false, false, false};
    State state_ = State::Idle;
    Kind kind_ = Kind::None;
    AtlasFault fault_ = AtlasFault::None;
    uint32_t baseline_[2] = {0, 0};
    float duties_[2] = {0, 0};
    float targetCm_ = 0;
    uint16_t requestedDuty_ = 0;
    uint32_t startedAtMs_ = 0;
    uint32_t lastControlMs_ = 0;
    uint32_t stoppedAtMs_ = 0;
};
