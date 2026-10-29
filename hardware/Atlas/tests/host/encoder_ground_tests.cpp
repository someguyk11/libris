// Production modules, simulated electrical I/O/time/NVS. NOT a physical test.
#include "LocalDrive.h"
#include "LocalRouteStore.h"
#include "LocalRouteJson.h"
#include <Preferences.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

namespace sim {
uint64_t us = 1000000;
struct Interrupt { void (*handler)(void*) = nullptr; void* arg = nullptr; int edge = 0; } irq[40];
bool pwmOk = true;
unsigned sensorIo = 0;
void checkPin(int pin) {
    assert(pin >= 0 && pin < 40);
    if (pin != 18 && pin != 19 && pin != 25 && pin != 26 && pin != 27 &&
        pin != 33 && pin != 34 && pin != 35) ++sensorIo;
}
}
uint32_t millis() { return static_cast<uint32_t>(sim::us / 1000); }
int64_t esp_timer_get_time() { return static_cast<int64_t>(sim::us); }
void pinMode(int pin, int) { sim::checkPin(pin); }
void digitalWrite(int pin, int) { sim::checkPin(pin); }
int digitalRead(int pin) { sim::checkPin(pin); return LOW; }
void delayMicroseconds(uint32_t us) { sim::us += us; }
void attachInterruptArg(int pin, void (*handler)(void*), void* arg, int edge) {
    sim::checkPin(pin); sim::irq[pin] = {handler, arg, edge};
}
bool ledcAttach(int pin, uint32_t, uint8_t) { sim::checkPin(pin); return true; }
bool ledcWrite(int pin, uint32_t) { sim::checkPin(pin); return sim::pwmOk; }
bool ledcDetach(int) { return true; }

using namespace AtlasConfig;
struct Rig {
    MotionController motion;
    SensorManager sensors;
    SafetyController safety{motion, sensors};
    CheckpointManager checkpoints;
    Routes routes{motion, sensors, checkpoints};
    ObstacleAvoidance obstacles;
    Navigation navigation{motion, sensors, safety, routes, checkpoints, obstacles};
    Rig() {
        sim::us += 1000000; sim::pwmOk = true;
        assert(!pinConfigurationError() && !tuningError() && !navigationTuningError());
        assert(distanceCalibrated() && !turnsCalibrated());
        assert(motion.begin() && sensors.begin());
        safety.begin(millis()); navigation.begin(millis());
        assert(LocalDrive::begin());
        tick(150);
        assert(safety.canMove());
        assert(sim::irq[34].edge == CHANGE && sim::irq[35].edge == CHANGE);
        assert(!sensors.reading(SensorId::Front, millis()).valid);
    }
    void tick(unsigned ms = 5) {
        for (unsigned i = 0; i < ms; i += 5) {
            sim::us += 5000;
            safety.update(millis()); sensors.update(millis()); navigation.update(millis());
            LocalDrive::update(motion, sensors, safety, navigation, routes, millis(), true);
            safety.update(millis()); motion.update(millis()); safety.update(millis());
            navigation.update(millis());
        }
    }
    JsonDocument status() {
        tick(50); JsonDocument doc; LocalDrive::status(doc); return doc;
    }
    void send(const char* command, const char* name = nullptr, int slot = -1) {
        auto s = status(); JsonDocument request;
        request["command"] = command; request["token"] = s["token"].as<const char*>();
        request["sequence"] = s["sequence"].as<uint32_t>() + 1;
        request["slot"] = slot; request["startConfirmed"] = true;
        if (name) request["name"] = name;
        assert(LocalDrive::submit(request)); tick();
    }
    void pulses(unsigned left, unsigned right) {
        for (unsigned i = 0; i < left; ++i) sim::irq[34].handler(sim::irq[34].arg);
        for (unsigned i = 0; i < right; ++i) sim::irq[35].handler(sim::irq[35].arg);
    }
    void finishStep() {
        // 39 edges = 19.91 cm; within existing 0.5 cm completion tolerance.
        pulses(39, 39); tick();
        assert(motion.state() == MotionController::State::Complete);
        tick(CheckpointSettleMs + 5);
    }
};

