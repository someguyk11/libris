#include "LocalRouteJson.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

enum class Kind : uint8_t { None, Forward, Backward, Clockwise90, Counterclockwise90 };
struct Endpoint { float rangesCm[4]{}; uint8_t validMask = 0; };
struct Segment { Kind kind{}; float distanceCm = 0; Endpoint endpoint; };
struct Route { char name[41]{}; uint8_t count = 0; Segment segments[32]; };
bool validSegment(const Segment& s) {
    const auto k = static_cast<unsigned>(s.kind);
    if (k < 1 || k > 4 || s.endpoint.validMask != 15) return false;
    if (k <= 2 ? (!std::isfinite(s.distanceCm) || s.distanceCm <= 0.5f || s.distanceCm > 200)
               : s.distanceCm != 0) return false;
    for (float cm : s.endpoint.rangesCm) if (!std::isfinite(cm) || cm < 2 || cm > 300) return false;
    return true;
}
int main() {
    Route input;
    std::memset(input.name, '"', 40); // maximize JSON escaping of a legal 40-byte name
    input.count = 32;
    for (auto& s : input.segments) {
        s.kind = Kind::Forward; s.distanceCm = 123.456789f; s.endpoint.validMask = 15;
        for (float& cm : s.endpoint.rangesCm) cm = 234.567891f;
    }
    JsonDocument doc;
    assert(LocalRouteJson::encode(input, doc, UINT32_MAX, UINT32_MAX));
    const size_t bytes = measureJson(doc) + 1;
    assert(bytes <= 3000); // unchanged production buffer and below NVS's 4000-byte string cap
    std::string json; serializeJson(doc, json);
    JsonDocument loaded; assert(!deserializeJson(loaded, json));
    Route output;
    assert(LocalRouteJson::decode(loaded, output, UINT32_MAX, validSegment));
    assert(output.count == 32);
    for (auto& s : output.segments) {
        assert(s.kind == Kind::Forward);
        assert(std::fabs(s.distanceCm - input.segments[0].distanceCm) < 0.0001f);
        assert(std::fabs(s.endpoint.rangesCm[0] - input.segments[0].endpoint.rangesCm[0]) < 0.0001f);
    }
    assert(!LocalRouteJson::decode(loaded, output, 0, validSegment)); // changed calibration
    loaded["segments"][0][2] = nullptr;
    assert(!LocalRouteJson::decode(loaded, output, UINT32_MAX, validSegment));
    assert(!deserializeJson(loaded, json));
    loaded["segments"].as<JsonArray>().add<JsonArray>(); // 33rd segment rejected
    assert(!LocalRouteJson::decode(loaded, output, UINT32_MAX, validSegment));
    input.segments[0].endpoint.validMask = 7;
    assert(!LocalRouteJson::encode(input, doc, 1, 1)); // never turn missing sensors into valid evidence
    const char* legacy = R"({"version":1,"name":"Old path","calibration":42,"segments":[{"kind":1,"cm":20,"mask":15,"ranges":[100,101,102,103]}]})";
    assert(!deserializeJson(loaded, legacy));
    assert(LocalRouteJson::decode(loaded, output, 42, validSegment));
    assert(output.count == 1 && output.segments[0].distanceCm == 20);
    size_t maximumBytes = bytes;
    uint32_t random = 42;
    auto sample = [&]() { random = random * 1664525U + 1013904223U; return (random >> 8) / 16777216.0f; };
    for (unsigned run = 0; run < 1000; ++run) {
        for (auto& s : input.segments) {
            s.kind = static_cast<Kind>(1 + run % 4);
            s.distanceCm = run % 4 < 2 ? 0.6f + sample() * 199.4f : 0;
            s.endpoint.validMask = 15;
            for (float& cm : s.endpoint.rangesCm) cm = 2 + sample() * 298;
        }
        assert(LocalRouteJson::encode(input, doc, UINT32_MAX, UINT32_MAX));
        const size_t size = measureJson(doc) + 1;
        assert(size <= 3000);
        if (size > maximumBytes) maximumBytes = size;
        assert(LocalRouteJson::decode(doc, output, UINT32_MAX, validSegment));
    }
    std::printf("32-segment JSON: initial %zu B, largest of 1000 varied routes %zu B (3000 B cap); round-trip/legacy/invalid-data tests passed\n", bytes, maximumBytes);
    // Ground-test routes contain NO manufactured ultrasonic evidence and must
    // not cross-load into normal firmware (or load old routes without checks).
    auto validEncoder = [](const Segment& s) {
        const auto kind = static_cast<unsigned>(s.kind);
        return kind >= 1 && kind <= 2 && s.endpoint.validMask == 0 &&
            std::isfinite(s.distanceCm) && s.distanceCm > 0.5f && s.distanceCm <= 200;
    };
    for (auto& s : input.segments) {
        s.kind = Kind::Backward; s.distanceCm = 19.90984f; s.endpoint = Endpoint{};
    }
    assert(LocalRouteJson::encode(input, doc, 42, 1, true));
    assert(doc["version"] == 3 && doc["segments"][0].size() == 2);
    const size_t groundBytes = measureJson(doc) + 1;
    assert(groundBytes <= 3000);
    json.clear(); serializeJson(doc, json); assert(!deserializeJson(loaded, json));
    assert(LocalRouteJson::decode(loaded, output, 42, validEncoder, true));
    assert(output.count == 32 && output.segments[0].endpoint.validMask == 0);
    assert(!LocalRouteJson::decode(loaded, output, 42, validSegment));
    assert(!LocalRouteJson::decode(loaded, output, 43, validEncoder, true));
    loaded["segments"][0][0] = 3; // turns prohibited in ground mode
    assert(!LocalRouteJson::decode(loaded, output, 42, validEncoder, true));
    assert(!deserializeJson(loaded, json)); loaded["segments"].as<JsonArray>().add<JsonArray>();
    assert(!LocalRouteJson::decode(loaded, output, 42, validEncoder, true));
    assert(!deserializeJson(loaded, legacy));
    assert(!LocalRouteJson::decode(loaded, output, 42, validEncoder, true));
    input.segments[0].kind = Kind::Clockwise90;
    assert(!LocalRouteJson::encode(input, doc, 42, 1, true));
    input.segments[0].kind = Kind::Forward; input.segments[0].endpoint.validMask = 15;
    assert(!LocalRouteJson::encode(input, doc, 42, 1, true));
    std::printf("32-segment encoder-only JSON: %zu B; round-trip/schema isolation/turn/capacity rejection passed\n", groundBytes);
}
