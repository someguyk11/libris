#include "LocalRouteStore.h"
#include "LocalRoutePolicy.h"
#include "LocalRouteJson.h"
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace LocalRouteStore {
namespace {
Preferences prefs;
QueueHandle_t jobs = nullptr, results = nullptr;
bool available = false;
struct Entry { bool valid = false; char name[41]{}; uint8_t count = 0; uint32_t savedOrder = 0; } entries[AtlasConfig::LocalRouteSlots];

uint8_t automaticSlot() {
    return LocalRoutePolicy::chooseSlot(entries);
}

uint32_t nextOrder() {
    return LocalRoutePolicy::nextOrder(entries);
}

uint32_t calibration() {
    // Reject routes after encoder geometry/count convention or turn calibration changes.
    const float values[] = {AtlasConfig::WheelDiameterCm, AtlasConfig::LeftCountsPerWheelRevolution,
        AtlasConfig::RightCountsPerWheelRevolution, AtlasConfig::WheelTrackCm,
        AtlasConfig::ClockwiseTurnScale, AtlasConfig::CounterclockwiseTurnScale,
        static_cast<float>(AtlasConfig::CountEdges)};
    uint32_t hash = 2166136261U;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(values);
    for (size_t i = 0; i < sizeof(values); ++i) hash = (hash ^ bytes[i]) * 16777619U;
    return hash;
}
void key(uint8_t slot, char* out, size_t size) { snprintf(out, size, "route%u", slot); }

bool decode(JsonDocument& doc, Route& route) {
    return LocalRouteJson::decode(doc, route, calibration(), Routes::validSegment, AtlasConfig::EncoderOnlyGroundTest);
}
bool read(uint8_t slot, JsonDocument& doc) {
    if (!available || slot >= AtlasConfig::LocalRouteSlots) return false;
    char name[16]; key(slot, name, sizeof(name));
    const size_t size = prefs.getStringLength(name);
    if (size < 2 || size > AtlasConfig::LocalRouteJsonBytes) return false;
    // Static: network worker owns runtime reads; no 3 KB stack frame per call.
    static char json[AtlasConfig::LocalRouteJsonBytes];
    if (prefs.getString(name, json, sizeof(json)) != size) return false;
    return !deserializeJson(doc, static_cast<const char*>(json), size - 1, DeserializationOption::NestingLimit(5));
}
void refresh(uint8_t slot) {
    JsonDocument doc; Route route;
    entries[slot] = Entry{};
    if (read(slot, doc) && decode(doc, route)) {
        entries[slot].valid = true; entries[slot].count = route.count;
        entries[slot].savedOrder = doc["savedOrder"] | 0U; // older firmware's routes sort first
        strlcpy(entries[slot].name, route.name, sizeof(entries[slot].name));
    }
}
}

bool begin() {
    static_assert(sizeof(Job) <= 1024, "Keep each local storage mailbox payload within 1 KB");
    static_assert(AtlasConfig::SensorCount == 4, "Local route JSON describes four ultrasonic directions");
    static_assert(AtlasConfig::LocalRouteMaxSegments <= AtlasConfig::MaximumRouteSegments, "Local route capacity exceeds navigation capacity");
    static_assert(AtlasConfig::LocalRouteSlots > 0 && AtlasConfig::LocalRouteSlots <= 9, "Local route URLs use one-digit slots");
    static_assert(AtlasConfig::LocalRouteMaxSegments > 0, "Local routes need at least one segment");
    jobs = xQueueCreate(1, sizeof(Job)); results = xQueueCreate(1, sizeof(Job));
    // Never overwrite normal ultrasonic-validated routes with test recordings.
    available = prefs.begin(AtlasConfig::EncoderOnlyGroundTest ? "atlasedgetest" : "atlasroutes", false);
    if (available) for (uint8_t i = 0; i < AtlasConfig::LocalRouteSlots; ++i) refresh(i);
    return available && jobs && results;
}
bool enqueue(const Job& job) { return available && jobs && results && xQueueSend(jobs, &job, 0) == pdTRUE; }
bool receive(Job& job) { return results && xQueueReceive(results, &job, 0) == pdTRUE; }
void workerUpdate() {
    Job job;
    if (!jobs || !results || xQueueReceive(jobs, &job, 0) != pdTRUE) return;
    JsonDocument doc;
    job.ok = false;
    if (job.kind == Job::Kind::Save && job.slot == AutomaticSlot) job.slot = automaticSlot();
    if (job.slot < AtlasConfig::LocalRouteSlots) {
        if (job.kind == Job::Kind::Save) {
            const uint32_t order = nextOrder();
            if (order && job.route.count && job.route.count <= AtlasConfig::LocalRouteMaxSegments) {
                Route checked;
                if (LocalRouteJson::encode(job.route, doc, calibration(), order, AtlasConfig::EncoderOnlyGroundTest) &&
                    decode(doc, checked) && measureJson(doc) + 1 <= AtlasConfig::LocalRouteJsonBytes) {
                    static char json[AtlasConfig::LocalRouteJsonBytes];
                    const size_t size = serializeJson(doc, json, sizeof(json));
                    char name[16]; key(job.slot, name, sizeof(name));
                    // putString commits the single JSON value. Never erase NVS or another slot on failure.
                    job.ok = prefs.putString(name, json) == size;
                    if (job.ok) {
                        JsonDocument verify; Route persisted;
                        job.ok = read(job.slot, verify) && decode(verify, persisted);
                        if (job.ok) {
                            // Compare exact normalized JSON, not just its length.
                            static char verified[AtlasConfig::LocalRouteJsonBytes];
                            serializeJson(verify, verified, sizeof(verified));
                            job.ok = !strcmp(json, verified);
                        }
                    }
                }
            }
            refresh(job.slot);
        } else job.ok = read(job.slot, doc) && decode(doc, job.route);
    }
    xQueueOverwrite(results, &job);
}
void list(JsonDocument& doc) {
    doc["storageReady"] = available && jobs && results;
    doc["automaticSaveSlot"] = automaticSlot();
    auto slots = doc["routes"].to<JsonArray>();
    for (uint8_t i = 0; i < AtlasConfig::LocalRouteSlots; ++i) {
        auto slot = slots.add<JsonObject>(); slot["slot"] = i; slot["valid"] = entries[i].valid;
        slot["name"] = entries[i].name; slot["segments"] = entries[i].count;
        slot["savedOrder"] = entries[i].savedOrder;
    }
}
bool content(uint8_t slot, JsonDocument& doc) {
    Route checked;
    return read(slot, doc) && decode(doc, checked);
}
}
