#include "SensorManager.h"
#include <esp_timer.h>

using namespace AtlasConfig;

void ARDUINO_ISR_ATTR SensorManager::echoInterrupt(void* context) {
    EchoCapture* capture = static_cast<EchoCapture*>(context);
    const bool high = digitalRead(capture->pin) == HIGH;
    const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());
    portENTER_CRITICAL_ISR(&capture->lock);
    if (capture->armed) {
        if (high && !capture->risen) {
            capture->riseUs = nowUs;
            capture->risen = true;
        } else if (!high && capture->risen) {
            capture->fallUs = nowUs;
            capture->done = true;
            capture->armed = false;
        }
    }
    portEXIT_CRITICAL_ISR(&capture->lock);
}

bool SensorManager::begin() {
    if (ready_) return true;
    if (pinConfigurationError() || tuningError()) {
        for (uint8_t i = 0; i < SensorCount; ++i) {
            readings_[i].error = Error::NotConfigured;
            readings_[i].faulted = true;
        }
        return false;
    }
    for (uint8_t i = 0; i < SensorCount; ++i) {
        digitalWrite(Sensors[i].trigger, LOW);
        pinMode(Sensors[i].trigger, OUTPUT);
        pinMode(Sensors[i].echo, INPUT);
        captures_[i].pin = Sensors[i].echo;
        attachInterruptArg(Sensors[i].echo, echoInterrupt, &captures_[i], CHANGE);
    }
    lastSlotMs_ = millis() - SensorSlotIntervalMs;
    ready_ = true;
    return true;
}

void SensorManager::disarm(uint8_t index) {
    portENTER_CRITICAL(&captures_[index].lock);
    captures_[index].armed = false;
    captures_[index].done = false;
    portEXIT_CRITICAL(&captures_[index].lock);
}

void SensorManager::invalidate(uint8_t index, Error error) {
    Reading& result = readings_[index];
    result.valid = false; // never carry forward the last clear value after a failed ping.
    result.error = error;
    if (result.consecutiveFailures < UINT16_MAX) ++result.consecutiveFailures;
    result.faulted = result.consecutiveFailures >= SensorFaultAfterFailures;
}

SensorManager::Reading SensorManager::reading(SensorId sensor, uint32_t nowMs) const {
    const uint8_t index = static_cast<uint8_t>(sensor);
    if (index >= SensorCount) {
        Reading invalid;
        invalid.error = Error::NotConfigured;
        invalid.faulted = true;
        return invalid;
    }
    Reading result = readings_[index];
    if (result.valid && nowMs - result.timestampMs > MaximumReadingAgeMs) {
        result.valid = false;
        result.faulted = true;
        result.error = Error::Stale;
    }
    return result;
}

void SensorManager::update(uint32_t nowMs) {
    if (!ready_) return;
    if (active_ >= 0) {
        EchoCapture& capture = captures_[active_];
        portENTER_CRITICAL(&capture.lock);
        const bool done = capture.done;
        const uint64_t riseUs = capture.riseUs;
        const uint64_t fallUs = capture.fallUs;
        portEXIT_CRITICAL(&capture.lock);
        // Sample time AFTER the ISR snapshot: a just-arrived edge must not
        // appear to be in the future and underflow the unsigned age check.
        const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());
        if (done) {
            disarm(active_);
            const uint64_t pulseUs = fallUs - riseUs;
            const float cm = pulseUs * SoundSpeedCmPerUs * 0.5f;
            if (riseUs < measurementStartUs_ || fallUs < riseUs ||
                fallUs - measurementStartUs_ > EchoTimeoutUs ||
                nowUs - fallUs > MaximumReadingAgeMs * 1000ULL) {
                invalidate(active_, Error::Timeout);
            } else if (!isfinite(cm) || cm < SensorMinCm || cm > SensorMaxCm) {
                invalidate(active_, Error::OutOfRange);
            } else {
                Reading& result = readings_[active_];
                result.distanceCm = cm;
                // Preserve the actual sample age if the main loop ran late.
                result.timestampMs = nowMs - static_cast<uint32_t>((nowUs - fallUs) / 1000ULL);
                result.valid = true;
                result.faulted = false;
                result.error = Error::None;
                result.consecutiveFailures = 0;
            }
            active_ = -1;
        } else if (nowUs - measurementStartUs_ >= EchoTimeoutUs) {
            disarm(active_);
            invalidate(active_, Error::Timeout);
            active_ = -1;
        }
    }
    if (active_ >= 0 || nowMs - lastSlotMs_ < SensorSlotIntervalMs) return;
    lastSlotMs_ = nowMs; // never send a burst of catch-up pings after a late loop.
    const uint8_t index = next_;
    next_ = (next_ + 1) % SensorCount;
    if (digitalRead(Sensors[index].echo) == HIGH) {
        invalidate(index, Error::StuckHigh);
        return;
    }
    EchoCapture& capture = captures_[index];
    portENTER_CRITICAL(&capture.lock);
    capture.armed = true;
    capture.risen = false;
    capture.done = false;
    capture.riseUs = 0;
    capture.fallUs = 0;
    portEXIT_CRITICAL(&capture.lock);
    active_ = index;
    measurementStartUs_ = static_cast<uint64_t>(esp_timer_get_time());
    digitalWrite(Sensors[index].trigger, HIGH);
    delayMicroseconds(TriggerPulseUs); // 10-20 us bounded trigger; no pulseIn / echo waiting.
    digitalWrite(Sensors[index].trigger, LOW);
}
