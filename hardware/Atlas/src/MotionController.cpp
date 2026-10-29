#include "MotionController.h"
#include <esp_timer.h>

using namespace AtlasConfig;

namespace {
void setDriverSleep(uint8_t level) {
    for (int pin : DriverSleepPins)
        if (pin != UnconfiguredPin) digitalWrite(pin, level);
}
int motorPin(uint8_t index) {
    const int pins[] = {LeftMotor.in1, LeftMotor.in2, RightMotor.in1, RightMotor.in2};
    return pins[index];
}
float ramp(float current, float target, float seconds) {
    if (target > current) return fminf(target, current + AccelerationDutyPerSecond * seconds);
    return fmaxf(target, current - DecelerationDutyPerSecond * seconds);
}
}

void ARDUINO_ISR_ATTR MotionController::encoderInterrupt(void* context) {
    Encoder* encoder = static_cast<Encoder*>(context);
    const uint32_t nowUs = static_cast<uint32_t>(esp_timer_get_time());
    portENTER_CRITICAL_ISR(&encoder->lock);
    if (!encoder->seenEdge || nowUs - encoder->lastEdgeUs >= EncoderMinEdgeIntervalUs) {
        encoder->counts = encoder->counts + 1;
        encoder->lastEdgeUs = nowUs;
        encoder->seenEdge = true;
    }
    portEXIT_CRITICAL_ISR(&encoder->lock);
}

bool MotionController::begin() {
    if (ready_) return true;
    if (pinConfigurationError() || tuningError() || !SingleSignalEncodersConfirmed ||
        CountEdges == EncoderEdges::Unconfigured) {
        fault_ = AtlasFault::Configuration;
        state_ = State::Failed;
        return false; // no unknown GPIO is touched.
    }
    for (int pin : DriverSleepPins) {
        if (pin == UnconfiguredPin) continue;
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        sleepInitialized_ = true;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        digitalWrite(motorPin(i), LOW);
        pinMode(motorPin(i), OUTPUT);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        attached_[i] = ledcAttach(motorPin(i), PwmFrequencyHz, PwmResolutionBits);
#else
        attached_[i] = ledcSetup(FirstPwmChannel + i, PwmFrequencyHz, PwmResolutionBits) > 0;
        if (attached_[i]) ledcAttachPin(motorPin(i), FirstPwmChannel + i);
#endif
        if (!attached_[i] || !writePwm(i, 0)) {
            emergencyStop(AtlasFault::Pwm);
            return false;
        }
    }
    const int mode = EncoderInternalPullup ? INPUT_PULLUP : INPUT;
    pinMode(LeftEncoderPin, mode);
    pinMode(RightEncoderPin, mode);
    const int edge = CountEdges == EncoderEdges::Both ? CHANGE :
        (CountEdges == EncoderEdges::Rising ? RISING : FALLING);
    attachInterruptArg(LeftEncoderPin, encoderInterrupt, &encoders_[0], edge);
    attachInterruptArg(RightEncoderPin, encoderInterrupt, &encoders_[1], edge);
    ready_ = true;
    fault_ = AtlasFault::None;
    state_ = State::Idle;
    stoppedAtMs_ = millis();
    return true;
}

bool MotionController::writePwm(uint8_t index, uint32_t duty) {
    if (!attached_[index]) return false;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    return ledcWrite(motorPin(index), duty);
#else
    ledcWrite(FirstPwmChannel + index, duty);
    return true; // the 2.x write API has no return status.
#endif
}

bool MotionController::drive(uint8_t side, float duty, bool forward) {
    const bool invert = side == 0 ? LeftMotor.inverted : RightMotor.inverted;
    if (invert) forward = !forward;
    const uint8_t active = side * 2 + (forward ? 0 : 1);
    const uint8_t inactive = side * 2 + (forward ? 1 : 0);
    // Clear the inactive leg first; do not reverse a powered H-bridge.
    const bool inactiveOk = writePwm(inactive, 0);
    return inactiveOk && writePwm(active, static_cast<uint32_t>(duty));
}

