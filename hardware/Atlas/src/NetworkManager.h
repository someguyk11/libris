#pragma once

#include "AtlasConfig.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <atomic>

// Worker-only except atomic getters and the explicitly main-only approval below.
// Legacy 512-byte EEPROM layout is preserved exactly (5 x 96-byte profiles).
// Arduino-ESP32 3.x already defines a global NetworkManager: keep ours scoped.
namespace Atlas {
class NetworkManager {
public:
    void begin();
    void update(uint32_t nowMs);
    bool connected() const { return linkUp_.load(); }
    uint32_t generation() const { return generation_.load(); }
    bool configurationPending() const { return configurationRequest_.load() != configurationCompleted_.load(); }
    bool localDriveAllowed() const { return AtlasConfig::LocalDriveEnabled && !localWifiMode_.load() && !configurationPending(); }
    // Main loop only, AFTER cancelling navigation. Flash writes may stall both cores.
    void approveConfiguration() { configurationApproved_.store(configurationRequest_.load()); }
    bool reconfigure(const char* ssid, const char* password);
    void factoryReset();
    void setAutoReconnect(bool enabled) { autoReconnect_ = enabled; }
    void reconnect(); // worker only, AFTER main-loop stop approval.
private:
    struct Profile { char ssid[32]; char password[64]; };
    enum class Configuration : uint8_t { None, Save, SaveOnly, Forget, Reset, LocalWifi, LocalDrive };
    bool queueConfiguration(Configuration action, const Profile& profile, uint8_t slot = 0);
    void applyConfiguration();
    bool load(uint8_t slot, Profile& profile);
    bool save(const char* ssid, const char* password);
    bool forget(uint8_t slot);
    void startAttempt(const Profile& profile, uint32_t nowMs);
    void startPortal();
    void portal(uint32_t nowMs);
    void handleRequest();
    void reply(int code, const char* type, const char* body, size_t length = 0, bool gzip = false);
    void closeClient();
    bool eepromReady_ = false;
    bool apActive_ = false;
    bool attempting_ = false;
    bool wasConnected_ = false;
    bool autoConnectHeld_ = false; // Save-only must not immediately reconnect using the new profile.
    bool autoReconnect_ = true;
    bool connectionFailed_ = false;
    char attemptSsid_[32]{};
    uint8_t nextSlot_ = 0;
    uint32_t attemptAtMs_ = 0;
    uint32_t retryAtMs_ = 0;
    uint32_t backoffMs_ = AtlasConfig::WifiRetryInitialMs;
    uint32_t connectedAtMs_ = 0;
    uint32_t clientAtMs_ = 0;
    uint32_t scanAtMs_ = 0;
    bool scanStarted_ = false;
    DNSServer dns_;
    WiFiServer server_{80};
    WiFiClient client_;
    char request_[AtlasConfig::PortalRequestBytes + 1] = {};
    size_t requestLength_ = 0;
    std::atomic<bool> linkUp_{false};
    std::atomic<bool> localWifiMode_{false};
    std::atomic<uint32_t> generation_{0};
    std::atomic<uint32_t> configurationRequest_{0}, configurationApproved_{0}, configurationCompleted_{0};
    Configuration pendingConfiguration_ = Configuration::None;
    Profile pendingProfile_{};
    uint8_t pendingSlot_ = 0;
    bool configurationSucceeded_ = false;
};
} // namespace Atlas
