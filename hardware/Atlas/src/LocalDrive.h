#pragma once

#include "Navigation.h"
#include <ArduinoJson.h>

// HTTP functions run on the existing network worker. update() is main-loop only.
// Queue copies are the only shared state; HTTP never touches motors/navigation.
namespace LocalDrive {
bool begin();
void update(MotionController& motion, SensorManager& sensors, SafetyController& safety,
            Navigation& navigation, Routes& routes, uint32_t nowMs, bool networkPermit);
void status(JsonDocument& doc);
bool submit(JsonDocument& doc);
}
