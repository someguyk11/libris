#include "Navigation.h"

using namespace AtlasConfig;

const char* Navigation::stateName(State state) {
    switch (state) {
        case State::Idle: return "IDLE";
        case State::Manual: return "MANUAL";
        case State::Recording: return "RECORDING";
        case State::Replaying: return "REPLAYING";
        case State::Paused: return "PAUSED";
        case State::Completed: return "COMPLETED";
        case State::Fault: return "FAULT";
    }
    return "UNKNOWN";
}

void Navigation::begin(uint32_t nowMs) {
    sessionAtMs_ = nowMs;
    checkpoints_.reset();
    obstacles_.reset();
    if (navigationTuningError()) fail(Error::Configuration);
    else if (safety_.latched()) fail(Error::Safety);
}

bool Navigation::startManual(const Routes::Segment& segment) {
    if ((state_ != State::Idle && state_ != State::Completed && state_ != State::Recording) ||
        manualStarted_ || !safety_.canMove() || !Routes::validSegment(segment)) return false;
    if (state_ == State::Recording && routes_.count() >= MaximumRouteSegments) return false;
    bool accepted = false;
    switch (segment.kind) {
        case MotionController::Kind::Forward: accepted = motion_.moveDistance(segment.distanceCm); break;
        case MotionController::Kind::Backward: accepted = motion_.moveDistance(-segment.distanceCm); break;
        case MotionController::Kind::Clockwise90: accepted = motion_.turn90(true); break;
        case MotionController::Kind::Counterclockwise90: accepted = motion_.turn90(false); break;
        default: return false;
    }
    if (!accepted) return false;
    if (state_ != State::Recording) {
        state_ = State::Manual;
        obstacles_.reset();
        checkpoints_.reset();
        sessionAtMs_ = millis();
    }
    manualSegment_ = segment;
    manualAtMs_ = millis();
    manualStarted_ = true;
    manualValidating_ = manualEndpoint_ = manualNeedsResume_ = false;
    error_ = Error::None;
    return true;
}

bool Navigation::driveDistance(float cm) {
    Routes::Segment segment;
    segment.kind = cm > 0 ? MotionController::Kind::Forward : MotionController::Kind::Backward;
    segment.distanceCm = fabsf(cm);
    return startManual(segment);
}

bool Navigation::turn90(bool clockwise) {
    Routes::Segment segment;
    segment.kind = clockwise ? MotionController::Kind::Clockwise90 : MotionController::Kind::Counterclockwise90;
    return startManual(segment);
}

bool Navigation::beginRecording() {
    if ((state_ != State::Idle && state_ != State::Completed) || !safety_.canMove() ||
        !routes_.beginRecording()) return false;
    obstacles_.reset();
    sessionAtMs_ = millis();
    error_ = Error::None;
    state_ = State::Recording;
    return true;
}

bool Navigation::finishRecording() {
    if (state_ != State::Recording || manualStarted_ || !routes_.finishRecording()) return false;
    state_ = State::Idle;
    return true;
}

bool Navigation::replay() {
    if ((state_ != State::Idle && state_ != State::Completed) || !safety_.canMove() ||
        !routes_.start(millis())) return false;
    obstacles_.reset();
    error_ = Error::None;
    state_ = State::Replaying;
    return true;
}

bool Navigation::enterPause(PauseReason reason, uint32_t nowMs) {
    if (state_ != State::Manual && state_ != State::Recording && state_ != State::Replaying) return false;
    pausedState_ = state_;
    if (state_ == State::Replaying) routes_.pause();
    else if (manualStarted_) {
        manualEndpoint_ = manualEndpoint_ || motion_.state() == MotionController::State::Complete;
        manualNeedsResume_ = !manualEndpoint_;
        if (motion_.moving()) motion_.stop();
        manualValidating_ = false;
    }
    pauseReason_ = reason;
    pausedAtMs_ = nowMs;
    resumeRequested_ = false;
    acknowledgementRequired_ = reason == PauseReason::Obstacle &&
        (safety_.latched() || !AutoResumeAfterObstacle);
    state_ = State::Paused;
    if (reason == PauseReason::Obstacle) obstacles_.begin(nowMs);
    return true;
}

bool Navigation::pause() {
    // A user pause during obstacle waiting suppresses automatic continuation.
    if (state_ == State::Paused && pauseReason_ == PauseReason::Obstacle) {
        // Keep clearance observation and its timeout; acknowledgement cannot
        // bypass the sustained-clear requirement or restart the wait budget.
        acknowledgementRequired_ = true;
        resumeRequested_ = false;
        return true;
    }
    return enterPause(PauseReason::User, millis());
}

bool Navigation::resume() {
    if (state_ != State::Paused || !safety_.canMove() || safety_.latched()) return false;
    if (pauseReason_ == PauseReason::Obstacle && obstacles_.state() != ObstacleAvoidance::State::Clear) return false;
    resumeRequested_ = true;
    return true;
}

