#include "PcfManager.h"
#include <Wire.h>

using namespace AtlasConfig;

bool PcfManager::begin() {
    state_.error = Error::Disabled;
    ready_ = false;
    lastReadMs_ = 0;

    if (PcfHardware == PcfVariant::None) {
        // No PCF hardware: nothing to initialize, no I2C activity.
        return true;
    }

    if (!PcfElectricalInterfaceConfirmed ||
        !outputPinAllowed(PcfSdaPin) || !outputPinAllowed(PcfSclPin) ||
        PcfSdaPin == PcfSclPin || PcfSdaPin == UnconfiguredPin ||
        PcfSclPin == UnconfiguredPin || PcfBusHz == 0 || PcfBusHz > 400000 ||
        PcfReadIntervalMs == 0 || PcfAddress < 0x08 || PcfAddress > 0x77 ||
        (PcfHardware == PcfVariant::DualPcf8574 &&
         (PcfAddressB < 0x08 || PcfAddressB > 0x77 || PcfAddressB == PcfAddress)) ||
        (ImuDiagnosticsEnabled &&
         (PcfSdaPin == ImuSdaPin || PcfSclPin == ImuSclPin))) {
        state_.error = Error::Configuration;
        return false;
    }

    state_.error = Error::Starting;
    const bool started = Wire.begin(PcfSdaPin, PcfSclPin, PcfBusHz);
    Wire.setTimeOut(10); // bounded I2C read timeout, milliseconds.
    if (!started) {
        state_.error = Error::Bus;
        return false;
    }
    ready_ = true;
    state_.error = Error::None;
    return true;
}

bool PcfManager::readBytes(uint8_t address, uint8_t* data, size_t count) {
    if (Wire.requestFrom(address, static_cast<uint8_t>(count), true) != count)
        return false;
    for (size_t i = 0; i < count; ++i) {
        const int value = Wire.read();
        if (value < 0) return false;
        data[i] = static_cast<uint8_t>(value);
    }
    return true;
}

void PcfManager::update(uint32_t nowMs) {
    if (!ready_ || PcfHardware == PcfVariant::None) return;
    if (nowMs - lastReadMs_ < PcfReadIntervalMs) return;
    lastReadMs_ = nowMs;

    uint8_t buf[2] = {0, 0};
    bool ok = false;

    if (PcfHardware == PcfVariant::Pcf8575) {
        // PCF8575: 16 bits across two bytes, P0-P7 then P8-P15.
        ok = readBytes(PcfAddress, buf, 2);
    } else if (PcfHardware == PcfVariant::Pcf8574) {
        // Single PCF8574: 8 bits, P0-P7. P8-P15 remain low (unavailable).
        ok = readBytes(PcfAddress, buf, 1);
    } else if (PcfHardware == PcfVariant::DualPcf8574) {
        // Two PCF8574 devices: one byte each.
        uint8_t b0 = 0, b1 = 0;
        ok = readBytes(PcfAddress, &b0, 1) && readBytes(PcfAddressB, &b1, 1);
        buf[0] = b0;
        buf[1] = b1;
    }

    if (ok) {
        state_.pins = static_cast<uint16_t>(buf[0]) |
                      (static_cast<uint16_t>(buf[1]) << 8);
        state_.timestampMs = nowMs;
        state_.valid = true;
        state_.error = Error::None;
    } else {
        state_.valid = false;
        state_.error = Error::Bus;
    }
}

PcfManager::State PcfManager::state(uint32_t nowMs) const {
    State result = state_;
    if (result.valid &&
        nowMs - result.timestampMs > static_cast<uint32_t>(PcfReadIntervalMs * 4)) {
        result.valid = false;
        result.error = Error::Bus; // treated as stale / no recent observation
    }
    return result;
}

bool PcfManager::readPin(uint8_t pin, uint32_t nowMs) const {
    if (pin > 15) return false;
    const State s = state(nowMs);
    if (!s.valid) return false;
    return static_cast<bool>((s.pins >> pin) & 1u);
}