void record_save_load_replay() {
    Preferences normal;
    assert(normal.begin("atlasroutes", false));
    assert(normal.putString("route0", "normal-route-sentinel") > 0);
    assert(LocalRouteStore::begin()); Rig r;
    assert(sizeof(Routes::Segment) == 28);
    auto status = r.status(); assert(status["encoderOnlyGroundTest"].as<bool>());
    r.send("record"); assert(r.navigation.state() == Navigation::State::Recording);
    r.send("front"); assert(r.motion.moving()); r.finishStep();
    assert(r.routes.count() == 1 && r.routes.segment(0)->endpoint.validMask == 0);
    const float measured = r.routes.segment(0)->distanceCm;
    assert(std::fabs(measured - 39 * 3.141592654f * 6.5f / 40) < 0.001f);
    r.send("stop"); r.send("back"); r.finishStep(); r.send("stop");
    assert(r.routes.count() == 2);
    r.send("save", "Ground path");
    LocalRouteStore::workerUpdate(); r.tick();
    JsonDocument saved; assert(LocalRouteStore::content(0, saved));
    assert(saved["version"] == 3 && saved["segments"].size() == 2);
    assert(saved["segments"][0].size() == 2); // no fabricated sensor evidence
    LocalRouteStore::Route decoded;
    assert(!LocalRouteJson::decode(saved, decoded, saved["calibration"], Routes::validSegment));
    const size_t jsonBytes = measureJson(saved) + 1;
    assert(LocalRouteStore::begin()); // simulated reopen, persisted fake NVS
    assert(LocalRouteStore::content(0, saved));
    r.send("discard"); assert(!r.routes.count());
    r.send("replay", nullptr, 0); LocalRouteStore::workerUpdate(); r.tick(150);
    assert(r.navigation.state() == Navigation::State::Replaying && r.motion.moving());
    r.finishStep(); assert(r.routes.index() == 1);
    r.send("heartbeat"); r.tick(150); assert(r.motion.moving());
    r.finishStep(); r.tick();
    assert(r.navigation.state() == Navigation::State::Idle); // local completion cleanup
    assert(r.routes.index() == 2 && !r.motion.moving());
    assert(!r.navigation.turn90(true) && !r.navigation.turn90(false));
    char sentinel[32];
    assert(normal.getString("route0", sentinel, sizeof(sentinel)) > 0);
    assert(std::string(sentinel) == "normal-route-sentinel");
    r.send("replay", nullptr, 0); LocalRouteStore::workerUpdate(); r.tick(150);
    assert(r.motion.moving()); r.tick(LocalDriveLeaseMs + 10);
    assert(r.safety.latched() && r.safety.fault() == AtlasFault::EmergencyStop && !r.motion.moving());
    std::printf("record/save/reopen/load/replay passed (simulated NVS, 2 steps %zu B, segment %zu B)\n", jsonBytes, sizeof(Routes::Segment));
}

void capacity_and_replacement() {
    for (unsigned save = 0; save < 4; ++save) {
        LocalRouteStore::Job job{};
        job.kind = LocalRouteStore::Job::Kind::Save;
        job.id = save + 1; job.slot = LocalRouteStore::AutomaticSlot;
        job.route.count = LocalRouteMaxSegments;
        std::snprintf(job.route.name, sizeof(job.route.name), "Full route %u", save);
        for (auto& segment : job.route.segments) {
            segment.kind = MotionController::Kind::Forward; segment.distanceCm = 20;
        }
        assert(LocalRouteStore::enqueue(job)); LocalRouteStore::workerUpdate();
        LocalRouteStore::Job done; assert(LocalRouteStore::receive(done) && done.ok);
    }
    JsonDocument list; LocalRouteStore::list(list);
    for (auto slot : list["routes"].as<JsonArray>())
        assert(slot["valid"].as<bool>() && slot["segments"] == 32);
    // Initial Ground path was oldest; four further saves replace it, then
    // replace Full route 0. All three newest full routes remain.
    unsigned names = 0;
    for (auto slot : list["routes"].as<JsonArray>()) {
        const std::string name = slot["name"].as<const char*>();
        if (name == "Full route 1" || name == "Full route 2" || name == "Full route 3") ++names;
    }
    assert(names == 3);
    std::puts("three 32-segment slots and oldest-replacement passed (simulated NVS)");
}

void protections() {
    { Rig r; r.send("front"); r.tick(LocalDriveLeaseMs + 10);
      assert(r.safety.latched() && r.safety.fault() == AtlasFault::EmergencyStop && !r.motion.moving()); }
    { Rig r; r.send("front"); r.send("emergency");
      assert(r.safety.latched() && !r.motion.moving());
      r.send("reset"); assert(!r.safety.latched() && !r.motion.moving()); }
    { Rig r; assert(r.navigation.driveDistance(20));
      r.tick(StallTimeoutMs + StallStartupGraceMs + 100);
      assert(r.safety.latched() && r.safety.fault() == AtlasFault::StallLeft); }
    { Rig r; assert(r.navigation.driveDistance(20));
      sim::us += (MaximumControlGapMs + 1) * 1000;
      r.safety.update(millis()); assert(r.safety.fault() == AtlasFault::ControlOverrun); }
    { Rig r; assert(r.navigation.driveDistance(20)); sim::pwmOk = false; r.tick();
      assert(r.safety.latched() && r.safety.fault() == AtlasFault::Pwm); sim::pwmOk = true; }
    { Rig r; assert(r.navigation.driveDistance(200));
      // Enough pulses to avoid a stall, too little distance to finish in 20 s.
      for (unsigned i = 0; i < 42 && r.motion.moving(); ++i) { r.pulses(1, 1); r.tick(500); }
      assert(r.safety.latched() && r.safety.fault() == AtlasFault::MovementTimeout); }
    { Rig r; r.send("record"); r.send("front"); r.pulses(45, 39); r.tick();
      r.tick(CheckpointSettleMs + 5); assert(r.navigation.state() == Navigation::State::Fault); }
    assert(sim::sensorIo == 0);
    std::puts("deadman/E-stop/reset/stall/control-overrun/PWM/checkpoint protections passed; no ultrasonic I/O");
}
int main() { record_save_load_replay(); capacity_and_replacement(); protections(); }
