#pragma once

#include "AtlasConfig.h"
#include <ArduinoJson.h>

// Minimal Supabase REST client for command polling (simulation / alternative
// to the render.com backend poll).
//
// GET  /rest/v1/robot_commands?select=*&robot_id=eq.{id}&status=eq.pending&order=created_at.asc&limit=1
// PATCH /rest/v1/robot_commands?id=eq.{id}   { "status": "executed", "executed_at": "..." }
//
// Required headers: apikey + Authorization: Bearer <anon_key>
// No new dependencies: reuses HTTPClient + WiFiClientSecure already used by BackendClient.
class SupabaseClient {
public:
    enum class Error : uint8_t { None, NotConfigured, Network, Parse, Http };

    struct CommandRow {
        char id[37] = {};
        char kind[16] = {};    // forward/backward/left/right/replay/stop
        float paramCm = 0.0f;  // distance (cm) for forward/backward
        int32_t ticksLeft = 0; // left encoder ticks for replay
        int32_t ticksRight = 0; // right encoder ticks for replay
        uint32_t fetchedAtMs = 0;
    };

    bool configured();
    // Returns true when a pending command row was read into row.
    bool poll(CommandRow& row);
    // PATCH status to "executed" or "failed" for the given command id.
    bool markStatus(const char* commandId, const char* status);
    Error lastError() const { return lastError_; }

private:
    bool request(const char* method, const char* endpoint, const char* payload,
                 char* response, size_t responseSize);
    Error lastError_ = Error::None;
};
