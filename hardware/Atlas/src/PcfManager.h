#pragma once

#include "AtlasConfig.h"

// PCF8574 / PCF8575 I2C port expander digital input reader.
// Reads all 16 PCF port pins (P0..P15) over I2C and exposes their levels.
// PCF8575  = single 16-bit device  (2 bytes: P0-P7, then P8-P15)
// Pcf8574  = single 8-bit device   (1 byte:  P0-P7; P8-P15 report low)
// DualPcf8574 = two 8-bit devices  (1 byte per device)
// Poll-driven: call update() from the main loop; no FreeRTOS task.
class PcfManager {
public:
    enum class Error : uint8_t { Disabled, Configuration, Starting, Bus, None };
    struct State {
        uint16_t pins = 0;        // bit i = level of PCF pin i (1 = high, 0 = low)
        uint32_t timestampMs = 0;
        bool valid = false;
        Error error = Error::Disabled;
    };
    bool begin();
    void update(uint32_t nowMs);
    State state(uint32_t nowMs) const;
    // Level of one PCF pin (0..15). Returns false when out of range or invalid.
    bool readPin(uint8_t pin, uint32_t nowMs) const;
    bool ready() const { return ready_; }
private:
    bool readBytes(uint8_t address, uint8_t* data, size_t count);
    State state_;
    bool ready_ = false;
    uint32_t lastReadMs_ = 0;
};
