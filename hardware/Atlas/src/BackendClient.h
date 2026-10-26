#pragma once

#include "AtlasConfig.h"
#include <ArduinoJson.h>

class BackendClient {
public:
    enum class CommandKind : uint8_t { Stop, Forward, Backward, Left, Right };
    enum class ResultStatus : uint8_t { Started, Completed, Failed, Paused, Resumed };
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
        bool autoReconnect = true;
        char recoveryId[37] = {};
        enum class Recovery : uint8_t { None, Reconnect, Restart, AutoReconnect } recovery = Recovery::None;
        bool recoveryEnabled = true;
        char ssid[32] = {};
        char password[64] = {};
        bool guidance = false, serverPaused = false;
        char taskId[37] = {}, sessionId[37] = {}, routeId[37] = {};
        uint32_t leftTicks = 0, rightTicks = 0;
        enum class Control : uint8_t { None, Pause, Resume } control = Control::None;
        char controlId[37] = {};
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
        bool guidance = false;
        char taskId[37] = {}, sessionId[37] = {}, routeId[37] = {}, controlId[37] = {};
    };
    struct SensorSample { char id[37] = {}; char recordingId[37] = {}; uint32_t sequence = 0; Sensors sensors; };

    // Network-worker only. No retries or sleeps outside bounded response reads.
    static bool configured();
    bool poll(Command& command);
    bool pollGuidance(Command& command, bool stopped);
    bool sendResult(const Result& result);
    bool sendSensors(const SensorSample& sample);
    bool sendTelemetry(const char* ssid, int signalStrength, uint64_t uptimeSeconds);
    bool acknowledgeRecovery(const Command& command);
    void recoveryApplied(const Command& command) { strlcpy(completedRecovery_, command.recoveryId, sizeof(completedRecovery_)); }
    int lastHttpStatus() const { return lastHttpStatus_; }
    static int64_t utcNowUs();
private:
    bool request(const char* endpoint, const char* payload, bool raw = false);
    static bool timestamp(int64_t epochUs, char* output, size_t size);
    static void addSensors(JsonObject target, const Sensors& sensors);
    bool postDocument(const char* endpoint, JsonDocument& document, bool raw = false);
    char response_[AtlasConfig::MaximumHttpResponseBytes + 1] = {};
    char payload_[AtlasConfig::MaximumHttpRequestBytes + 1] = {};
    JsonDocument responseDocument_;
    int lastHttpStatus_ = 0;
    char bootId_[33] = {};
    char completedRecovery_[37] = {};
};
