#include "SupabaseClient.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ctype.h>

using namespace AtlasConfig;

bool SupabaseClient::configured() {
    if (strncmp(SupabaseBaseUrl, "https://", 8) != 0) return false;
    const size_t n = strlen(SupabaseAnonKey);
    if (!n || n > 512) return false;
    for (size_t i = 0; i < n; ++i) if (SupabaseAnonKey[i] < 33 || SupabaseAnonKey[i] > 126) return false;
    const size_t rid = strlen(SupabaseRobotId);
    if (!rid || rid > 80) return false;
    for (size_t i = 0; i < rid; ++i)
        if (!isalnum(static_cast<unsigned char>(SupabaseRobotId[i])) && SupabaseRobotId[i] != '-') return false;
    return true;
}

bool SupabaseClient::request(const char* method, const char* endpoint, const char* payload,
                             char* response, size_t responseSize) {
    lastError_ = Error::None;
    if (!configured()) { lastError_ = Error::NotConfigured; return false; }

    WiFiClientSecure tls;
    tls.setCACert(AtlasConfig::TlsCa);
    tls.setHandshakeTimeout(AtlasConfig::TlsHandshakeTimeoutSeconds);
    HTTPClient http;
    String url = SupabaseBaseUrl;
    if (url.endsWith("/")) url.remove(url.length() - 1);
    url += endpoint;
    if (!http.begin(tls, url)) { lastError_ = Error::Network; return false; }
    http.setConnectTimeout(AtlasConfig::HttpConnectTimeoutMs);
    http.setTimeout(AtlasConfig::HttpReadTimeoutMs);
    http.setReuse(false);
    http.useHTTP10(true);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.addHeader("apikey", SupabaseAnonKey);
    http.addHeader("Authorization", String("Bearer ") + SupabaseAnonKey);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "return=representation");

    int code;
    if (!strcmp(method, "PATCH"))
        code = http.sendRequest("PATCH", reinterpret_cast<uint8_t*>(const_cast<char*>(payload)), payload ? strlen(payload) : 0);
    else if (payload)
        code = http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(payload)), strlen(payload));
    else
        code = http.GET();

    // HTTP status is intentionally not used for success/failure; Supabase returns
    // 200/201 on success and the body tells us whether rows exist. Body size and
    // stream read are the validity checks instead.
    (void)code;

    const int size = http.getSize();
    bool ok = size > 0 && size <= static_cast<int>(responseSize - 1);
    size_t received = 0;
    const uint32_t started = millis();
    auto* stream = http.getStreamPtr();
    while (ok && received < static_cast<size_t>(size)) {
        if (millis() - started >= AtlasConfig::HttpBodyDeadlineMs) { ok = false; break; }
        const int available = stream->available();
        if (available > 0) {
            const int count = stream->read(reinterpret_cast<uint8_t*>(response + received), min(size_t(available), responseSize - 1 - received));
            if (count <= 0) { ok = false; break; }
            received += count;
        } else if (!stream->connected()) { ok = false; break; }
        else vTaskDelay(1);
    }
    http.end();
    if (!ok) { lastError_ = Error::Http; return false; }
    response[received] = 0;
    return true;
}

bool SupabaseClient::poll(CommandRow& row) {
    lastError_ = Error::None;
    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint),
        "/rest/v1/robot_commands?select=id,kind,param_cm,param_ticks_left,param_ticks_right"
        "&robot_id=eq.%s&status=eq.pending&order=created_at.asc&limit=1", SupabaseRobotId);
    char response[AtlasConfig::MaximumHttpResponseBytes + 1] = {};
    if (!request("GET", endpoint, nullptr, response, sizeof(response))) return false;

    JsonDocument doc;
    const auto error = deserializeJson(doc, response, strlen(response), DeserializationOption::NestingLimit(8));
    if (error || !doc.is<JsonArray>() || doc.size() == 0) { lastError_ = Error::Parse; return false; }

    const auto rowObj = doc[0].as<JsonObjectConst>();
    const char* id = rowObj["id"] | "";
    const char* kind = rowObj["kind"] | "";
    if (!id[0] || !kind[0]) { lastError_ = Error::Parse; return false; }
    strlcpy(row.id, id, sizeof(row.id));
    strlcpy(row.kind, kind, sizeof(row.kind));
    row.paramCm = rowObj["param_cm"] | 0.0f;
    row.ticksLeft = rowObj["param_ticks_left"] | 0;
    row.ticksRight = rowObj["param_ticks_right"] | 0;
    row.fetchedAtMs = millis();
    return true;
}

bool SupabaseClient::markStatus(const char* commandId, const char* status) {
    lastError_ = Error::None;
    if (!commandId[0] || !status[0]) return false;

    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "/rest/v1/robot_commands?id=eq.%s", commandId);

    JsonDocument doc;
    doc["status"] = status;

    char payload[256];
    size_t n = measureJson(doc);
    if (n == 0 || n >= sizeof(payload)) { lastError_ = Error::Parse; return false; }
    serializeJson(doc, payload, sizeof(payload));

    char response[AtlasConfig::MaximumHttpResponseBytes + 1] = {};
    return request("PATCH", endpoint, payload, response, sizeof(response));
}
