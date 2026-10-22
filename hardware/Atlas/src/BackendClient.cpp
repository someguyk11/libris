#include "BackendClient.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <sys/time.h>
#include <ctype.h>

namespace {
bool uuid(const char* value) {
    if (!value || strnlen(value, 37) != 36) return false;
    for (uint8_t i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (value[i] != '-') return false; }
        else if (!isxdigit(static_cast<unsigned char>(value[i]))) return false;
    }
    return true;
}
const char* direction(BackendClient::Direction d) {
    switch (d) {
        case BackendClient::Direction::Forward: return "forward";
        case BackendClient::Direction::Backward: return "backward";
        default: return "stop";
    }
}
}

bool BackendClient::configured() {
    if (strncmp(AtlasConfig::BackendBaseUrl, "https://", 8) != 0 ||
        strlen(AtlasConfig::DeviceKey) < 32 || strlen(AtlasConfig::DeviceKey) > 512 ||
        !strstr(AtlasConfig::TlsCa, "-----BEGIN CERTIFICATE-----")) return false;
    const size_t n = strlen(AtlasConfig::RobotId);
    if (!n || n > 80) return false;
    for (size_t i = 0; i < n; ++i) if (!isalnum(static_cast<unsigned char>(AtlasConfig::RobotId[i])) && AtlasConfig::RobotId[i] != '-') return false;
    // Prevent injected HTTP headers from private provisioning mistakes.
    for (const char* p = AtlasConfig::DeviceKey; *p; ++p) if (*p < 33 || *p > 126) return false;
    return true;
}

int64_t BackendClient::utcNowUs() {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    if (tv.tv_sec < AtlasConfig::MinimumUtcEpoch) return 0;
    return static_cast<int64_t>(tv.tv_sec) * 1000000LL + tv.tv_usec;
}

bool BackendClient::timestamp(int64_t epochUs, char* out, size_t size) {
    if (epochUs / 1000000 < AtlasConfig::MinimumUtcEpoch) return false;
    time_t seconds = epochUs / 1000000;
    tm utc{};
    if (!gmtime_r(&seconds, &utc)) return false;
    char date[24];
    if (!strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &utc)) return false;
    return snprintf(out, size, "%s.%06luZ", date, static_cast<unsigned long>(epochUs % 1000000)) < static_cast<int>(size);
}

bool BackendClient::request(const char* endpoint, const char* payload) {
    lastHttpStatus_ = 0;
    responseDocument_.clear();
    if (!configured() || !utcNowUs()) return false; // certificate validation needs real time.
    WiFiClientSecure tls;
    tls.setCACert(AtlasConfig::TlsCa);
    tls.setHandshakeTimeout(AtlasConfig::TlsHandshakeTimeoutSeconds);
    HTTPClient http;
    String url;
    url.reserve(strlen(AtlasConfig::BackendBaseUrl) + strlen(AtlasConfig::RobotId) + strlen(endpoint) + 16);
    url = AtlasConfig::BackendBaseUrl;
    if (url.endsWith("/")) url.remove(url.length() - 1);
    url += "/robot/"; url += AtlasConfig::RobotId; url += endpoint;
    if (!http.begin(tls, url)) return false;
    http.setConnectTimeout(AtlasConfig::HttpConnectTimeoutMs);
    http.setTimeout(AtlasConfig::HttpReadTimeoutMs);
    http.setReuse(false);
    http.useHTTP10(true); // require finite Content-Length; no unbounded chunked decoding.
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    const char* headers[] = {"Content-Type", "Transfer-Encoding"};
    http.collectHeaders(headers, 2);
    http.addHeader("X-Atlas-Key", AtlasConfig::DeviceKey);
    if (payload) http.addHeader("Content-Type", "application/json");
    lastHttpStatus_ = payload ? http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(payload)), strlen(payload)) : http.GET();
    const int size = http.getSize();
    bool ok = lastHttpStatus_ == 200 && size > 0 && size <= static_cast<int>(AtlasConfig::MaximumHttpResponseBytes) &&
        http.header("Content-Type").startsWith("application/json") && http.header("Transfer-Encoding").isEmpty();
    size_t received = 0;
    const uint32_t started = millis();
    auto* stream = http.getStreamPtr();
    while (ok && received < static_cast<size_t>(size)) {
        if (millis() - started >= AtlasConfig::HttpBodyDeadlineMs) { ok = false; break; }
        const int available = stream->available();
        if (available > 0) {
            const int count = stream->read(reinterpret_cast<uint8_t*>(response_ + received), min(static_cast<size_t>(available), static_cast<size_t>(size) - received));
            if (count <= 0) { ok = false; break; }
            received += count;
        } else if (!stream->connected()) { ok = false; break; }
        else vTaskDelay(1); // at least one tick, only on the network worker.
    }
    http.end();
    if (!ok) return false;
    response_[received] = 0;
    const auto error = deserializeJson(responseDocument_, response_, received, DeserializationOption::NestingLimit(8));
    return !error && !responseDocument_.overflowed() && responseDocument_.is<JsonObject>() &&
        responseDocument_["status"].is<int>() && responseDocument_["status"].as<int>() == 200;
}

