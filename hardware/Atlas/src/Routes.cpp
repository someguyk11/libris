#include "Routes.h"

using namespace AtlasConfig;

bool Routes::validSegment(const Segment& s) {
    const bool straight = s.kind == MotionController::Kind::Forward || s.kind == MotionController::Kind::Backward;
    const bool turn = s.kind == MotionController::Kind::Clockwise90 || s.kind == MotionController::Kind::Counterclockwise90;
    if (EncoderOnlyGroundTest && !straight) return false;
    if (straight ? (!positive(s.distanceCm) || s.distanceCm <= DistanceToleranceCm || s.distanceCm > MaximumMoveCm)
                 : (!turn || s.distanceCm != 0)) return false;
    if (s.endpoint.validMask & ~((1U << SensorCount) - 1)) return false;
    for (uint8_t i = 0; i < SensorCount; ++i)
        if ((s.endpoint.validMask & (1U << i)) &&
            (!isfinite(s.endpoint.rangesCm[i]) || s.endpoint.rangesCm[i] < SensorMinCm ||
             s.endpoint.rangesCm[i] > SensorMaxCm)) return false;
    return true;
}

bool Routes::beginRecording() {
    if (motion_.moving() || state_ == State::Running || state_ == State::Paused || state_ == State::Recording) return false;
    count_ = index_ = 0;
    hasRoute_ = false;
    error_ = Error::None;
    state_ = State::Recording;
    checkpoints_.reset();
    return true;
}

bool Routes::record(const Segment& s) {
    if (state_ != State::Recording) return false;
    if (count_ >= MaximumRouteSegments) { error_ = Error::Full; return false; }
    if (!validSegment(s)) { error_ = Error::InvalidSegment; return false; }
    segments_[count_++] = s;
    return true;
}

bool Routes::finishRecording() {
    if (state_ != State::Recording || motion_.moving()) return false;
    if (count_ == 0) { error_ = Error::Empty; return false; }
    hasRoute_ = true;
    state_ = State::Ready;
    error_ = Error::None;
    return true;
}

bool Routes::start(uint32_t nowMs) {
    if (!hasRoute_ || count_ == 0 || motion_.moving() || state_ == State::Recording ||
        state_ == State::Running || state_ == State::Paused) return false;
    for (uint8_t i = 0; i < count_; ++i) {
        if (!validSegment(segments_[i])) return false;
        const auto kind = segments_[i].kind;
        if ((kind == MotionController::Kind::Clockwise90 || kind == MotionController::Kind::Counterclockwise90) &&
            !turnsCalibrated()) return false;
    }
    if (!distanceCalibrated()) return false;
    checkpoints_.reset();
    index_ = 0;
    started_ = validating_ = needsResume_ = endpointReached_ = false;
    routeAtMs_ = segmentAtMs_ = readyAtMs_ = nowMs;
    error_ = Error::None;
    state_ = State::Running;
    return true;
}

bool Routes::load(const Segment* segments, uint8_t count) {
    if (!segments || !count || count > MaximumRouteSegments || motion_.moving() ||
        state_ == State::Recording || state_ == State::Running || state_ == State::Paused) return false;
    for (uint8_t i = 0; i < count; ++i) if (!validSegment(segments[i])) return false;
    for (uint8_t i = 0; i < count; ++i) segments_[i] = segments[i];
    count_ = count; index_ = 0; hasRoute_ = true;
    started_ = validating_ = endpointReached_ = needsResume_ = false;
    state_ = State::Ready; error_ = Error::None;
    return true;
}

bool Routes::clear() {
    if (motion_.moving() || state_ == State::Running || state_ == State::Paused) return false;
    count_ = index_ = 0; hasRoute_ = false;
    started_ = validating_ = endpointReached_ = needsResume_ = false;
    state_ = State::Empty; error_ = Error::None;
    return true;
}

bool Routes::launch(const Segment& s) {
    switch (s.kind) {
        case MotionController::Kind::Forward: return motion_.moveDistance(s.distanceCm);
        case MotionController::Kind::Backward: return motion_.moveDistance(-s.distanceCm);
        case MotionController::Kind::Clockwise90: return motion_.turn90(true);
        case MotionController::Kind::Counterclockwise90: return motion_.turn90(false);
        default: return false;
    }
}

void Routes::fail(Error error) {
    if (motion_.moving()) motion_.stop();
    error_ = error;
    state_ = State::Failed;
}

void Routes::update(uint32_t nowMs) {
    if (state_ != State::Running && state_ != State::Paused) return;
    if (nowMs - routeAtMs_ >= RouteTimeoutMs) { fail(Error::RouteTimeout); return; }
    if (nowMs - segmentAtMs_ >= RouteSegmentTimeoutMs) { fail(Error::SegmentTimeout); return; }
    if (state_ == State::Paused) return;
    if (!started_ || needsResume_) {
        if (nowMs - readyAtMs_ >= RouteStartTimeoutMs) { fail(Error::StartDenied); return; }
        if (nowMs - readyAtMs_ < DirectionSettleMs) return;
        if (needsResume_) {
            const auto p = motion_.progress();
            if (p.leftCm > p.targetCm + CheckpointDistanceToleranceCm ||
                p.rightCm > p.targetCm + CheckpointDistanceToleranceCm) { fail(Error::Checkpoint); return; }
            if (!motion_.resume()) return;
        } else if (!launch(segments_[index_])) return;
        started_ = true;
        needsResume_ = false;
    }
    if (!endpointReached_) {
        if (motion_.state() == MotionController::State::Failed) { fail(Error::Motion); return; }
        if (motion_.state() != MotionController::State::Complete) return;
        endpointReached_ = true;
    }
    if (!validating_) {
        checkpoints_.begin(nowMs, &segments_[index_].endpoint);
        validating_ = true;
    }
    auto progress = motion_.progress();
    progress.state = MotionController::State::Complete; // endpoint survives an explicit safety reset at rest.
    const auto result = checkpoints_.update(progress, sensors_, nowMs);
    if (result == CheckpointManager::Result::Failed) { fail(Error::Checkpoint); return; }
    if (result != CheckpointManager::Result::Valid) return;
    ++index_;
    started_ = validating_ = endpointReached_ = false;
    if (index_ == count_) state_ = State::Complete;
    else segmentAtMs_ = readyAtMs_ = nowMs;
}

bool Routes::pause() {
    if (state_ != State::Running) return false;
    endpointReached_ = endpointReached_ || (started_ && motion_.state() == MotionController::State::Complete);
    needsResume_ = started_ && !endpointReached_;
    if (motion_.moving()) motion_.stop();
    validating_ = false; // require a new settled endpoint scan on resume.
    state_ = State::Paused;
    return true;
}

bool Routes::resume(uint32_t nowMs) {
    if (state_ != State::Paused || motion_.moving()) return false;
    readyAtMs_ = nowMs;
    state_ = State::Running;
    return true;
}

void Routes::cancel() {
    if (motion_.moving()) motion_.stop();
    if (state_ == State::Recording) { count_ = 0; hasRoute_ = false; }
    started_ = validating_ = needsResume_ = endpointReached_ = false;
    // Cleanup must retain a failed run's diagnostic until a new run/recording.
    if (state_ != State::Failed) state_ = State::Cancelled;
}
