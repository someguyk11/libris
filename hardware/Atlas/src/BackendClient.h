#pragma once

#include "AtlasConfig.h"
#include <ArduinoJson.h>

class BackendClient {
public:
    enum class CommandKind : uint8_t { Stop, Forward, Backward, Left, Right };
    enum class ResultStatus : uint8_t { Started, Completed, Failed };
    enum class Direction : uint8_t { Stop, Forward, Backward };
    struct Command {
        CommandKind kind = CommandKind::Stop;
        char id[37] = {};
        char recordingId[37] = {};
        uint32_t transportSequence = 0;
        uint32_t sequence = 0;
        uint32_t fetchedAtMs = 0; // request START, not response receipt.
        uint32_t generation = 0;
        uint32_t linkGeneration = 0;
        bool execute = false;
        bool baseline = false; // boot/reconnect first response must not start motion.
        bool factoryReset = false;
        bool reconfigure = false;
        char ssid[32] = {};
        char password[64] = {};
    };
    struct Sensors {
        float cm[AtlasConfig::SensorCount] = {};
        uint8_t validMask = 0;
        int64_t capturedAtUs = 0;
    };
    struct Result {
        char id[37] = {};
        char recordingId[37] = {};
        uint32_t sequence = 0;
        ResultStatus status = ResultStatus::Failed;
        int64_t capturedAtUs = 0;
        bool measured = false;
        uint32_t leftTicks = 0;
        uint32_t rightTicks = 0;
        Direction leftDirection = Direction::Stop;
        Direction rightDirection = Direction::Stop;
        Sensors sensors;
        char error[128] = {};
    };
    struct SensorSample { char id[37] = {}; char recordingId[37] = {}; uint32_t sequence = 0; Sensors sensors; };

    // Network-worker only. No retries or sleeps outside bounded response reads.
    static bool configured();
    bool poll(Command& command);
    bool sendResult(const Result& result);
    bool sendSensors(const SensorSample& sample);
    bool sendTelemetry(const char* ssid, int signalStrength, uint64_t uptimeSeconds);
    int lastHttpStatus() const { return lastHttpStatus_; }
    static int64_t utcNowUs();
private:
    bool request(const char* endpoint, const char* payload);
    static bool timestamp(int64_t epochUs, char* output, size_t size);
    static void addSensors(JsonObject target, const Sensors& sensors);
    bool postDocument(const char* endpoint, JsonDocument& document);
    char response_[AtlasConfig::MaximumHttpResponseBytes + 1] = {};
    char payload_[AtlasConfig::MaximumHttpRequestBytes + 1] = {};
    JsonDocument responseDocument_;
    int lastHttpStatus_ = 0;
};
