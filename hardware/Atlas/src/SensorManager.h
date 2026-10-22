#pragma once

#include "AtlasConfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

class SensorManager {
public:
    enum class Error : uint8_t { None, NotConfigured, NoReading, Timeout, StuckHigh, OutOfRange, Stale };
    struct Reading {
        float distanceCm = 0;
        uint32_t timestampMs = 0;
        uint16_t consecutiveFailures = 0;
        bool valid = false;
        bool faulted = false;
        Error error = Error::NoReading;
    };
    bool begin();
    void update(uint32_t nowMs);
    Reading reading(AtlasConfig::SensorId sensor, uint32_t nowMs) const;
    bool ready() const { return ready_; }

private:
    struct EchoCapture {
        portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
        int pin = AtlasConfig::UnconfiguredPin;
        volatile bool armed = false;
        volatile bool risen = false;
        volatile bool done = false;
        volatile uint64_t riseUs = 0;
        volatile uint64_t fallUs = 0;
    } captures_[AtlasConfig::SensorCount];
    static void ARDUINO_ISR_ATTR echoInterrupt(void* context);
    void invalidate(uint8_t index, Error error);
    void disarm(uint8_t index);
    Reading readings_[AtlasConfig::SensorCount];
    bool ready_ = false;
    int active_ = -1;
    uint8_t next_ = 0;
    uint64_t measurementStartUs_ = 0;
    uint32_t lastSlotMs_ = 0;
};
