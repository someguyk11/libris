#pragma once
#include <ArduinoJson.h>
#include <cstdint>
#include <cstring>

namespace LocalRouteJson {
// Compact JSON with no added measurement quantization. Each segment is
// [kind, centimetres, frontCm, backCm, leftCm, rightCm]. All four ranges required.
// Kept framework-independent so capacity can be tested with the real serializer.
template <typename Route>
bool encode(const Route& route, JsonDocument& doc, uint32_t calibration, uint32_t savedOrder) {
    if (!route.count || route.count > sizeof(route.segments) / sizeof(route.segments[0])) return false;
    for (uint8_t i = 0; i < route.count; ++i)
        if (route.segments[i].endpoint.validMask != 15) return false;
    doc.clear(); doc["version"] = 2; doc["name"] = route.name;
    doc["calibration"] = calibration; doc["savedOrder"] = savedOrder;
    auto segments = doc["segments"].template to<JsonArray>();
    for (uint8_t n = 0; n < route.count; ++n) {
        const auto& segment = route.segments[n]; auto s = segments.template add<JsonArray>();
        s.add(static_cast<uint8_t>(segment.kind)); s.add(segment.distanceCm);
        for (float cm : segment.endpoint.rangesCm) s.add(cm);
    }
    return !doc.overflowed();
}

template <typename Route, typename Validator>
bool decode(JsonDocument& doc, Route& route, uint32_t calibration, Validator validSegment) {
    if (!doc["version"].is<int>() || (doc["version"].as<int>() != 1 && doc["version"].as<int>() != 2) ||
        !doc["calibration"].is<uint32_t>() || doc["calibration"].as<uint32_t>() != calibration ||
        !doc["name"].is<const char*>() || !doc["segments"].is<JsonArray>() ||
        (!doc["savedOrder"].isNull() && !doc["savedOrder"].is<uint32_t>())) return false;
    const char* name = doc["name"];
    const size_t length = std::strlen(name);
    if (!length || length >= sizeof(route.name)) return false;
    auto segments = doc["segments"].template as<JsonArray>();
    if (!segments.size() || segments.size() > sizeof(route.segments) / sizeof(route.segments[0])) return false;
    route = Route{};
    std::memcpy(route.name, name, length + 1);
    const bool legacy = doc["version"].as<int>() == 1;
    for (JsonVariantConst s : segments) {
        if (legacy ? (!s.is<JsonObjectConst>() || !s["mask"].is<unsigned>() || s["mask"].as<unsigned>() != 15 ||
                         !s["ranges"].is<JsonArrayConst>() || s["ranges"].size() != 4)
                   : (!s.is<JsonArrayConst>() || s.size() != 6)) return false;
        const JsonVariantConst kindValue = legacy ? s["kind"] : s[0];
        const JsonVariantConst distance = legacy ? s["cm"] : s[1];
        if (!kindValue.is<unsigned>() || !distance.is<float>()) return false;
        const unsigned kind = kindValue.as<unsigned>();
        if (kind < 1 || kind > 4) return false;
        auto& segment = route.segments[route.count];
        segment.kind = static_cast<decltype(segment.kind)>(kind); segment.distanceCm = distance.as<float>();
        segment.endpoint.validMask = 15;
        for (uint8_t i = 0; i < 4; ++i) {
            const JsonVariantConst cm = legacy ? s["ranges"][i] : s[i + 2];
            if (!cm.is<float>()) return false;
            segment.endpoint.rangesCm[i] = cm.as<float>();
        }
        if (!validSegment(segment)) return false;
        ++route.count;
    }
    return true;
}
}
