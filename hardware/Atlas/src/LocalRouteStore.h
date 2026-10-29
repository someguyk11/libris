#pragma once

#include "Routes.h"
#include <ArduinoJson.h>

// NVS contains JSON strings, not browser-local routes. All runtime flash writes
// happen on the network worker, after the main loop has entered a stationary hold.
namespace LocalRouteStore {
constexpr uint8_t AutomaticSlot = 255;
struct Route {
    char name[41]{};
    uint8_t count = 0;
    Routes::Segment segments[AtlasConfig::LocalRouteMaxSegments];
};
struct Job {
    enum class Kind : uint8_t { Save, Load } kind;
    uint32_t id = 0;
    uint8_t slot = 0;
    Route route;
    bool ok = false;
};
bool begin(); // setup only, before any motor command
bool enqueue(const Job& job); // main-loop only; caller holds navigation stationary until result
bool receive(Job& job); // main-loop only
void workerUpdate();
void list(JsonDocument& doc); // worker only
bool content(uint8_t slot, JsonDocument& doc); // worker only
}