void MotionController::stopOutputs(StopMode mode) {
    // Partial initialization or a failed PWM interface must never wake a bridge.
    if (!ready_ || fault_ == AtlasFault::Pwm || fault_ == AtlasFault::Configuration)
        mode = StopMode::Coast;
    // Remove drive before updating PWM when a hardware disable is available.
    if (sleepInitialized_ && mode == StopMode::Coast) setDriverSleep(LOW);
    bool ok = true;
    const uint32_t level = mode == StopMode::Brake ? (1UL << PwmResolutionBits) - 1 : 0;
    for (uint8_t i = 0; i < 4; ++i)
        if (attached_[i] && !writePwm(i, level)) ok = false;
    // nSLEEP low disables both bridges (coast). Brake requires the driver awake.
    if (sleepInitialized_)
        setDriverSleep(mode == StopMode::Brake && ok ? HIGH : LOW);
    if (!ok) {
        // Detach PWM and force LOW if the core reports a write failure.
        for (uint8_t i = 0; i < 4; ++i) {
            if (!attached_[i]) continue;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
            ledcDetach(motorPin(i));
#else
            ledcDetachPin(motorPin(i));
#endif
            attached_[i] = false;
            pinMode(motorPin(i), OUTPUT);
            digitalWrite(motorPin(i), LOW);
        }
        ready_ = false;
        fault_ = AtlasFault::Pwm;
        state_ = State::Failed;
        safetyPermit_ = false;
    }
    duties_[0] = duties_[1] = 0;
    stoppedAtMs_ = millis();
}

void MotionController::stop() {
    stopOutputs(NormalStopMode);
    if (state_ != State::Failed) state_ = State::Stopped;
}

void MotionController::emergencyStop(AtlasFault reason) {
    safetyPermit_ = false;
    fault_ = reason;
    state_ = State::Failed;
    stopOutputs(EmergencyStopMode);
}

void MotionController::setSafetyPermit(bool permitted) {
    safetyPermit_ = permitted;
    if (!permitted && moving()) emergencyStop(AtlasFault::EmergencyStop);
}

void MotionController::clearFailure() {
    if (ready_) { fault_ = AtlasFault::None; state_ = State::Idle; }
}

void MotionController::encoderCounts(uint32_t& left, uint32_t& right) {
    portENTER_CRITICAL(&encoders_[0].lock);
    left = encoders_[0].counts;
    portEXIT_CRITICAL(&encoders_[0].lock);
    portENTER_CRITICAL(&encoders_[1].lock);
    right = encoders_[1].counts;
    portEXIT_CRITICAL(&encoders_[1].lock);
}

float MotionController::distanceFor(uint8_t side, uint32_t counts) const {
    const float cpr = side == 0 ? LeftCountsPerWheelRevolution : RightCountsPerWheelRevolution;
    return positive(cpr) ? counts * (3.141592654f * WheelDiameterCm / cpr) : 0;
}

MotionController::Progress MotionController::progress() {
    uint32_t left, right;
    encoderCounts(left, right);
    const float l = distanceFor(0, left - baseline_[0]);
    const float r = distanceFor(1, right - baseline_[1]);
    const float lt = tickReplay_ ? distanceFor(0, tickTarget_[0]) : targetCm_;
    const float rt = tickReplay_ ? distanceFor(1, tickTarget_[1]) : targetCm_;
    const float fraction = lt > 0 && rt > 0 ? fminf(1.0f, fminf(l / lt, r / rt)) : 0;
    return {state_, kind_, l, r, targetCm_, fraction, lt, rt};
}

bool MotionController::start(float targetCm, Kind kind, uint16_t duty, bool resuming) {
    const uint32_t nowMs = millis();
    if (!ready_ || !safetyPermit_ || moving() || fault_ != AtlasFault::None ||
        !distanceCalibrated() || !positive(targetCm) || targetCm <= DistanceToleranceCm ||
        targetCm > MaximumMoveCm || duty < MinimumDriveDuty || duty > MaxDuty ||
        nowMs - stoppedAtMs_ < DirectionSettleMs) return false;
    // Release a previous brake before ramping from zero. Otherwise clearing just
    // one HIGH brake input would briefly apply full reverse/forward drive.
    stopOutputs(StopMode::Coast);
    if (!ready_ || fault_ != AtlasFault::None) return false;
    if (!resuming) { encoderCounts(baseline_[0], baseline_[1]); tickReplay_ = false; }
    duties_[0] = duties_[1] = 0;
    targetCm_ = targetCm;
    kind_ = kind;
    requestedDuty_ = duty;
    startedAtMs_ = lastControlMs_ = nowMs;
    state_ = State::Moving;
    resumable_ = true;
    return true; // hardware output is deferred until the next safety-checked update.
}