bool BackendClient::poll(Command& command) {
    const uint32_t started = millis();
    if (!request("/command", nullptr)) return false;
    const JsonObjectConst data = responseDocument_["data"].as<JsonObjectConst>();
    if (data.isNull() || !data["command"].is<const char*>() || !data["sequence"].is<uint32_t>() ||
        !data["execute"].is<bool>() || !data["factory_reset"].is<bool>() ||
        !data.containsKey("new_network") || !data.containsKey("command_id") ||
        !data.containsKey("recording_id") || !data.containsKey("recording_sequence")) return false;
    Command c;
    const char* name = data["command"];
    if (!strcmp(name, "stop")) c.kind = CommandKind::Stop;
    else if (!strcmp(name, "forward")) c.kind = CommandKind::Forward;
    else if (!strcmp(name, "backward")) c.kind = CommandKind::Backward;
    else if (!strcmp(name, "left") || !strcmp(name, "left_turn")) c.kind = CommandKind::Left;
    else if (!strcmp(name, "right") || !strcmp(name, "right_turn")) c.kind = CommandKind::Right;
    else return false;
    c.transportSequence = data["sequence"];
    c.sequence = c.transportSequence;
    c.execute = data["execute"];
    c.factoryReset = data["factory_reset"];
    c.fetchedAtMs = started;
    if (!data["command_id"].isNull()) {
        const char* id = data["command_id"].as<const char*>();
        const char* status = data["execution_status"].as<const char*>();
        if (!uuid(id) || !c.sequence || !status) return false;
        const bool delivered = !strcmp(status, "delivered"), active = !strcmp(status, "started");
        const bool terminal = !strcmp(status, "completed") || !strcmp(status, "failed");
        if ((!delivered && !active && !terminal) || c.execute != delivered || (terminal && c.kind != CommandKind::Stop)) return false;
        strlcpy(c.id, id, sizeof(c.id));
    } else if (c.kind != CommandKind::Stop || c.execute) return false;
    if (!data["recording_id"].isNull()) {
        const char* id = data["recording_id"].as<const char*>();
        if (!uuid(id)) return false;
        strlcpy(c.recordingId, id, sizeof(c.recordingId));
        if (c.id[0]) {
            if (!data["recording_sequence"].is<uint32_t>() || data["recording_sequence"].as<uint32_t>() == 0) return false;
            c.sequence = data["recording_sequence"];
        }
    } else if (!data["recording_sequence"].isNull()) return false;
    if (!data["new_network"].isNull()) {
        const auto network = data["new_network"].as<JsonObjectConst>();
        const char* ssid = network["ssid"].as<const char*>();
        const char* password = network["password"].as<const char*>();
        if (!ssid || !password || !strlen(ssid) || strlen(ssid) >= sizeof(c.ssid) || strlen(password) >= sizeof(c.password) || (strlen(password) > 0 && strlen(password) < 8)) return false;
        strlcpy(c.ssid, ssid, sizeof(c.ssid)); strlcpy(c.password, password, sizeof(c.password)); c.reconfigure = true;
    }
    command = c;
    return true;
}

