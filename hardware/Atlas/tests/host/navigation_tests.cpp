// Real Navigation/Routes/CheckpointManager/ObstacleAvoidance, fake lower layers.
// The real lower-layer HEADERS are used, not replacement interfaces.
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include "Navigation.h"

using namespace AtlasConfig;
namespace sim {
uint32_t now = 1000;
float left = 0, right = 0;
float leftTargetCm = 0, rightTargetCm = 0;
bool complete = false, failed = false, denied = false;
bool sensorsValid = true, stale = false;
float range = 100;
AtlasFault fault = AtlasFault::None;
bool latched = false;
unsigned launches = 0, resumes = 0;
}
uint32_t millis() { return sim::now; }

bool MotionController::begin() { ready_ = true; return true; }
bool MotionController::start(float target, Kind kind, uint16_t duty, bool resuming) {
    if (!ready_ || !safetyPermit_ || moving() || fault_ != AtlasFault::None || sim::denied) return false;
    if (!resuming) { sim::left = sim::right = 0; ++sim::launches; }
    targetCm_ = target; kind_ = kind; requestedDuty_ = duty;
    state_ = State::Moving; resumable_ = true; return true;
}
bool MotionController::moveDistance(float cm, uint16_t duty) {
    return start(fabsf(cm), cm > 0 ? Kind::Forward : Kind::Backward, duty);
}
bool MotionController::turn90(bool clockwise, uint16_t duty) {
    return start(10, clockwise ? Kind::Clockwise90 : Kind::Counterclockwise90, duty);
}
bool MotionController::resume() {
    if (!resumable_ || moving() || state_ == State::Complete) return false;
    if (!start(targetCm_, kind_, requestedDuty_, true)) return false;
    ++sim::resumes; return true;
}
bool MotionController::replayTicks(uint32_t leftTicks, uint32_t rightTicks, Kind kind) {
    if (!ready_ || !safetyPermit_ || moving() || fault_ != AtlasFault::None || sim::denied) return false;
    sim::left = sim::right = 0; ++sim::launches;
    // Store independent targets for progress tracking (Progress struct updated below)
    sim::leftTargetCm = leftTicks * 0.1f;  // approximate cm from ticks for test simulation
    sim::rightTargetCm = rightTicks * 0.1f;
    targetCm_ = fmaxf(sim::leftTargetCm, sim::rightTargetCm);
    kind_ = kind;
    requestedDuty_ = TurnDuty;
    state_ = State::Moving; resumable_ = true; return true;
}
void MotionController::stop() { if (state_ != State::Failed) state_ = State::Stopped; }
void MotionController::emergencyStop(AtlasFault reason) {
    safetyPermit_ = false; fault_ = reason; state_ = State::Failed;
}
void MotionController::clearFailure() { fault_ = AtlasFault::None; state_ = State::Idle; }
void MotionController::setSafetyPermit(bool permitted) { safetyPermit_ = permitted; }
void MotionController::update(uint32_t) {
    if (sim::failed) { emergencyStop(AtlasFault::EmergencyStop); sim::failed = false; }
    if (sim::complete) { state_ = State::Complete; resumable_ = false; sim::complete = false; }
}
MotionController::Progress MotionController::progress() {
    return {state_, kind_, sim::left, sim::right, targetCm_, targetCm_ ? fminf(sim::left, sim::right) / targetCm_ : 0, sim::leftTargetCm, sim::rightTargetCm};
}
bool SensorManager::begin() { ready_ = true; return true; }
SensorManager::Reading SensorManager::reading(SensorId, uint32_t now) const {
    Reading r;
    r.distanceCm = sim::range; r.timestampMs = sim::stale ? now - MaximumReadingAgeMs - 1 : now;
    r.valid = sim::sensorsValid && !sim::stale;
    r.faulted = !r.valid; r.error = r.valid ? Error::None : Error::Stale;
    return r;
}
// Model the relevant real safety contract: a reset erases motion's terminal
// state, retains encoder baselines/target, and DOES NOT grant motion permission.
void SafetyController::begin(uint32_t now) { update(now); }
void SafetyController::update(uint32_t) {
    fault_ = sim::fault; latched_ = sim::latched;
    permitted_ = fault_ == AtlasFault::None && !latched_;
    if (!permitted_ && motion_.moving()) motion_.emergencyStop(fault_);
    motion_.setSafetyPermit(permitted_);
}
bool SafetyController::reset(uint32_t) {
    permitted_ = false;
    if (motion_.moving() || !sim::sensorsValid || sim::stale || sim::range < 30) return false;
    motion_.clearFailure(); motion_.setSafetyPermit(false);
    fault_ = sim::fault = AtlasFault::None; latched_ = sim::latched = false;
    return true;
}

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(std::string(__func__) + ": " #condition); } while (0)
struct Rig {
    MotionController motion;
    SensorManager sensors;
    SafetyController safety{motion, sensors};
    CheckpointManager checkpoints;
    Routes routes{motion, sensors, checkpoints};
    ObstacleAvoidance obstacles;
    Navigation nav{motion, sensors, safety, routes, checkpoints, obstacles};
    Rig() {
        sim::now = 1000; sim::left = sim::right = 0;
        sim::complete = sim::failed = sim::denied = sim::stale = sim::latched = false;
        sim::sensorsValid = true; sim::range = 100; sim::fault = AtlasFault::None;
        sim::launches = sim::resumes = 0;
        motion.begin(); sensors.begin(); safety.begin(sim::now); nav.begin(sim::now);
    }
    void tick(uint32_t dt = 0) { sim::now += dt; safety.update(sim::now); nav.update(sim::now); }
    void endpoint(float cm = 10) { sim::left = sim::right = cm; sim::complete = true; motion.update(sim::now); }
    void seed(unsigned count = 1) {
        CHECK(routes.beginRecording());
        Routes::Segment s; s.kind = MotionController::Kind::Forward; s.distanceCm = 10;
        for (unsigned i = 0; i < count; ++i) CHECK(routes.record(s));
        CHECK(routes.finishRecording());
    }
    void replay() { seed(); CHECK(nav.replay()); tick(DirectionSettleMs); CHECK(motion.moving()); }
};

void recording_and_replay() {
    Rig r;
    CHECK(r.nav.beginRecording()); CHECK(!r.nav.finishRecording()); CHECK(r.nav.driveDistance(10));
    CHECK(!r.nav.finishRecording()); r.endpoint(10.25f); r.tick();
    CHECK(r.routes.count() == 0); r.tick(CheckpointSettleMs);
    CHECK(r.routes.count() == 1); CHECK(r.routes.segment(0)->distanceCm == 10.25f);
    CHECK(r.routes.segment(0)->endpoint.validMask == 15); CHECK(r.nav.finishRecording());
    CHECK(r.nav.replay()); r.tick(DirectionSettleMs); r.endpoint(10.25f); r.tick(); r.tick(CheckpointSettleMs);
    CHECK(r.nav.state() == Navigation::State::Completed); CHECK(r.routes.state() == Routes::State::Complete);
}
void reset_at_manual_endpoint() {
    Rig r; CHECK(r.nav.driveDistance(10)); r.endpoint();
    CHECK(r.nav.resetSafety(sim::now));
    CHECK(r.nav.state() == Navigation::State::Paused);
    r.tick(DirectionSettleMs); CHECK(!r.motion.moving()); CHECK(r.nav.resume()); r.tick(); r.tick(); r.tick(CheckpointSettleMs);
    CHECK(r.nav.state() == Navigation::State::Completed); CHECK(sim::resumes == 0);
}
void reset_at_replay_endpoint() {
    Rig r; r.replay(); r.endpoint(); CHECK(r.nav.resetSafety(sim::now));
    CHECK(r.nav.state() == Navigation::State::Paused); r.tick(DirectionSettleMs);
    CHECK(r.nav.resume()); r.tick(); r.tick(); r.tick(CheckpointSettleMs);
    CHECK(r.nav.state() == Navigation::State::Completed); CHECK(sim::resumes == 0);
}
void reset_observes_unprocessed_fault() {
    Rig r; CHECK(r.nav.beginRecording()); CHECK(r.nav.driveDistance(10));
    sim::fault = AtlasFault::EmergencyStop; sim::latched = true; r.safety.update(sim::now);
    CHECK(r.nav.state() == Navigation::State::Recording); CHECK(r.nav.resetSafety(sim::now)); r.tick();
    CHECK(r.nav.state() == Navigation::State::Idle); CHECK(!r.nav.movementPending());
    CHECK(r.routes.count() == 0); CHECK(!r.nav.resume()); CHECK(sim::resumes == 0);
}
void reset_clears_queued_resume() {
    Rig r; CHECK(r.nav.driveDistance(10)); CHECK(r.nav.pause()); CHECK(r.nav.resume());
    CHECK(r.nav.resetSafety(sim::now)); r.tick(DirectionSettleMs);
    CHECK(r.nav.state() == Navigation::State::Paused); CHECK(!r.motion.moving()); CHECK(sim::resumes == 0);
    CHECK(r.nav.resume()); r.tick(); CHECK(r.motion.moving()); CHECK(sim::resumes == 1);
}
void route_failure_diagnostic() {
    Rig r; r.replay(); r.endpoint(20); r.tick();
    CHECK(r.nav.state() == Navigation::State::Fault);
    CHECK(r.routes.state() == Routes::State::Failed); CHECK(r.routes.error() == Routes::Error::Checkpoint);
    r.nav.cancel(); CHECK(r.routes.state() == Routes::State::Failed);
    CHECK(r.nav.resetSafety(sim::now)); r.tick(); CHECK(r.nav.replay());
}
void user_pause_preserves_progress() {
    Rig r; r.replay(); sim::left = sim::right = 4; CHECK(r.nav.pause());
    sim::left = sim::right = 5; r.tick(DirectionSettleMs); CHECK(!r.motion.moving());
    CHECK(r.nav.resume()); r.tick(); r.tick(DirectionSettleMs);
    CHECK(r.motion.moving()); CHECK(sim::left == 5); CHECK(sim::resumes == 1); CHECK(sim::launches == 1);
    r.endpoint(); r.tick(); r.tick(CheckpointSettleMs); CHECK(r.nav.state() == Navigation::State::Completed);
}
void cancel_draft_and_committed() {
    Rig r; CHECK(r.nav.beginRecording()); CHECK(r.nav.driveDistance(10)); r.nav.cancel();
    CHECK(r.nav.state() == Navigation::State::Idle); CHECK(r.routes.count() == 0); CHECK(!r.nav.replay());
    r.seed(); CHECK(r.nav.replay()); r.tick(DirectionSettleMs); r.nav.cancel();
    CHECK(!r.motion.moving()); CHECK(r.routes.count() == 1); CHECK(r.nav.replay());
    r.tick(DirectionSettleMs); CHECK(r.routes.index() == 0);
}
void paused_deadline() {
    Rig r; r.replay(); CHECK(r.nav.pause()); r.tick(RouteSegmentTimeoutMs);
    CHECK(r.nav.state() == Navigation::State::Fault); CHECK(r.routes.error() == Routes::Error::SegmentTimeout);
}
void coast_overshoot_is_rejected() {
    Rig r; r.replay(); CHECK(r.nav.pause()); sim::left = 13; sim::right = 10;
    r.tick(DirectionSettleMs); CHECK(r.nav.resume()); r.tick(); r.tick(DirectionSettleMs);
    CHECK(r.nav.state() == Navigation::State::Fault); CHECK(sim::resumes == 0);
}
void checkpoint_stale_retry_and_mismatch() {
    Rig r;
    MotionController::Progress p{MotionController::State::Complete, MotionController::Kind::Forward, 10, 10, 10, 1};
    r.checkpoints.begin(sim::now); sim::stale = true;
    for (unsigned i = 0; i <= CheckpointMaxRetries; ++i) {
        sim::now += CheckpointAttemptTimeoutMs;
        auto result = r.checkpoints.update(p, r.sensors, sim::now);
        CHECK(result == (i == CheckpointMaxRetries ? CheckpointManager::Result::Failed : CheckpointManager::Result::Waiting));
    }
    sim::stale = false; r.checkpoints.begin(sim::now); p.leftCm = 13;
    CHECK(r.checkpoints.update(p, r.sensors, sim::now) == CheckpointManager::Result::Failed);
}
void checkpoint_evidence_and_settle() {
    Rig r; CheckpointManager::Evidence expected; expected.validMask = 15;
    for (auto& range : expected.rangesCm) range = 200;
    MotionController::Progress p{MotionController::State::Complete, MotionController::Kind::Forward, 10, 10, 10, 1};
    r.checkpoints.begin(sim::now, &expected);
    CHECK(r.checkpoints.update(p, r.sensors, sim::now) == CheckpointManager::Result::Waiting);
    sim::now += CheckpointSettleMs;
    CHECK(r.checkpoints.update(p, r.sensors, sim::now) == CheckpointManager::Result::Waiting);
    sim::range = 200;
    CHECK(r.checkpoints.update(p, r.sensors, sim::now) == CheckpointManager::Result::Valid);
}
void obstacle_clearance_and_limits() {
    Rig r; r.obstacles.begin(sim::now); r.obstacles.update(r.sensors, sim::now);
    sim::now += ObstacleClearanceMs - 1; CHECK(r.obstacles.update(r.sensors, sim::now) != ObstacleAvoidance::State::Clear);
    sim::sensorsValid = false; r.obstacles.update(r.sensors, sim::now); sim::sensorsValid = true;
    r.obstacles.update(r.sensors, sim::now); sim::now += ObstacleClearanceMs;
    CHECK(r.obstacles.update(r.sensors, sim::now) == ObstacleAvoidance::State::Clear);
    r.obstacles.finish();
    for (unsigned i = 1; i < MaximumObstacleInterruptions; ++i) { r.obstacles.begin(sim::now); CHECK(r.obstacles.state() == ObstacleAvoidance::State::Waiting); r.obstacles.finish(); }
    r.obstacles.begin(sim::now); CHECK(r.obstacles.state() == ObstacleAvoidance::State::Aborted);
    r.obstacles.reset(); r.obstacles.begin(sim::now); sim::now += ObstacleWaitTimeoutMs;
    CHECK(r.obstacles.update(r.sensors, sim::now) == ObstacleAvoidance::State::Aborted);
}
void latched_obstacle_requires_reset_and_resume() {
    Rig r; r.replay(); sim::fault = AtlasFault::Obstacle; sim::latched = true; sim::range = 10; r.tick();
    CHECK(r.nav.state() == Navigation::State::Paused); CHECK(!r.nav.resume()); CHECK(!r.nav.resetSafety(sim::now));
    sim::range = 100; CHECK(r.nav.resetSafety(sim::now)); r.tick(); r.tick(ObstacleClearanceMs);
    CHECK(r.nav.state() == Navigation::State::Paused); CHECK(!r.motion.moving()); CHECK(r.nav.resume());
    r.tick(); r.tick(DirectionSettleMs); CHECK(r.motion.moving());
}
void rollover_checkpoint_and_pause() {
    Rig r; sim::now = UINT32_MAX - 100; CHECK(r.nav.driveDistance(10)); CHECK(r.nav.pause());
    r.tick(200); CHECK(r.nav.resume()); r.tick(); CHECK(r.motion.moving());
    r.endpoint(); r.tick(); r.tick(CheckpointSettleMs); CHECK(r.nav.state() == Navigation::State::Completed);
}
void invalid_segments_and_capacity() {
    Rig r; CHECK(r.routes.beginRecording()); Routes::Segment s;
    CHECK(!r.routes.record(s)); s.kind = MotionController::Kind::Forward; s.distanceCm = NAN;
    CHECK(!r.routes.record(s)); s.distanceCm = 10; s.endpoint.validMask = 128; CHECK(!r.routes.record(s));
    s.endpoint.validMask = 0;
    for (unsigned i = 0; i < MaximumRouteSegments; ++i) CHECK(r.routes.record(s));
    CHECK(!r.routes.record(s)); CHECK(r.routes.error() == Routes::Error::Full);
}

void all_segment_kinds_record_and_replay() {
    Rig r; CHECK(r.nav.beginRecording());
    for (unsigned i = 0; i < 4; ++i) {
        CHECK(i < 2 ? r.nav.driveDistance(i == 0 ? 10 : -10) : r.nav.turn90(i == 2));
        r.endpoint(); r.tick(); r.tick(CheckpointSettleMs);
        CHECK(r.routes.count() == i + 1); CHECK(r.nav.state() == Navigation::State::Recording);
    }
    CHECK(r.nav.finishRecording()); CHECK(r.nav.replay());
    for (unsigned i = 0; i < 4; ++i) {
        r.tick(DirectionSettleMs); CHECK(r.motion.kind() == r.routes.segment(i)->kind);
        r.endpoint(); r.tick(); r.tick(CheckpointSettleMs); CHECK(r.routes.index() == i + 1);
    }
    CHECK(r.nav.state() == Navigation::State::Completed);
}
void reset_at_recording_endpoint() {
    Rig r; CHECK(r.nav.beginRecording()); CHECK(r.nav.driveDistance(10)); r.endpoint();
    CHECK(r.nav.resetSafety(sim::now)); CHECK(r.nav.state() == Navigation::State::Paused);
    r.tick(DirectionSettleMs); CHECK(r.routes.count() == 0); CHECK(r.nav.resume());
    r.tick(); r.tick(); r.tick(CheckpointSettleMs);
    CHECK(r.nav.state() == Navigation::State::Recording); CHECK(r.routes.count() == 1);
    CHECK(sim::resumes == 0); CHECK(r.nav.finishRecording());
}
void reset_observes_unprocessed_obstacle() {
    Rig r; r.replay(); sim::fault = AtlasFault::Obstacle; sim::latched = true;
    r.safety.update(sim::now); // reset input can precede the next navigation update.
    CHECK(r.nav.resetSafety(sim::now)); r.tick(); r.tick(ObstacleClearanceMs);
    CHECK(r.nav.state() == Navigation::State::Paused); CHECK(!r.motion.moving());
    CHECK(r.nav.resume()); r.tick(); r.tick(DirectionSettleMs); CHECK(r.motion.moving());
}
void reset_between_segments_never_launches() {
    Rig r; r.seed(2); CHECK(r.nav.replay()); r.tick(DirectionSettleMs);
    r.endpoint(); r.tick(); r.tick(CheckpointSettleMs); CHECK(r.routes.index() == 1);
    CHECK(r.nav.resetSafety(sim::now)); r.tick(DirectionSettleMs);
    CHECK(r.nav.state() == Navigation::State::Paused); CHECK(sim::launches == 1);
    CHECK(r.nav.resume()); r.tick(); r.tick(DirectionSettleMs); CHECK(sim::launches == 2);
}
void reset_cannot_erase_raw_motion_failure() {
    Rig r; CHECK(r.nav.driveDistance(10)); r.motion.emergencyStop(AtlasFault::StallLeft);
    // Safety has not yet mirrored the motion failure.
    CHECK(r.nav.resetSafety(sim::now)); r.tick();
    CHECK(r.nav.state() == Navigation::State::Idle); CHECK(!r.nav.movementPending());
    CHECK(!r.nav.resume()); CHECK(sim::resumes == 0);
}
void repeated_reset_does_not_extend_pause_deadline() {
    Rig r; r.replay(); CHECK(r.nav.pause()); r.tick(RouteSegmentTimeoutMs / 2);
    CHECK(r.nav.resetSafety(sim::now)); r.tick(RouteSegmentTimeoutMs / 2);
    CHECK(r.nav.state() == Navigation::State::Fault);
    CHECK(r.routes.error() == Routes::Error::SegmentTimeout);
}
void cancel_after_reset_cannot_resume() {
    Rig r; r.replay(); CHECK(r.nav.pause()); CHECK(r.nav.resetSafety(sim::now)); r.nav.cancel();
    r.tick(DirectionSettleMs); CHECK(!r.nav.resume()); CHECK(!r.motion.moving());
    CHECK(r.nav.replay()); r.tick(DirectionSettleMs); CHECK(sim::launches == 2);
    CHECK(sim::resumes == 0); CHECK(r.routes.index() == 0);
}
void route_start_denied_is_bounded() {
    Rig r; r.seed(); CHECK(r.nav.replay()); sim::denied = true;
    r.tick(DirectionSettleMs); CHECK(r.routes.index() == 0); CHECK(sim::launches == 0);
    r.tick(RouteStartTimeoutMs - DirectionSettleMs);
    CHECK(r.nav.state() == Navigation::State::Fault); CHECK(r.routes.state() == Routes::State::Failed);
    CHECK(r.routes.error() == Routes::Error::StartDenied);
}
void route_total_deadline_spans_segments() {
    Rig r; r.seed(12); CHECK(r.nav.replay()); const auto started = sim::now;
    for (unsigned i = 0; i < 10; ++i) {
        r.tick(DirectionSettleMs); r.tick(59000); r.endpoint(); r.tick(); r.tick(CheckpointSettleMs);
        CHECK(r.nav.state() == Navigation::State::Replaying);
    }
    r.tick(DirectionSettleMs); r.tick(RouteTimeoutMs - (sim::now - started));
    CHECK(r.nav.state() == Navigation::State::Fault); CHECK(r.routes.error() == Routes::Error::RouteTimeout);
}
void recording_session_deadline() {
    Rig r; CHECK(r.nav.beginRecording()); r.tick(RouteTimeoutMs);
    CHECK(r.nav.state() == Navigation::State::Fault); CHECK(r.routes.count() == 0);
    CHECK(r.nav.error() == Navigation::Error::Timeout);
}
void checkpoint_failure_discards_draft_and_recovers() {
    Rig r; CHECK(r.nav.beginRecording()); CHECK(r.nav.driveDistance(10)); r.endpoint(); r.tick();
    sim::stale = true;
    for (unsigned i = 0; i <= CheckpointMaxRetries; ++i) r.tick(CheckpointAttemptTimeoutMs);
    CHECK(r.nav.state() == Navigation::State::Fault); CHECK(r.nav.error() == Navigation::Error::Checkpoint);
    CHECK(r.routes.count() == 0); CHECK(!r.nav.finishRecording());
    sim::stale = false; CHECK(r.nav.resetSafety(sim::now)); r.tick(); CHECK(r.nav.beginRecording());
}

int main() {
    using Test = void (*)();
    const Test tests[] = {recording_and_replay, reset_at_manual_endpoint, reset_at_replay_endpoint,
        reset_observes_unprocessed_fault, reset_clears_queued_resume, route_failure_diagnostic,
        user_pause_preserves_progress, cancel_draft_and_committed, paused_deadline, coast_overshoot_is_rejected,
        checkpoint_stale_retry_and_mismatch, checkpoint_evidence_and_settle, obstacle_clearance_and_limits,
        latched_obstacle_requires_reset_and_resume, rollover_checkpoint_and_pause, invalid_segments_and_capacity,
        all_segment_kinds_record_and_replay, reset_at_recording_endpoint, reset_observes_unprocessed_obstacle,
        reset_between_segments_never_launches, reset_cannot_erase_raw_motion_failure,
        repeated_reset_does_not_extend_pause_deadline, cancel_after_reset_cannot_resume,
        route_start_denied_is_bounded, route_total_deadline_spans_segments, recording_session_deadline,
        checkpoint_failure_discards_draft_and_recovers};
    unsigned failed = 0;
    for (auto test : tests) {
        try { test(); }
        catch (const std::exception& error) { ++failed; std::printf("FAIL %s\n", error.what()); }
    }
    std::printf("%u/%zu tests passed\n", static_cast<unsigned>(sizeof(tests) / sizeof(Test)) - failed, sizeof(tests) / sizeof(Test));
    return failed ? 1 : 0;
}
