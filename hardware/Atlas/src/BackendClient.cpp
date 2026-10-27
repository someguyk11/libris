#include "BackendClient.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <sys/time.h>
#include <ctype.h>
#include <esp_random.h>

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

bool BackendClient::request(const char* endpoint, const char* payload, bool raw) {
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
        (raw || (responseDocument_["status"].is<int>() && responseDocument_["status"].as<int>() == 200));
}

bool BackendClient::poll(Command& command) {
    const uint32_t started = millis();
    if (!bootId_[0]) {
        uint8_t bytes[16];
        esp_fill_random(bytes, sizeof(bytes));
        for (uint8_t i = 0; i < sizeof(bytes); ++i) snprintf(bootId_ + i * 2, 3, "%02x", bytes[i]);
    }
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "/command?boot_id=%s%s%s", bootId_,
        completedRecovery_[0] ? "&recovery_completed=" : "", completedRecovery_);
    if (!request(endpoint, nullptr)) return false;
    const JsonObjectConst data = responseDocument_["data"].as<JsonObjectConst>();
    if (data.isNull() || !data["command"].is<const char*>() || !data["sequence"].is<uint32_t>() ||
        !data["execute"].is<bool>() || !data["factory_reset"].is<bool>() ||
        !data.containsKey("new_network") || !data.containsKey("command_id") ||
        !data.containsKey("recording_id") || !data.containsKey("recording_sequence")) return false;
    Command c;
    // Older backends remain usable for manual commands, but expose no recovery controls.
    if (data.containsKey("auto_reconnect")) {
        if (!data["auto_reconnect"].is<bool>()) return false;
        c.autoReconnect = data["auto_reconnect"];
    }
    if (!data["recovery"].isNull()) {
        const auto recovery = data["recovery"].as<JsonObjectConst>();
        const char* id = recovery["request_id"].as<const char*>();
        const char* action = recovery["action"].as<const char*>();
        if (!uuid(id) || !action) return false;
        if (!strcmp(action, "restart")) c.recovery = Command::Recovery::Restart;
        else if (!strcmp(action, "reconnect")) c.recovery = Command::Recovery::Reconnect;
        else if (!strcmp(action, "auto_reconnect") && recovery["enabled"].is<bool>()) {
            c.recovery = Command::Recovery::AutoReconnect;
            c.recoveryEnabled = recovery["enabled"];
        } else return false;
        strlcpy(c.recoveryId, id, sizeof(c.recoveryId));
    }
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

bool BackendClient::pollGuidance(Command& command, bool stopped) {
    const uint32_t started = millis();
    if (!request(stopped ? "/guidance/command?protocol=3&stopped=true" :
        "/guidance/command?protocol=3&stopped=false", nullptr, true)) return false;
    const auto data = responseDocument_.as<JsonObjectConst>();
    if (!data["protocol"].is<int>() || data["protocol"].as<int>() != 3 ||
        !data["command"].is<const char*>() || !data["execute"].is<bool>()) return false;
    Command c; c.guidance = true; c.fetchedAtMs = started;
    const char* name = data["command"];
    if (!strcmp(name, "idle")) {
        if (data["execute"].as<bool>()) return false;
        if (data["awaiting_confirmation"] == true && data["operator_attested"] == true && stopped) {
            JsonDocument arrival;
            for (const char* key : {"task_id", "session_id", "route_id", "checkpoint_id"}) {
                const char* id = data[key];
                if (!uuid(id)) return false;
                arrival[key] = id;
            }
            arrival["stopped"] = true; arrival["confirmation_method"] = "operator_attestation";
            if (!postDocument("/guidance/arrival", arrival, true) || responseDocument_["acknowledged"] != true) return false;
        }
    } else if (!strcmp(name, "replay_segment")) {
        const char* id = data["command_id"], *task = data["task_id"], *session = data["session_id"], *route = data["route_id"];
        if (!uuid(id) || !uuid(task) || !uuid(session) || !uuid(route) ||
            !data["sequence"].is<uint32_t>() || !data["sequence"].as<uint32_t>() ||
            !data["lease_ms"].is<uint32_t>() || data["lease_ms"].as<uint32_t>() != 1500 ||
            !data["segment_timeout_ms"].is<uint32_t>() || data["segment_timeout_ms"].as<uint32_t>() != 60000) return false;
        strlcpy(c.id, id, sizeof(c.id)); strlcpy(c.taskId, task, sizeof(c.taskId));
        strlcpy(c.sessionId, session, sizeof(c.sessionId)); strlcpy(c.routeId, route, sizeof(c.routeId));
        c.sequence = data["sequence"]; c.execute = data["execute"];
        const char* kind = data["motion_kind"], *status = data["execution_status"];
        if (!kind || !status) return false;
        if (!strcmp(kind, "forward")) c.kind = CommandKind::Forward;
        else if (!strcmp(kind, "backward")) c.kind = CommandKind::Backward;
        else if (!strcmp(kind, "left")) c.kind = CommandKind::Left;
        else if (!strcmp(kind, "right")) c.kind = CommandKind::Right;
        else if (strcmp(kind, "stop")) return false;
        const bool turn = c.kind == CommandKind::Left || c.kind == CommandKind::Right;
        if (turn ? (!data["turn_degrees"].is<int>() || data["turn_degrees"].as<int>() != 90) : !data["turn_degrees"].isNull()) return false;
        const bool offered = !strcmp(status, "offered");
        c.serverPaused = !strcmp(status, "paused");
        if ((!offered && !c.serverPaused && strcmp(status, "accepted") && strcmp(status, "started")) || c.execute != offered) return false;
        const auto m = data["movement"].as<JsonObjectConst>();
        if (!m["left_encoder_ticks"].is<uint32_t>() || !m["right_encoder_ticks"].is<uint32_t>()) return false;
        c.leftTicks = m["left_encoder_ticks"]; c.rightTicks = m["right_encoder_ticks"];
        const char* left = m["left_direction"], *right = m["right_direction"];
        const char* expectedLeft = c.kind == CommandKind::Stop ? "stop" :
            (c.kind == CommandKind::Backward || c.kind == CommandKind::Left ? "backward" : "forward");
        const char* expectedRight = c.kind == CommandKind::Stop ? "stop" :
            (c.kind == CommandKind::Backward || c.kind == CommandKind::Right ? "backward" : "forward");
        if (!left || !right || strcmp(left, expectedLeft) || strcmp(right, expectedRight) ||
            (c.kind == CommandKind::Stop ? (c.leftTicks || c.rightTicks) : (!c.leftTicks || !c.rightTicks))) return false;
        if (!data["control"].isNull()) {
            const auto ctrl = data["control"].as<JsonObjectConst>();
            const char* controlId = ctrl["request_id"], *action = ctrl["action"];
            if (!uuid(controlId) || !action) return false;
            if (!strcmp(action, "pause")) c.control = Command::Control::Pause;
            else if (!strcmp(action, "resume")) c.control = Command::Control::Resume;
            else return false;
            strlcpy(c.controlId, controlId, sizeof(c.controlId));
        }
    } else return false;
    command = c;
    return true;
}