void Navigation::continueActivity(uint32_t nowMs) {
    if (!safety_.canMove() || nowMs - pausedAtMs_ < DirectionSettleMs) return;
    if (pausedState_ == State::Replaying) {
        if (!routes_.resume(nowMs)) { fail(Error::ResumeDenied); return; }
    } else if (manualStarted_ && manualNeedsResume_) {
        const auto p = motion_.progress();
        if (p.leftCm > p.targetCm + CheckpointDistanceToleranceCm ||
            p.rightCm > p.targetCm + CheckpointDistanceToleranceCm) { fail(Error::Checkpoint); return; }
        if (!motion_.resume()) { fail(Error::ResumeDenied); return; }
        manualNeedsResume_ = false;
    }
    obstacles_.finish();
    state_ = pausedState_;
    pauseReason_ = PauseReason::None;
    resumeRequested_ = false;
}

void Navigation::cancel() {
    if (motion_.moving()) motion_.stop();
    routes_.cancel();
    manualStarted_ = manualValidating_ = manualEndpoint_ = manualNeedsResume_ = false;
    obstacles_.reset();
    pauseReason_ = PauseReason::None;
    resumeRequested_ = false;
    if (state_ != State::Fault) { state_ = State::Idle; error_ = Error::None; }
}

void Navigation::fail(Error error) {
    cancel();
    error_ = error;
    state_ = State::Fault;
}

bool Navigation::resetSafety(uint32_t nowMs) {
    if (navigationTuningError() || !safety_.reset(nowMs)) return false;
    if (state_ == State::Fault) {
        cancel();
        state_ = State::Idle;
        error_ = Error::None;
    }
    // A paused activity and its acknowledgement requirement survive reset.
    return true;
}

void Navigation::updateManual(uint32_t nowMs) {
    if (!manualStarted_) return;
    if (!manualEndpoint_) {
        if (motion_.state() == MotionController::State::Failed) { fail(Error::Motion); return; }
        if (motion_.state() != MotionController::State::Complete) return;
        manualEndpoint_ = true;
    }
    if (!manualValidating_) { checkpoints_.begin(nowMs); manualValidating_ = true; }
    auto progress = motion_.progress();
    progress.state = MotionController::State::Complete;
    const auto result = checkpoints_.update(progress, sensors_, nowMs);
    if (result == CheckpointManager::Result::Failed) { fail(Error::Checkpoint); return; }
    if (result != CheckpointManager::Result::Valid) return;
    if (state_ == State::Recording) {
        manualSegment_.endpoint = checkpoints_.evidence();
        if (manualSegment_.kind == MotionController::Kind::Forward ||
            manualSegment_.kind == MotionController::Kind::Backward)
            manualSegment_.distanceCm = (progress.leftCm + progress.rightCm) * 0.5f;
        if (!routes_.record(manualSegment_)) { fail(Error::RecordingFull); return; }
    } else state_ = State::Completed;
    manualStarted_ = manualValidating_ = manualEndpoint_ = false;
}

void Navigation::update(uint32_t nowMs) {
    if (state_ == State::Fault) return;
    const auto fault = safety_.fault();
    const bool activity = state_ == State::Manual || state_ == State::Recording ||
        state_ == State::Replaying || state_ == State::Paused;
    if ((safety_.latched() && fault != AtlasFault::Obstacle) ||
        (fault != AtlasFault::None && fault != AtlasFault::Obstacle && fault != AtlasFault::Sensor) ||
        (activity && fault == AtlasFault::Sensor)) { fail(Error::Safety); return; }
    if (manualStarted_ && nowMs - manualAtMs_ >= RouteSegmentTimeoutMs) { fail(Error::Timeout); return; }
    const bool recording = state_ == State::Recording || (state_ == State::Paused && pausedState_ == State::Recording);
    if (recording && nowMs - sessionAtMs_ >= RouteTimeoutMs) { fail(Error::Timeout); return; }
    if (fault == AtlasFault::Obstacle && (manualStarted_ || state_ == State::Replaying) && state_ != State::Paused)
        enterPause(PauseReason::Obstacle, nowMs);
    if (state_ == State::Paused) {
        // An obstacle appearing during a user pause still needs a complete
        // clearance window. Preserve the user's explicit-resume requirement.
        if (pauseReason_ == PauseReason::User && fault == AtlasFault::Obstacle) {
            pauseReason_ = PauseReason::Obstacle;
            acknowledgementRequired_ = true;
            resumeRequested_ = false;
            obstacles_.begin(nowMs);
        }
        if (pausedState_ == State::Replaying) {
            routes_.update(nowMs); // enforce bounded route/segment ages even while paused.
            if (routes_.state() == Routes::State::Failed) { fail(Error::Route); return; }
        }
        if (pauseReason_ == PauseReason::Obstacle) {
            const auto obstacle = obstacles_.update(sensors_, nowMs);
            if (obstacle == ObstacleAvoidance::State::Aborted) { fail(Error::ObstacleLimit); return; }
            if (obstacle != ObstacleAvoidance::State::Clear) return;
            if (acknowledgementRequired_ && !resumeRequested_) return;
        } else if (!resumeRequested_) return;
        continueActivity(nowMs);
        return;
    }
    if (state_ == State::Replaying) {
        routes_.update(nowMs);
        if (routes_.state() == Routes::State::Failed) fail(Error::Route);
        else if (routes_.state() == Routes::State::Complete) state_ = State::Completed;
    } else if (state_ == State::Manual || state_ == State::Recording) updateManual(nowMs);
}
