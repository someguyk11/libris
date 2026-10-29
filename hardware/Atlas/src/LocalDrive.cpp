#include "LocalDrive.h"
#include "LocalRouteStore.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_random.h>

namespace LocalDrive {
namespace {
enum class Action : uint8_t { Stop, Front, Back, Left, Right, Emergency, Reset, Record, Save, Replay, Heartbeat, Discard };
struct Request { Action action; uint32_t at; uint32_t sequence; uint8_t slot; char name[41]; };
struct Snapshot {
    uint32_t at, left, right, sequence;
    float cm[AtlasConfig::SensorCount];
    bool valid[AtlasConfig::SensorCount];
    bool permitted, latched, moving;
    AtlasFault fault;
    Navigation::State state;
    bool recording, replaying, storageBusy, stepPending;
    uint8_t routeCount, routeIndex;
    char message[96];
};
QueueHandle_t requests = nullptr, snapshots = nullptr;
char token[33]{};
uint32_t highSequence = 0; // worker-only, never reset by a new browser
Action held = Action::Stop; // main-only
uint32_t heartbeat = 0, published = 0;
bool storageBusy = false;
uint32_t storageJob = 0;
Snapshot snapshot{};
}

bool begin() {
    requests = xQueueCreate(1, sizeof(Request));
    snapshots = xQueueCreate(1, sizeof(Snapshot));
    return requests && snapshots;
}

bool submit(JsonDocument& doc) {
    if (!AtlasConfig::LocalDriveEnabled || !requests || !snapshots || !token[0] ||
        !doc["token"].is<const char*>() || strcmp(doc["token"], token) ||
        !doc["sequence"].is<uint32_t>() || !doc["command"].is<const char*>()) return false;
    const uint32_t sequence = doc["sequence"];
    if (sequence <= highSequence) return false;
    const char* command = doc["command"];
    Action action;
    if (!strcmp(command, "stop")) action = Action::Stop;
    else if (!strcmp(command, "front")) action = Action::Front;
    else if (!strcmp(command, "back")) action = Action::Back;
    else if (!strcmp(command, "left")) action = Action::Left;
    else if (!strcmp(command, "right")) action = Action::Right;
    else if (!strcmp(command, "emergency")) action = Action::Emergency;
    else if (!strcmp(command, "reset")) action = Action::Reset;
    else if (!strcmp(command, "record")) action = Action::Record;
    else if (!strcmp(command, "save")) action = Action::Save;
    else if (!strcmp(command, "replay")) action = Action::Replay;
    else if (!strcmp(command, "heartbeat")) action = Action::Heartbeat;
    else if (!strcmp(command, "discard")) action = Action::Discard;
    else return false;
    Request request{};
    if (action == Action::Save || action == Action::Replay) {
        if (action == Action::Save && doc["slot"].is<int>() && doc["slot"].as<int>() == -1) request.slot = LocalRouteStore::AutomaticSlot;
        else {
            if (!doc["slot"].is<unsigned>() || doc["slot"].as<unsigned>() >= AtlasConfig::LocalRouteSlots) return false;
            request.slot = doc["slot"];
        }
    }
    if (action == Action::Replay && (!doc["startConfirmed"].is<bool>() || !doc["startConfirmed"].as<bool>())) return false;
    if (action == Action::Save) {
        if (!doc["name"].is<const char*>()) return false;
        const char* name = doc["name"];
        if (!name[0] || strlen(name) > 40) return false;
        for (const char* p = name; *p; ++p) if (static_cast<uint8_t>(*p) < 32) return false;
        strlcpy(request.name, name, sizeof(request.name));
    }
    highSequence = sequence;
    request.action = action; request.at = millis(); request.sequence = sequence;
    return xQueueOverwrite(requests, &request) == pdPASS;
}

void status(JsonDocument& doc) {
    // Worker-only, after Wi-Fi is active (RF provides the RNG entropy source).
    if (!token[0]) {
        uint8_t random[16]; esp_fill_random(random, sizeof(random));
        for (uint8_t i = 0; i < sizeof(random); ++i) snprintf(token + i * 2, 3, "%02x", random[i]);
    }
    Snapshot value{};
    doc["enabled"] = AtlasConfig::LocalDriveEnabled;
    doc["token"] = token;
    doc["sequence"] = highSequence;
    const bool fresh = snapshots && xQueuePeek(snapshots, &value, 0) == pdTRUE && millis() - value.at < 500;
    doc["fresh"] = fresh;
    doc["ready"] = fresh && value.permitted;
    doc["moving"] = value.moving;
    doc["latched"] = value.latched;
    doc["fault"] = atlasFaultName(value.fault);
    doc["navigation"] = Navigation::stateName(value.state);
    doc["message"] = value.message;
    doc["appliedSequence"] = value.sequence;
    doc["leftEdges"] = value.left; doc["rightEdges"] = value.right;
    doc["distanceCalibrated"] = AtlasConfig::distanceCalibrated();
    doc["turnsCalibrated"] = AtlasConfig::turnsCalibrated();
    doc["stepCm"] = AtlasConfig::LocalDriveStepCm;
    doc["leaseMs"] = AtlasConfig::LocalDriveLeaseMs;
    doc["recording"] = value.recording; doc["replaying"] = value.replaying;
    doc["storageBusy"] = value.storageBusy; doc["stepPending"] = value.stepPending;
    doc["routeCount"] = value.routeCount; doc["routeIndex"] = value.routeIndex;
    doc["maxRouteSegments"] = AtlasConfig::LocalRouteMaxSegments;
    LocalRouteStore::list(doc);
    doc["configurationError"] = AtlasConfig::pinConfigurationError();
    auto readings = doc["sensors"].to<JsonArray>();
    for (uint8_t i = 0; i < AtlasConfig::SensorCount; ++i) {
        auto reading = readings.add<JsonObject>();
        reading["valid"] = fresh && value.valid[i];
        if (fresh && value.valid[i]) reading["cm"] = value.cm[i];
        else reading["cm"] = nullptr;
    }
}

void update(MotionController& motion, SensorManager& sensors, SafetyController& safety,
            Navigation& navigation, Routes& routes, uint32_t nowMs, bool networkPermit) {
    if (!AtlasConfig::LocalDriveEnabled || !requests || !snapshots) return;
    if (!networkPermit) {
        navigation.cancel(); held = Action::Stop;
    }
    // Check expiration BEFORE consuming a new heartbeat; an expired hold cannot resume.
    if (held != Action::Stop && nowMs - heartbeat >= AtlasConfig::LocalDriveLeaseMs) {
        navigation.cancel(); safety.emergencyStop(); held = Action::Stop;
        strlcpy(snapshot.message, "Heartbeat lost: reset safety, then press a new direction", sizeof(snapshot.message));
    }
    LocalRouteStore::Job completed;
    if (storageBusy && LocalRouteStore::receive(completed)) {
        storageBusy = false;
        if (completed.kind == LocalRouteStore::Job::Kind::Save) {
            if (completed.ok) snprintf(snapshot.message, sizeof(snapshot.message), "Path saved as JSON on ESP32 in slot %u", completed.slot + 1);
            else strlcpy(snapshot.message, "Save failed: storage full/unavailable; RAM path retained", sizeof(snapshot.message));
        } else if (completed.id == storageJob && held == Action::Replay) {
            const bool ok = networkPermit && completed.ok && safety.canMove() &&
                nowMs - heartbeat < AtlasConfig::LocalDriveLeaseMs &&
                routes.load(completed.route.segments, completed.route.count) && navigation.replay();
            if (!ok) held = Action::Stop;
            strlcpy(snapshot.message, ok ? "Replaying selected local route" : "Replay rejected: invalid route/calibration/safety/lease", sizeof(snapshot.message));
        }
    }
    if (held == Action::Replay && !storageBusy && navigation.state() != Navigation::State::Replaying) {
        // A pause/fault is a terminal local replay interruption, not an auto-resume.
        const bool complete = navigation.state() == Navigation::State::Completed;
        navigation.cancel(); held = Action::Stop;
        strlcpy(snapshot.message, complete ? "Replay completed" : "Replay interrupted; re-position before starting again", sizeof(snapshot.message));
    }
    Request request{};
    if (xQueueReceive(requests, &request, 0) == pdTRUE) {
        snapshot.sequence = request.sequence;
        if (nowMs - request.at >= AtlasConfig::LocalDriveLeaseMs) {
            navigation.cancel(); held = Action::Stop;
            strlcpy(snapshot.message, "Expired request rejected", sizeof(snapshot.message));
        } else if (request.action == Action::Heartbeat) {
            if (held == Action::Replay) heartbeat = request.at;
        } else if (request.action == Action::Stop || request.action == Action::Emergency || request.action == Action::Reset || request.action == Action::Discard) {
            const bool keepRecording = networkPermit && request.action == Action::Stop && navigation.state() == Navigation::State::Recording && !motion.moving();
            if (!keepRecording) navigation.cancel();
            held = Action::Stop;
            if (request.action == Action::Discard) routes.clear();
            if (request.action == Action::Emergency) safety.emergencyStop();
            const bool ok = request.action != Action::Reset || navigation.resetSafety(nowMs);
            strlcpy(snapshot.message, ok ? "Stopped; no automatic resume" : "Reset denied: check configuration/sensors", sizeof(snapshot.message));
        } else if (!networkPermit) {
            strlcpy(snapshot.message, "Wi-Fi setup/mode change active: movement disabled", sizeof(snapshot.message));
        } else if (storageBusy) {
            strlcpy(snapshot.message, "Storage busy: motors held stationary", sizeof(snapshot.message));
        } else if (request.action == Action::Record) {
            const bool ok = held == Action::Stop && !navigation.movementPending() && navigation.beginRecording();
            strlcpy(snapshot.message, ok ? "Recording: hold each move until validated; then release" : "Cannot record: stop/reset/discard first", sizeof(snapshot.message));
        } else if (request.action == Action::Save) {
            const bool recording = navigation.state() == Navigation::State::Recording;
            bool ok = held == Action::Stop && !motion.moving() && !navigation.movementPending() &&
                (recording || navigation.state() == Navigation::State::Idle) &&
                routes.count() && routes.count() <= AtlasConfig::LocalRouteMaxSegments;
            if (ok && recording) ok = navigation.finishRecording();
            if (ok) {
                LocalRouteStore::Job job{}; job.kind = LocalRouteStore::Job::Kind::Save;
                job.id = request.sequence; job.slot = request.slot; job.route.count = routes.count();
                strlcpy(job.route.name, request.name, sizeof(job.route.name));
                for (uint8_t i = 0; i < routes.count(); ++i) job.route.segments[i] = *routes.segment(i);
                ok = LocalRouteStore::enqueue(job);
                storageBusy = ok; storageJob = job.id;
            }
            strlcpy(snapshot.message, ok ? "Saving JSON; motors held stationary" : "Save rejected: finish a valid nonempty path and release controls", sizeof(snapshot.message));
        } else if (request.action == Action::Replay) {
            const bool idle = navigation.state() == Navigation::State::Idle || navigation.state() == Navigation::State::Completed;
            bool ok = held == Action::Stop && idle && !motion.moving() && !navigation.movementPending() && safety.canMove() && AtlasConfig::ObstacleAvoidanceEnabled;
            if (ok) {
                LocalRouteStore::Job job{}; job.kind = LocalRouteStore::Job::Kind::Load;
                job.id = request.sequence; job.slot = request.slot;
                navigation.cancel();
                ok = LocalRouteStore::enqueue(job);
                if (ok) { storageBusy = true; storageJob = job.id; held = Action::Replay; heartbeat = request.at; }
            }
            strlcpy(snapshot.message, ok ? "Loading selected local route" : "Replay rejected: stop/discard/reset first", sizeof(snapshot.message));
        } else if (request.action == held) {
            heartbeat = request.at; // never repeat a completed movement while held
        } else {
            if (held == Action::Replay || navigation.movementPending()) {
                strlcpy(snapshot.message, "Movement pending: stop or wait for validation", sizeof(snapshot.message));
            } else {
                const bool recording = navigation.state() == Navigation::State::Recording;
                if (!recording) navigation.cancel();
                held = request.action; heartbeat = request.at;
                const bool configured = AtlasConfig::positive(AtlasConfig::LocalDriveStepCm) &&
                    AtlasConfig::LocalDriveStepCm > AtlasConfig::DistanceToleranceCm &&
                    AtlasConfig::LocalDriveStepCm <= AtlasConfig::MaximumMoveCm &&
                    AtlasConfig::LocalDriveLeaseMs >= 400 && AtlasConfig::LocalDriveLeaseMs <= 2000 &&
                    AtlasConfig::ObstacleAvoidanceEnabled && (!recording || routes.count() < AtlasConfig::LocalRouteMaxSegments);
                const bool ok = configured && (request.action == Action::Front ? navigation.driveDistance(AtlasConfig::LocalDriveStepCm) :
                    request.action == Action::Back ? navigation.driveDistance(-AtlasConfig::LocalDriveStepCm) :
                    navigation.turn90(request.action == Action::Right));
                strlcpy(snapshot.message, ok ? "Movement accepted locally; release to stop" : "Movement rejected: check safety/calibration/dwell", sizeof(snapshot.message));
            }
        }
    }
    if (nowMs - published < 50) return;
    published = nowMs; snapshot.at = nowMs;
    motion.encoderCounts(snapshot.left, snapshot.right);
    snapshot.permitted = networkPermit && safety.canMove(); snapshot.latched = safety.latched();
    snapshot.fault = safety.fault(); snapshot.moving = motion.moving(); snapshot.state = navigation.state();
    snapshot.recording = navigation.state() == Navigation::State::Recording;
    snapshot.replaying = held == Action::Replay;
    snapshot.storageBusy = storageBusy; snapshot.stepPending = navigation.movementPending();
    snapshot.routeCount = routes.count(); snapshot.routeIndex = routes.index();
    for (uint8_t i = 0; i < AtlasConfig::SensorCount; ++i) {
        auto reading = sensors.reading(static_cast<AtlasConfig::SensorId>(i), nowMs);
        snapshot.valid[i] = reading.valid && !reading.faulted;
        snapshot.cm[i] = reading.distanceCm;
    }
    xQueueOverwrite(snapshots, &snapshot);
}
}
