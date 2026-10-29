#pragma once
#include <cstdint>
namespace hostNetwork { extern bool connected; }
struct HostSsid { const char* c_str() const { return "host-test"; } };
struct HostWifi { HostSsid SSID() const { return {}; } int RSSI() const { return -50; } };
inline HostWifi WiFi;
namespace Atlas {
class NetworkManager {
public:
    void begin() {} void update(uint32_t) {}
    bool connected() const { return hostNetwork::connected; }
    uint32_t generation() const { return 1; }
    bool configurationPending() const { return false; }
    void approveConfiguration() {} void reconnect() {} void setAutoReconnect(bool) {}
    void factoryReset() {} bool reconfigure(const char*,const char*) { return true; }
};
}
