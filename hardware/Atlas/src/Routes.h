#pragma once

#include "CheckpointManager.h"

class Routes {
public:
    struct Segment {
        MotionController::Kind kind = MotionController::Kind::None;
        float distanceCm = 0; // positive magnitude for straights; zero for a 90-degree turn.
        CheckpointManager::Evidence endpoint;
    };
    enum class State : uint8_t { Empty, Recording, Ready, Running, Paused, Complete, Cancelled, Failed };
    enum class Error : uint8_t { None, Full, InvalidSegment, Empty, StartDenied, Motion,
                                 Checkpoint, SegmentTimeout, RouteTimeout };
    Routes(MotionController& motion, SensorManager& sensors, CheckpointManager& checkpoints)
        : motion_(motion), sensors_(sensors), checkpoints_(checkpoints) {}

    bool beginRecording(); // replaces the RAM route; no flash storage.
    bool record(const Segment& segment);
    bool finishRecording();
    bool start(uint32_t nowMs);
    void update(uint32_t nowMs);
    bool pause();
    bool resume(uint32_t nowMs);
    void cancel();
    static bool validSegment(const Segment& segment);
    State state() const { return state_; }
    Error error() const { return error_; }
    uint8_t count() const { return count_; }
    uint8_t index() const { return index_; } // zero based; count at completion.
    const Segment* segment(uint8_t index) const { return index < count_ ? &segments_[index] : nullptr; }

private:
    bool launch(const Segment& segment);
    void fail(Error error);
    MotionController& motion_;
    SensorManager& sensors_;
    CheckpointManager& checkpoints_;
    Segment segments_[AtlasConfig::MaximumRouteSegments];
    State state_ = State::Empty;
    Error error_ = Error::None;
    uint8_t count_ = 0;
    uint8_t index_ = 0;
    bool hasRoute_ = false;
    bool started_ = false;
    bool validating_ = false;
    bool endpointReached_ = false;
    bool needsResume_ = false;
    uint32_t routeAtMs_ = 0;
    uint32_t segmentAtMs_ = 0;
    uint32_t readyAtMs_ = 0;
};
