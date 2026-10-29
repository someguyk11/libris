#pragma once

#include "BackendClient.h"
#include "NetworkManager.h"
#include "Navigation.h"
#include "SupabaseClient.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>

// Main loop owns navigation/sensors/encoders. The worker owns Wi-Fi and HTTP.
// Fixed-size copied queues are the only cross-task payloads; no shared JSON/Strings.
class TelemetryManager {
public:
    TelemetryManager(MotionController& motion, SensorManager& sensors,
        SafetyController& safety, Navigation& navigation)
        : motion_(motion), sensors_(sensors), safety_(safety), navigation_(navigation) {}
    bool begin();
    void update(uint32_t nowMs);
    bool connected() const { return network_.connected(); }
    bool localDriveAllowed() const { return network_.localDriveAllowed(); }
    bool backendReady() const { return healthy_.load() && !deliveryFault_.load(); }
private:
    struct Ack { char id[37]; char controlId[37]; BackendClient::ResultStatus status; bool ok; };
    enum class Phase : uint8_t { Idle, AwaitingStart, Running, AwaitingPauseAck, Paused, AwaitingResumeAck, Resuming };
    static void taskEntry(void* context);
    void worker();
    void command(const BackendClient::Command& command, uint32_t nowMs);
    bool fresh(uint32_t nowMs) const;
    BackendClient::Command rowToCommand(const SupabaseClient::CommandRow& row);
    BackendClient::Sensors capture(uint32_t nowMs) const;
    BackendClient::Result result(const BackendClient::Command& command,
        BackendClient::ResultStatus status, uint32_t nowMs) const;
    void measure(BackendClient::Result& result, BackendClient::CommandKind kind,
        uint32_t leftStart, uint32_t rightStart) const;
    bool enqueue(const BackendClient::Result& result);
    void finish(bool completed, const char* error, uint32_t nowMs);
    bool seen(const char* id) const;
    void remember(const char* id);

    MotionController& motion_;
    SensorManager& sensors_;
    SafetyController& safety_;
    Navigation& navigation_;
    Atlas::NetworkManager network_;
    BackendClient backend_;
    SupabaseClient supabase_;
    QueueHandle_t commands_ = nullptr, results_ = nullptr, acks_ = nullptr, samples_ = nullptr;
    std::atomic<bool> healthy_{false}, deliveryFault_{false}, activeMovement_{false};
    std::atomic<bool> recoveryHold_{false}, recoveryApproved_{false};
    std::atomic<bool> stoppedSnapshot_{true};
    std::atomic<uint32_t> generation_{0};
    Phase phase_ = Phase::Idle;
    BackendClient::Command active_;
    char controlId_[37] = {};
    uint32_t lastPollMs_ = 0, lastSampleMs_ = 0, leftStart_ = 0, rightStart_ = 0;
    uint32_t highSequence_ = 0, observedGeneration_ = 0;
    char recent_[AtlasConfig::RecentCommandCapacity][37] = {};
    uint8_t recentIndex_ = 0;
    bool begun_ = false;
};