void BackendClient::addSensors(JsonObject target, const Sensors& s) {
    static const char* names[] = {"front_cm", "back_cm", "left_cm", "right_cm"};
    for (uint8_t i = 0; i < AtlasConfig::SensorCount; ++i)
        if ((s.validMask & (1U << i)) && isfinite(s.cm[i]) && s.cm[i] >= 0) target[names[i]] = s.cm[i];
    // Invalid/stale values are omitted, not represented as zero or invented flags.
}

bool BackendClient::postDocument(const char* endpoint, JsonDocument& doc) {
    if (doc.overflowed() || measureJson(doc) > AtlasConfig::MaximumHttpRequestBytes) return false;
    serializeJson(doc, payload_, sizeof(payload_));
    return request(endpoint, payload_);
}

bool BackendClient::sendResult(const Result& r) {
    char at[36];
    if (!uuid(r.id) || !r.sequence || !timestamp(r.capturedAtUs, at, sizeof(at)) ||
        (r.status == ResultStatus::Completed && !r.measured)) return false;
    JsonDocument doc;
    doc["command_id"] = r.id;
    if (r.recordingId[0]) doc["recording_id"] = r.recordingId;
    doc["sequence"] = r.sequence;
    const char* status = r.status == ResultStatus::Started ? "started" : r.status == ResultStatus::Completed ? "completed" : "failed";
    doc["status"] = status;
    doc["captured_at"] = at;
    if (r.measured) {
        auto m = doc["movement"].to<JsonObject>();
        m["left_encoder_ticks"] = r.leftTicks; m["right_encoder_ticks"] = r.rightTicks;
        m["left_direction"] = direction(r.leftDirection); m["right_direction"] = direction(r.rightDirection);
    }
    if (r.sensors.validMask) addSensors(doc["sensor_readings"].to<JsonObject>(), r.sensors);
    if (r.error[0]) doc["error"] = r.error;
    if (!postDocument("/command/result", doc)) return false;
    const auto data = responseDocument_["data"].as<JsonObjectConst>();
    const char* id = data["command_id"].as<const char*>();
    const char* acknowledgedStatus = data["execution_status"].as<const char*>();
    return id && !strcmp(id, r.id) && acknowledgedStatus && !strcmp(acknowledgedStatus, status) &&
        data["acknowledged"].is<bool>() && data["acknowledged"].as<bool>();
}

bool BackendClient::sendSensors(const SensorSample& s) {
    char at[36];
    if (!uuid(s.id) || !uuid(s.recordingId) || !s.sequence || !s.sensors.validMask || !timestamp(s.sensors.capturedAtUs, at, sizeof(at))) return false;
    JsonDocument doc;
    doc["command_id"] = s.id; doc["recording_id"] = s.recordingId; doc["sequence"] = s.sequence; doc["captured_at"] = at;
    addSensors(doc["sensor_readings"].to<JsonObject>(), s.sensors);
    return postDocument("/sensors", doc);
}

bool BackendClient::sendTelemetry(const char* ssid, int signalStrength, uint64_t uptimeSeconds) {
    JsonDocument doc;
    doc["wifi_ssid"] = ssid; doc["signal_strength"] = signalStrength; doc["uptime_seconds"] = uptimeSeconds; doc["firmware_version"] = AtlasConfig::FirmwareVersion;
    return postDocument("/telemetry", doc);
}
