#pragma once

#include "AtlasConfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// Dedicated I2C owner. The real-time loop only copies a bounded snapshot;
// missing IMU data is never substituted with a stationary/level measurement.
class ImuManager {
public:
    enum class Error : uint8_t { Disabled, Configuration, Starting, Identity, Bus, Reset, Stale, None };
    struct Reading {
        float accelerationMg[3] = {};
        float angularRateMdps[3] = {};
        uint32_t timestampMs = 0;
        bool valid = false;
        Error error = Error::Disabled;
    };
    bool begin();
    Reading reading(uint32_t nowMs) const;
private:
    static void workerEntry(void* context);
    void worker();
    QueueHandle_t samples_ = nullptr;
    TaskHandle_t task_ = nullptr;
};