void BackendClient::addSensors(JsonObject target, const Sensors& s) {
    static const char* names[] = {"front_cm", "back_cm", "left_cm", "right_cm"};
    for (uint8_t i = 0; i < AtlasConfig::SensorCount; ++i)
        if ((s.validMask & (1U << i)) && isfinite(s.cm[i]) && s.cm[i] >= 0) target[names[i]] = s.cm[i];
    // Invalid/stale values are omitted, not represented as zero or invented flags.
}

bool BackendClient::postDocument(const char* endpoint, JsonDocument& doc, bool raw) {
    if (doc.overflowed() || measureJson(doc) > AtlasConfig::MaximumHttpRequestBytes) return false;
    serializeJson(doc, payload_, sizeof(payload_));
    return request(endpoint, payload_, raw);
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
    if (!r.guidance) doc["captured_at"] = at;
    else {
        if (!uuid(r.taskId) || !uuid(r.sessionId) || !uuid(r.routeId)) return false;
        doc["task_id"] = r.taskId; doc["session_id"] = r.sessionId; doc["route_id"] = r.routeId;
        if (r.status == ResultStatus::Paused || r.status == ResultStatus::Resumed) {
            if (!uuid(r.controlId)) return false;
            status = r.status == ResultStatus::Paused ? "paused" : "resumed";
            doc["status"] = status; doc["control_request_id"] = r.controlId;
        }
    }
    if (r.measured) {
        auto m = doc["movement"].to<JsonObject>();
        m["left_encoder_ticks"] = r.leftTicks; m["right_encoder_ticks"] = r.rightTicks;
        m["left_direction"] = direction(r.leftDirection); m["right_direction"] = direction(r.rightDirection);
    }
    if (!r.guidance && r.sensors.validMask) addSensors(doc["sensor_readings"].to<JsonObject>(), r.sensors);
    if (r.error[0]) doc["error"] = r.error;
    if (r.guidance) {
        if (!postDocument("/guidance/result", doc, true) || !responseDocument_["acknowledged"].is<bool>() ||
            !responseDocument_["acknowledged"].as<bool>()) return false;
        if (r.status == ResultStatus::Paused || r.status == ResultStatus::Resumed) {
            const char* id = responseDocument_["control_request_id"];
            const char* echoed = responseDocument_["status"];
            return id && !strcmp(id, r.controlId) && echoed && !strcmp(echoed, status);
        }
        return true;
    }
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

bool BackendClient::acknowledgeRecovery(const Command& command) {
    JsonDocument doc;
    doc["request_id"] = command.recoveryId;
    doc["boot_id"] = bootId_;
    if (!postDocument("/recovery/ack", doc)) return false;
    const auto data = responseDocument_["data"].as<JsonObjectConst>();
    const char* id = data["request_id"].as<const char*>();
    return id && !strcmp(id, command.recoveryId) && data["acknowledged"].is<bool>() && data["acknowledged"].as<bool>();
}
