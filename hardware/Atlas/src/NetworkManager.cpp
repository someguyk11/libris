#include "NetworkManager.h"
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "SetupPage.generated.h"

namespace {
bool validCredentials(const char* ssid, const char* password) {
    if (!ssid || !password) return false;
    const size_t s = strnlen(ssid, 32), p = strnlen(password, 64);
    return s > 0 && s < 32 && p < 64 && (p == 0 || p >= 8);
}

bool headerEquals(const char* line, const char* name, const char* value) {
    const size_t n = strlen(name);
    if (strncasecmp(line, name, n)) return false;
    line += n;
    while (*line == ' ' || *line == '\t') ++line;
    const char* end = strstr(line, "\r\n");
    if (!end) return false;
    while (end > line && (end[-1] == ' ' || end[-1] == '\t')) --end;
    return static_cast<size_t>(end - line) == strlen(value) && !strncasecmp(line, value, end - line);
}
}

namespace Atlas {

void NetworkManager::begin() {
    eepromReady_ = EEPROM.begin(AtlasConfig::WifiEepromBytes);
    WiFi.persistent(false); // Do not maintain a second, incompatible credential store.
    WiFi.setHostname(AtlasConfig::WifiHostname); // before mode/begin/softAP.
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t) {
        // Arduino invokes this on its event task. Never manipulate Wi-Fi or profiles here.
        if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) linkUp_.store(true);
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED || event == ARDUINO_EVENT_WIFI_STA_STOP) {
            linkUp_.store(false);
            generation_.fetch_add(1);
        }
    });
    WiFi.mode(WIFI_AP_STA);
    WiFi.setAutoReconnect(false); // bounded attempts/backoff are owned here.
    startPortal();
    configTime(0, 0, AtlasConfig::NtpServer); // async SNTP; no getLocalTime wait loop.
}

bool NetworkManager::load(uint8_t slot, Profile& p) {
    if (!eepromReady_ || slot >= AtlasConfig::WifiProfileCount) return false;
    EEPROM.get(slot * AtlasConfig::WifiProfileBytes, p);
    if (!memchr(p.ssid, 0, sizeof(p.ssid)) || !memchr(p.password, 0, sizeof(p.password))) return false;
    return validCredentials(p.ssid, p.password);
}

bool NetworkManager::save(const char* ssid, const char* password) {
    if (!eepromReady_ || !validCredentials(ssid, password)) return false;
    uint8_t chosen = 0;
    bool emptyFound = false;
    for (uint8_t i = 0; i < AtlasConfig::WifiProfileCount; ++i) {
        Profile p{};
        if (load(i, p)) {
            if (strcmp(p.ssid, ssid) == 0) { chosen = i; break; }
        } else if (!emptyFound) { chosen = i; emptyFound = true; }
    }
    Profile p{};
    strlcpy(p.ssid, ssid, sizeof(p.ssid));
    strlcpy(p.password, password, sizeof(p.password));
    EEPROM.put(chosen * AtlasConfig::WifiProfileBytes, p);
    EEPROM.write(AtlasConfig::LegacyOtaFlagAddress, 0);
    return EEPROM.commit();
}

bool NetworkManager::forget(uint8_t slot) {
    if (!eepromReady_ || slot >= AtlasConfig::WifiProfileCount) return false;
    return queueConfiguration(Configuration::Forget, Profile{}, slot);
}

void NetworkManager::startAttempt(const Profile& p, uint32_t nowMs) {
    autoConnectHeld_ = false;
    connectionFailed_ = false;
    strlcpy(attemptSsid_, p.ssid, sizeof(attemptSsid_));
    WiFi.disconnect(false, false);
    linkUp_.store(false);
    generation_.fetch_add(1);
    WiFi.begin(p.ssid, p.password);
    attempting_ = true;
    attemptAtMs_ = nowMs;
}

bool NetworkManager::reconfigure(const char* ssid, const char* password) {
    if (!eepromReady_ || !validCredentials(ssid, password)) return false;
    Profile p{};
    strlcpy(p.ssid, ssid, sizeof(p.ssid));
    strlcpy(p.password, password, sizeof(p.password));
    return queueConfiguration(Configuration::Save, p);
}

void NetworkManager::factoryReset() {
    queueConfiguration(Configuration::Reset, Profile{});
}

bool NetworkManager::queueConfiguration(Configuration action, const Profile& profile, uint8_t slot) {
    if (pendingConfiguration_ != Configuration::None) return false;
    pendingProfile_ = profile;
    pendingSlot_ = slot;
    pendingConfiguration_ = action;
    configurationRequest_.fetch_add(1); // requires a NEW main-loop acknowledgement.
    return true;
}

