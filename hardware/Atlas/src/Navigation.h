#pragma once

#include "Routes.h"
#include "ObstacleAvoidance.h"
#include "SafetyController.h"

// Single-loop, immediate-return command/status API for future Layer 3 callers.
// No networking, allocations, persistence, task creation or safety overrides.
class Navigation {
public:
    enum class State : uint8_t { Idle, Manual, Recording, Replaying, Paused, Completed, Fault };
    enum class PauseReason : uint8_t { None, User, Obstacle };
    enum class Error : uint8_t { None, Configuration, Safety, Motion, Checkpoint,
                                 Route, RecordingFull, ObstacleLimit, Timeout, ResumeDenied };
    Navigation(MotionController& motion, SensorManager& sensors, SafetyController& safety,
               Routes& routes, CheckpointManager& checkpoints, ObstacleAvoidance& obstacles)
        : motion_(motion), sensors_(sensors), safety_(safety), routes_(routes),
          checkpoints_(checkpoints), obstacles_(obstacles) {}

    void begin(uint32_t nowMs);
    void update(uint32_t nowMs);
    bool driveDistance(float signedCentimetres);
    bool turn90(bool clockwise);
    bool beginRecording();
    bool finishRecording();
    bool replay();
    bool pause();
    bool resume(); // request, not a synchronous wait or motor-permit override.
    void cancel();
    bool resetSafety(uint32_t nowMs); // explicit caller request; never resumes.
    State state() const { return state_; }
    PauseReason pauseReason() const { return pauseReason_; }
    Error error() const { return error_; }
    bool movementPending() const {
        return manualStarted_ || state_ == State::Replaying ||
            (state_ == State::Paused && pausedState_ == State::Replaying);
    }
    static const char* stateName(State state);

private:
    bool startManual(const Routes::Segment& segment);
    bool enterPause(PauseReason reason, uint32_t nowMs);
    void continueActivity(uint32_t nowMs);
    void updateManual(uint32_t nowMs);
    void fail(Error error);
    MotionController& motion_;
    SensorManager& sensors_;
    SafetyController& safety_;
    Routes& routes_;
    CheckpointManager& checkpoints_;
    ObstacleAvoidance& obstacles_;
    State state_ = State::Idle;
    State pausedState_ = State::Idle;
    PauseReason pauseReason_ = PauseReason::None;
    Error error_ = Error::None;
    Routes::Segment manualSegment_;
    bool manualStarted_ = false;
    bool manualValidating_ = false;
    bool manualEndpoint_ = false;
    bool manualNeedsResume_ = false;
    bool resumeRequested_ = false;
    bool acknowledgementRequired_ = false;
    uint32_t sessionAtMs_ = 0;
    uint32_t manualAtMs_ = 0;
    uint32_t pausedAtMs_ = 0;
};