bool MotionController::moveDistance(float cm, uint16_t duty) {
    return start(fabsf(cm), cm > 0 ? Kind::Forward : Kind::Backward, duty);
}

bool MotionController::turn90(bool clockwise, uint16_t duty) {
    if (!turnsCalibrated()) return false;
    const float scale = clockwise ? ClockwiseTurnScale : CounterclockwiseTurnScale;
    const float wheelTravelCm = 3.141592654f * WheelTrackCm * 0.25f * scale;
    return start(wheelTravelCm, clockwise ? Kind::Clockwise90 : Kind::Counterclockwise90, duty);
}

bool MotionController::replayTicks(uint32_t left, uint32_t right, Kind kind) {
    if (!left || !right || (kind != Kind::Forward && kind != Kind::Backward &&
        kind != Kind::Clockwise90 && kind != Kind::Counterclockwise90)) return false;
    const float l = distanceFor(0, left), r = distanceFor(1, right);
    if (!positive(l) || !positive(r) || l > MaximumMoveCm || r > MaximumMoveCm) return false;
    const bool turn = kind == Kind::Clockwise90 || kind == Kind::Counterclockwise90;
    if (turn) {
        if (!turnsCalibrated()) return false;
        const float scale = kind == Kind::Clockwise90 ? ClockwiseTurnScale : CounterclockwiseTurnScale;
        const float target = 3.141592654f * WheelTrackCm * 0.25f * scale;
        // Metadata alone is insufficient: reject tick targets inconsistent with local 90-degree calibration.
        if (fabsf(l - target) > CheckpointDistanceToleranceCm ||
            fabsf(r - target) > CheckpointDistanceToleranceCm) return false;
    } else if (fabsf(l - r) > CheckpointWheelMismatchCm) return false;
    if (!start(fmaxf(l, r), kind, turn ? TurnDuty : CruiseDuty)) return false;
    tickTarget_[0] = left; tickTarget_[1] = right; tickReplay_ = true;
    return true;
}

bool MotionController::resume() {
    if (!resumable_ || moving() || state_ == State::Complete ||
        (kind_ != Kind::Forward && kind_ != Kind::Backward && !turnsCalibrated())) return false;
    return start(targetCm_, kind_, requestedDuty_, true);
}

void MotionController::update(uint32_t nowMs) {
    if (!moving()) return;
    if (!safetyPermit_) { emergencyStop(); return; }
    const uint32_t elapsed = nowMs - lastControlMs_;
    if (elapsed > MaximumControlGapMs) { emergencyStop(AtlasFault::ControlOverrun); return; }
    if (elapsed < ControlIntervalMs) return;
    lastControlMs_ = nowMs;
    const Progress p = progress();
    const float remaining[2] = {p.leftTargetCm - p.leftCm, p.rightTargetCm - p.rightCm};
    uint32_t left = 0, right = 0;
    if (tickReplay_) {
        encoderCounts(left, right);
        left -= baseline_[0]; right -= baseline_[1];
        if (left > tickTarget_[0] || right > tickTarget_[1]) { emergencyStop(AtlasFault::EncoderTarget); return; }
    }
    const bool reached[2] = {tickReplay_ ? left == tickTarget_[0] : remaining[0] <= DistanceToleranceCm,
                            tickReplay_ ? right == tickTarget_[1] : remaining[1] <= DistanceToleranceCm};
    if (reached[0] && reached[1]) {
        stopOutputs(NormalStopMode);
        if (state_ != State::Failed) { state_ = State::Complete; resumable_ = false; }
        return;
    }
    for (uint8_t side = 0; side < 2; ++side) {
        if (reached[side]) {
            duties_[side] = 0; // each wheel group stops at its own encoder target.
        } else {
            const float ratio = fminf(1.0f, remaining[side] / SlowdownDistanceCm);
            const float targetDuty = MinimumDriveDuty + (requestedDuty_ - MinimumDriveDuty) * ratio;
            duties_[side] = ramp(duties_[side], targetDuty, elapsed * 0.001f);
        }
        const bool forward = kind_ == Kind::Forward ||
            (kind_ == Kind::Clockwise90 && side == 0) ||
            (kind_ == Kind::Counterclockwise90 && side == 1);
        if (!drive(side, duties_[side], forward)) { emergencyStop(AtlasFault::Pwm); return; }
    }
    if (sleepInitialized_) setDriverSleep(HIGH);
}