void NetworkManager::applyConfiguration() {
    if (pendingConfiguration_ == Configuration::None ||
        configurationApproved_.load() != configurationRequest_.load()) return;
    // Main loop has stopped navigation and will not dispatch until completion.
    configurationSucceeded_ = false;
    if (pendingConfiguration_ == Configuration::Save || pendingConfiguration_ == Configuration::SaveOnly) {
        configurationSucceeded_ = save(pendingProfile_.ssid, pendingProfile_.password);
        if (configurationSucceeded_ && pendingConfiguration_ == Configuration::Save) {
            startPortal();
            startAttempt(pendingProfile_, millis());
        }
        if (configurationSucceeded_ && pendingConfiguration_ == Configuration::SaveOnly) autoConnectHeld_ = true;
    } else if (pendingConfiguration_ == Configuration::Forget) {
        Profile empty{};
        EEPROM.put(pendingSlot_ * AtlasConfig::WifiProfileBytes, empty);
        configurationSucceeded_ = EEPROM.commit();
    } else {
        if (eepromReady_) {
            for (uint16_t i = 0; i < AtlasConfig::WifiEepromBytes; ++i) EEPROM.write(i, 0);
            configurationSucceeded_ = EEPROM.commit();
        }
        // No reboot, key deletion or implicit safety reset. Clear Wi-Fi storage only.
        WiFi.disconnect(false, true);
        linkUp_.store(false);
        generation_.fetch_add(1);
        attempting_ = false;
        autoConnectHeld_ = false;
        nextSlot_ = 0;
        startPortal();
    }
    memset(&pendingProfile_, 0, sizeof(pendingProfile_));
    pendingConfiguration_ = Configuration::None;
    configurationCompleted_.store(configurationRequest_.load());
}

void NetworkManager::startPortal() {
    if (apActive_) return;
    const IPAddress ip(AtlasConfig::SetupIp);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
    apActive_ = WiFi.softAP(AtlasConfig::SetupSsid);
    if (apActive_) {
        dns_.start(53, "*", ip);
        server_.begin();
    }
}

void NetworkManager::update(uint32_t nowMs) {
    applyConfiguration();
    const bool up = connected();
    if (up) {
        attempting_ = false;
        backoffMs_ = AtlasConfig::WifiRetryInitialMs;
        if (!wasConnected_) connectedAtMs_ = nowMs;
        if (apActive_ && nowMs - connectedAtMs_ >= AtlasConfig::PortalCloseGraceMs) {
            closeClient(); server_.end(); dns_.stop(); WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            apActive_ = false;
        }
    } else {
        startPortal();
        if (attempting_ && nowMs - attemptAtMs_ >= AtlasConfig::WifiAttemptTimeoutMs) {
            WiFi.disconnect(false, false);
            attempting_ = false;
            connectionFailed_ = true;
            retryAtMs_ = nowMs + backoffMs_;
            backoffMs_ = min(backoffMs_ * 2, AtlasConfig::WifiRetryMaximumMs);
        }
        if (autoReconnect_ && !attempting_ && !autoConnectHeld_ && static_cast<int32_t>(nowMs - retryAtMs_) >= 0) {
            bool found = false;
            for (uint8_t i = 0; i < AtlasConfig::WifiProfileCount; ++i) {
                Profile p{};
                const uint8_t slot = nextSlot_++ % AtlasConfig::WifiProfileCount;
                if (load(slot, p)) { startAttempt(p, nowMs); found = true; break; }
            }
            if (!found) retryAtMs_ = nowMs + AtlasConfig::WifiRetryMaximumMs;
        }
    }
    wasConnected_ = up;
    if (apActive_) { dns_.processNextRequest(); portal(nowMs); }
}

void NetworkManager::reconnect() {
    WiFi.disconnect(false, false); // preserve saved credentials.
    linkUp_.store(false);
    generation_.fetch_add(1);
    attempting_ = false;
    autoConnectHeld_ = false;
    retryAtMs_ = millis();
    backoffMs_ = AtlasConfig::WifiRetryInitialMs;
    // Explicit reconnect makes one attempt even when automatic retries are disabled.
    for (uint8_t i = 0; i < AtlasConfig::WifiProfileCount; ++i) {
        Profile profile{};
        if (load(i, profile)) { startAttempt(profile, millis()); break; }
    }
}

void NetworkManager::closeClient() { client_.stop(); requestLength_ = 0; }

