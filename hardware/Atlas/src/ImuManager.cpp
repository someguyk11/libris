#include "ImuManager.h"
#include <Wire.h>

using namespace AtlasConfig;

namespace {
bool configurationValid() {
    if (!ImuElectricalInterfaceConfirmed || !outputPinAllowed(ImuSdaPin) ||
        !outputPinAllowed(ImuSclPin) || ImuSdaPin == ImuSclPin ||
        (ImuAddress != 0x6A && ImuAddress != 0x6B) || ImuBusHz == 0 || ImuBusHz > 400000 ||
        ImuBusTimeoutMs == 0 || ImuBusTimeoutMs > 20 || ImuSampleIntervalMs == 0 ||
        ImuMaximumAgeMs <= ImuSampleIntervalMs) return false;
    const int used[] = {LeftMotor.in1, LeftMotor.in2, RightMotor.in1, RightMotor.in2,
        DriverSleepPin, SecondDriverSleepPin, LeftEncoderPin, RightEncoderPin,
        Sensors[0].trigger, Sensors[0].echo, Sensors[1].trigger, Sensors[1].echo,
        Sensors[2].trigger, Sensors[2].echo, Sensors[3].trigger, Sensors[3].echo};
    for (int pin : used) if (pin == ImuSdaPin || pin == ImuSclPin) return false;
    return true;
}
// Register definitions verified against ST's lsm6dso-pid register driver.
bool readRegisters(uint8_t reg, uint8_t* data, size_t count) {
    Wire.beginTransmission(ImuAddress);
    if (Wire.write(reg) != 1 || Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(ImuAddress, count, true) != count) return false;
    for (size_t i = 0; i < count; ++i) {
        const int value = Wire.read();
        if (value < 0) return false;
        data[i] = static_cast<uint8_t>(value);
    }
    return true;
}
bool writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(ImuAddress);
    const uint8_t bytes[] = {reg, value};
    return Wire.write(bytes, sizeof(bytes)) == sizeof(bytes) && Wire.endTransmission(true) == 0;
}
}

bool ImuManager::begin() {
    if (!ImuDiagnosticsEnabled) return true;
    if (samples_) return task_ != nullptr;
    samples_ = xQueueCreate(1, sizeof(Reading));
    if (!samples_) return false;
    Reading initial;
    initial.error = configurationValid() ? Error::Starting : Error::Configuration;
    xQueueOverwrite(samples_, &initial);
    if (initial.error == Error::Configuration) return false;
    return xTaskCreatePinnedToCore(workerEntry, "atlas-imu", 4096, this, 1, &task_, 0) == pdPASS;
}

ImuManager::Reading ImuManager::reading(uint32_t nowMs) const {
    Reading result;
    if (ImuDiagnosticsEnabled) {
        result.error = Error::Configuration;
        if (samples_) xQueuePeek(samples_, &result, 0);
        if (result.valid && nowMs - result.timestampMs > ImuMaximumAgeMs) {
            result.valid = false;
            result.error = Error::Stale;
        }
    }
    return result;
}

void ImuManager::workerEntry(void* context) { static_cast<ImuManager*>(context)->worker(); }

void ImuManager::worker() {
    Reading sample;
    sample.error = Error::Bus;
    bool ready = Wire.begin(ImuSdaPin, ImuSclPin, ImuBusHz);
    Wire.setTimeOut(ImuBusTimeoutMs);
    uint8_t value = 0;
    if (ready) {
        ready = readRegisters(0x0F, &value, 1) && value == 0x6C;
        if (!ready) sample.error = Error::Identity;
    }
    if (ready) {
        ready = writeRegister(0x12, 0x01); // bounded software reset; never an actuation self-test.
        const uint32_t started = millis();
        while (ready && millis() - started < 100) {
            ready = readRegisters(0x12, &value, 1);
            if (!ready || !(value & 1)) break;
            vTaskDelay(pdMS_TO_TICKS(2));
        }
        ready = ready && !(value & 1);
        if (!ready) sample.error = Error::Reset;
    }
    // BDU + increment; 104 Hz, 2 g / 250 dps. Accelerometer LPF2 ODR/10;
    // gyro LPF1 default ftype=0. No invented tilt/stall/calibration thresholds.
    const uint8_t settings[][2] = {{0x12,0x44}, {0x10,0x42}, {0x11,0x40},
        {0x13,0x02}, {0x14,0x00}, {0x15,0x00}, {0x16,0x00}, {0x17,0x20}};
    if (ready) {
        sample.error = Error::Bus;
        for (const auto& setting : settings) {
            if (!writeRegister(setting[0], setting[1]) || !readRegisters(setting[0], &value, 1) ||
                value != setting[1]) { ready = false; break; }
        }
    }
    xQueueOverwrite(samples_, &sample);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(ImuSampleIntervalMs));
        if (!ready) continue; // explicit reboot/reconfiguration required; never authorize movement.
        if (!readRegisters(0x1E, &value, 1)) {
            sample.valid = false;
            sample.error = Error::Bus;
        } else if ((value & 3) == 3) {
            uint8_t bytes[12];
            sample.valid = readRegisters(0x22, bytes, sizeof(bytes));
            sample.error = sample.valid ? Error::None : Error::Bus;
            if (sample.valid) {
                for (uint8_t i = 0; i < 6; ++i) {
                    const int16_t raw = static_cast<int16_t>(static_cast<uint16_t>(bytes[i*2]) |
                        (static_cast<uint16_t>(bytes[i*2+1]) << 8));
                    if (i < 3) sample.angularRateMdps[i] = raw * 8.75f;
                    else sample.accelerationMg[i-3] = raw * 0.061f;
                }
                sample.timestampMs = millis();
            }
        }
        xQueueOverwrite(samples_, &sample); // no new DRDY keeps actual age; accessor rejects stale.
    }
}
