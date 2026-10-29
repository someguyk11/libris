#include "LocalRoutePolicy.h"
#include <cassert>
#include <cstdio>

struct Entry { bool valid; uint32_t savedOrder; char name; };
int main() {
    Entry slots[3]{};
    auto save = [&](char name) {
        const auto slot = LocalRoutePolicy::chooseSlot(slots);
        const auto order = LocalRoutePolicy::nextOrder(slots);
        assert(order != 0);
        slots[slot] = {true, order, name};
        return slot;
    };
    assert(save('A') == 0);
    assert(save('B') == 1);
    assert(save('C') == 2);
    assert(save('D') == 0);
    assert(slots[0].name == 'D' && slots[1].name == 'B' && slots[2].name == 'C');
    // Simulate reboot by restoring the saved metadata, with no volatile cursor.
    const Entry restored[3] = {slots[0], slots[1], slots[2]};
    assert(LocalRoutePolicy::chooseSlot(restored) == 1);
    assert(save('E') == 1);
    assert(slots[0].name == 'D' && slots[1].name == 'E' && slots[2].name == 'C');
    // An explicit manual overwrite makes that slot newest too.
    slots[2] = {true, LocalRoutePolicy::nextOrder(slots), 'F'};
    assert(LocalRoutePolicy::chooseSlot(slots) == 0);
    // Unused/invalid slots precede all valid routes, regardless of age.
    slots[2].valid = false;
    assert(LocalRoutePolicy::chooseSlot(slots) == 2);
    const Entry legacy[3] = {{true, 0, 'A'}, {true, 0, 'B'}, {true, 0, 'C'}};
    assert(LocalRoutePolicy::chooseSlot(legacy) == 0);
    assert(LocalRoutePolicy::nextOrder(legacy) == 1);
    const Entry exhausted[1] = {{true, UINT32_MAX, 'X'}};
    assert(LocalRoutePolicy::nextOrder(exhausted) == 0);
    std::puts("Local route policy tests passed");
}