void NetworkManager::reply(int code, const char* type, const char* body, size_t length, bool gzip) {
    if (!length) length = strlen(body);
    char header[256];
    snprintf(header, sizeof(header), "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n%sConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n\r\n",
        code, code == 200 ? "OK" : "Error", type, static_cast<unsigned>(length), gzip ? "Content-Encoding: gzip\r\n" : "");
    client_.write(reinterpret_cast<const uint8_t*>(header), strlen(header));
    client_.write(reinterpret_cast<const uint8_t*>(body), length);
    closeClient();
}

void NetworkManager::portal(uint32_t nowMs) {
    if (!client_) {
        client_ = server_.accept();
        if (!client_) return;
        // Portal is local to the open setup AP; never expose it on the STA LAN.
        const IPAddress remote = client_.remoteIP();
        if (client_.localIP() != IPAddress(AtlasConfig::SetupIp) ||
            remote[0] != AtlasConfig::SetupIp[0] || remote[1] != AtlasConfig::SetupIp[1] || remote[2] != AtlasConfig::SetupIp[2]) { closeClient(); return; }
        client_.setTimeout(AtlasConfig::PortalWriteTimeoutMs);
        clientAtMs_ = nowMs;
        requestLength_ = 0;
    }
    if (nowMs - clientAtMs_ >= AtlasConfig::PortalClientTimeoutMs) { closeClient(); return; }
    // One bounded read per worker tick; no wait for headers/body.
    const size_t count = min(static_cast<size_t>(client_.available()), AtlasConfig::PortalRequestBytes - requestLength_);
    if (count) {
        const int n = client_.read(reinterpret_cast<uint8_t*>(request_ + requestLength_), count);
        if (n > 0) requestLength_ += n;
    }
    request_[requestLength_] = 0;
    char* end = strstr(request_, "\r\n\r\n");
    if (!end) { if (requestLength_ == AtlasConfig::PortalRequestBytes) reply(413, "application/json", "{\"error\":\"Request too large\"}"); return; }
    size_t length = 0;
    // Only Content-Length bodies. Reject ambiguous/framing variants and chunking.
    char* line = strstr(request_, "\r\n");
    bool seenLength = false;
    while (line && line < end) {
        line += 2;
        if (strncasecmp(line, "Transfer-Encoding:", 18) == 0) { reply(400, "application/json", "{\"error\":\"Unsupported framing\"}"); return; }
        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            char* tail = nullptr;
            length = strtoul(line + 15, &tail, 10);
            if (seenLength || tail == line + 15 || strncmp(tail, "\r\n", 2) != 0 || length > AtlasConfig::PortalMaximumBodyBytes) { reply(413, "application/json", "{\"error\":\"Invalid body length\"}"); return; }
            seenLength = true;
        }
        line = strstr(line, "\r\n");
    }
    const size_t headerBytes = end + 4 - request_;
    if (requestLength_ >= headerBytes + length) {
        request_[headerBytes + length] = 0; // never parse pipelined bytes as JSON.
        handleRequest();
    }
    else if (requestLength_ == AtlasConfig::PortalRequestBytes) reply(413, "application/json", "{}");
}

