#pragma once
#include <cstddef>
#include <cstdint>

namespace LocalRoutePolicy {
// Pure policy, shared with host tests. Entries expose valid and savedOrder.
template <typename Entry, size_t Count>
constexpr uint8_t chooseSlot(const Entry (&entries)[Count]) {
    static_assert(Count > 0 && Count <= 255, "Route slot count must fit uint8_t");
    uint8_t oldest = 0;
    for (size_t i = 0; i < Count; ++i) {
        if (!entries[i].valid) return static_cast<uint8_t>(i);
        if (entries[i].savedOrder < entries[oldest].savedOrder) oldest = static_cast<uint8_t>(i);
    }
    return oldest;
}

template <typename Entry, size_t Count>
constexpr uint32_t nextOrder(const Entry (&entries)[Count]) {
    uint32_t latest = 0;
    for (const auto& entry : entries) if (entry.savedOrder > latest) latest = entry.savedOrder;
    return latest == UINT32_MAX ? 0 : latest + 1; // reject exhaustion instead of wrapping
}
}