void NetworkManager::handleRequest() {
    char method[8]{}, path[64]{};
    if (sscanf(request_, "%7s %63s", method, path) != 2) { reply(400, "application/json", "{}"); return; }
    JsonDocument doc;
    char body[AtlasConfig::PortalResponseBytes];
    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/status") == 0) {
            doc["connected"] = connected(); doc["wifiConfigured"] = false;
            for (uint8_t i = 0; i < AtlasConfig::WifiProfileCount; ++i) { Profile p{}; if (load(i, p)) doc["wifiConfigured"] = true; }
            doc["apModeActive"] = apActive_; doc["currentSsid"] = WiFi.SSID();
            doc["ipAddress"] = WiFi.localIP().toString(); doc["signalStrength"] = WiFi.RSSI();
            doc["connecting"] = attempting_; doc["connectionFailed"] = connectionFailed_; doc["attemptSsid"] = attemptSsid_;
            doc["operation"] = configurationRequest_.load();
            doc["operationStatus"] = configurationPending() ? "pending" : (configurationSucceeded_ ? "complete" : "failed");
        } else if (strcmp(path, "/history") == 0) {
            auto a = doc.to<JsonArray>();
            for (uint8_t i = 0; i < AtlasConfig::WifiProfileCount; ++i) { Profile p{}; if (load(i, p)) { auto o = a.add<JsonObject>(); o["slot"] = i; o["ssid"] = p.ssid; } }
        } else if (strcmp(path, "/scan") == 0) {
            const int n = WiFi.scanComplete();
            if (n >= 0) {
                doc["scanning"] = false;
                auto a = doc["networks"].to<JsonArray>();
                for (int i = 0; i < min(n, static_cast<int>(AtlasConfig::PortalMaximumScanResults)); ++i) { auto o = a.add<JsonObject>(); o["ssid"] = WiFi.SSID(i); o["rssi"] = WiFi.RSSI(i); }
                // Retain bounded scan results until the next explicitly requested scan.
                if (millis() - scanAtMs_ >= AtlasConfig::PortalScanIntervalMs) { WiFi.scanDelete(); scanStarted_ = false; }
            } else {
                if (n != WIFI_SCAN_RUNNING && scanStarted_) {
                    doc["scanning"] = false; doc["error"] = "Could not scan. Wait for any connection attempt to finish and try again.";
                    scanStarted_ = false;
                } else {
                    doc["scanning"] = true;
                    if (n != WIFI_SCAN_RUNNING) { WiFi.scanNetworks(true); scanStarted_ = true; scanAtMs_ = millis(); }
                }
            }
        } else { reply(200, "text/html; charset=utf-8", reinterpret_cast<const char*>(SetupPageGzip), sizeof(SetupPageGzip), true); return; }
    } else if (strcmp(method, "POST") == 0) {
        // Require JSON + the AP host, preventing form CSRF and DNS rebinding.
        // No CORS/preflight support. Non-browser AP clients may omit Origin.
        const String address = IPAddress(AtlasConfig::SetupIp).toString();
        const String hostWithPort = address + ":80";
        const String origin = "http://" + address;
        const String originWithPort = origin + ":80";
        bool host = false, type = false, originSeen = false, allowed = true;
        const char* end = strstr(request_, "\r\n\r\n");
        const char* line = strstr(request_, "\r\n");
        while (line && line < end) {
            line += 2;
            if (!strncasecmp(line, "Host:", 5)) {
                allowed &= !host && (headerEquals(line, "Host:", address.c_str()) || headerEquals(line, "Host:", hostWithPort.c_str()));
                host = true;
            } else if (!strncasecmp(line, "Content-Type:", 13)) {
                allowed &= !type && headerEquals(line, "Content-Type:", "application/json"); type = true;
            } else if (!strncasecmp(line, "Origin:", 7)) {
                allowed &= !originSeen && (headerEquals(line, "Origin:", origin.c_str()) || headerEquals(line, "Origin:", originWithPort.c_str()));
                originSeen = true;
            }
            line = strstr(line, "\r\n");
        }
        if (!allowed || !host || !type) { reply(403, "application/json", "{\"error\":\"Use the setup portal at its IP address\"}"); return; }
        char* payload = strstr(request_, "\r\n\r\n") + 4;
        if (deserializeJson(doc, payload, DeserializationOption::NestingLimit(3)) || !doc.is<JsonObject>()) { reply(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
        if (configurationPending()) { reply(409, "application/json", "{\"error\":\"A network change is still being saved. Try again shortly.\"}"); return; }
        bool ok = false;
        if (strcmp(path, "/forget") == 0 && doc["slot"].is<unsigned>() && doc["slot"].as<unsigned>() < AtlasConfig::WifiProfileCount) {
            ok = forget(doc["slot"].as<uint8_t>());
        } else if (strcmp(path, "/save") == 0 && doc["ssid"].is<const char*>() && doc["pass"].is<const char*>()) {
            Profile p{};
            if (eepromReady_ && validCredentials(doc["ssid"], doc["pass"])) {
                strlcpy(p.ssid, doc["ssid"], sizeof(p.ssid)); strlcpy(p.password, doc["pass"], sizeof(p.password));
                ok = queueConfiguration(Configuration::SaveOnly, p);
            }
        } else if (strcmp(path, "/connect") == 0) {
            if (doc["slot"].is<unsigned>() && doc["slot"].as<unsigned>() < AtlasConfig::WifiProfileCount) {
                Profile p{};
                if (load(doc["slot"].as<uint8_t>(), p)) { startAttempt(p, millis()); ok = true; }
            } else if (doc["ssid"].is<const char*>() && doc["pass"].is<const char*>()) {
                ok = reconfigure(doc["ssid"], doc["pass"]);
            }
        }
        if (!ok) { reply(400, "application/json", "{\"error\":\"Check the network name and password, or try saving again.\"}"); return; }
        doc.clear(); doc["ok"] = true; doc["operation"] = configurationRequest_.load();
        doc["pending"] = configurationPending();
    } else { reply(405, "application/json", "{}"); return; }
    if (measureJson(doc) >= sizeof(body)) { reply(500, "application/json", "{}"); return; }
    serializeJson(doc, body, sizeof(body)); reply(200, "application/json", body);
}
} // namespace Atlas
