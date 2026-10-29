#define main control_test_main
#include "hardware_integration_tests.cpp"
#undef main
#include "TelemetryManager.h"
#include <deque>
#include <vector>
namespace hostNetwork { bool connected=true; }
struct HostQueue { unsigned capacity; size_t bytes; std::deque<std::vector<uint8_t>> data; };
QueueHandle_t xQueueCreate(unsigned count,size_t bytes) { return new HostQueue{count,bytes,{}}; }
void vQueueDelete(QueueHandle_t queue) { delete static_cast<HostQueue*>(queue); }
BaseType_t xQueueSend(QueueHandle_t queue,const void* data,TickType_t) {
    auto& q=*static_cast<HostQueue*>(queue); if(q.data.size()==q.capacity) return 0;
    const auto* bytes=static_cast<const uint8_t*>(data); q.data.emplace_back(bytes,bytes+q.bytes); return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t queue,void* data,TickType_t) {
    auto& q=*static_cast<HostQueue*>(queue); if(q.data.empty()) return 0;
    std::memcpy(data,q.data.front().data(),q.bytes); q.data.pop_front(); return pdTRUE;
}
BaseType_t xQueueOverwrite(QueueHandle_t queue,const void* data) {
    auto& q=*static_cast<HostQueue*>(queue); q.data.clear(); return xQueueSend(queue,data,0);
}
BaseType_t xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t*,int) { return pdPASS; }
void vTaskDelay(TickType_t) { throw std::runtime_error("network worker must not run in host main-loop tests"); }
void esp_restart() { throw std::runtime_error("restart forbidden"); }
bool BackendClient::configured() { return true; }
int64_t BackendClient::utcNowUs() { return static_cast<int64_t>(AtlasConfig::MinimumUtcEpoch)*1000000+sim::us; }
bool BackendClient::poll(Command&) { return false; }
bool BackendClient::pollGuidance(Command&,bool) { return false; }
bool BackendClient::sendResult(const Result&) { return true; }
bool BackendClient::sendSensors(const SensorSample&) { return true; }
bool BackendClient::sendTelemetry(const char*,int,uint64_t) { return true; }
bool BackendClient::acknowledgeRecovery(const Command&) { return true; }
struct TelemetryHarness {
    static void ready(TelemetryManager& t) { t.healthy_.store(true); t.generation_.store(1); t.observedGeneration_=1; }
    static void submit(TelemetryManager& t,const BackendClient::Command& c) { t.command(c,millis()); }
    static bool running(const TelemetryManager& t) { return t.phase_==TelemetryManager::Phase::Running; }
    static bool waiting(const TelemetryManager& t) { return t.phase_==TelemetryManager::Phase::AwaitingStart; }
    static bool idle(const TelemetryManager& t) { return t.phase_==TelemetryManager::Phase::Idle; }
    static bool paused(const TelemetryManager& t) { return t.phase_==TelemetryManager::Phase::Paused; }
    static BackendClient::Result take(TelemetryManager& t) {
        BackendClient::Result r; CHECK(xQueueReceive(t.results_,&r,0)==pdTRUE); return r;
    }
    static void ack(TelemetryManager& t,const BackendClient::Result& r,bool ok=true) {
        TelemetryManager::Ack a{}; strlcpy(a.id,r.id,sizeof(a.id)); strlcpy(a.controlId,r.controlId,sizeof(a.controlId)); a.status=r.status; a.ok=ok;
        CHECK(xQueueSend(t.acks_,&a,0)==pdTRUE);
    }
    static void cleanup(TelemetryManager& t) { for(auto q:{t.commands_,t.results_,t.acks_,t.samples_}) vQueueDelete(q); }
};
struct BackendRig:Rig {
    TelemetryManager telemetry{motion,sensors,safety,nav};
    BackendRig() { hostNetwork::connected=true; CHECK(telemetry.begin()); TelemetryHarness::ready(telemetry); }
    ~BackendRig() { TelemetryHarness::cleanup(telemetry); hostNetwork::connected=true; }
    BackendClient::Command command() {
        BackendClient::Command c; c.guidance=true; c.kind=BackendClient::CommandKind::Forward;
        strlcpy(c.id,"00000000-0000-4000-8000-000000000001",sizeof(c.id));
        strlcpy(c.taskId,"00000000-0000-4000-8000-000000000002",sizeof(c.taskId));
        strlcpy(c.sessionId,"00000000-0000-4000-8000-000000000003",sizeof(c.sessionId));
        strlcpy(c.routeId,"00000000-0000-4000-8000-000000000004",sizeof(c.routeId));
        c.execute=true; c.sequence=c.transportSequence=1; c.generation=c.linkGeneration=1;
        c.leftTicks=c.rightTicks=50; c.fetchedAtMs=millis(); return c;
    }
    void tickBackend() {
        tick(); telemetry.confidence(ccps.supervisor().decision()); telemetry.update(millis()); ccps.apply(millis()); safety.update(millis());
    }
    void startBackend() {
        telemetry.confidence(ccps.supervisor().decision()); TelemetryHarness::submit(telemetry,command());
        const auto r=TelemetryHarness::take(telemetry); CHECK(r.status==BackendClient::ResultStatus::Started);
        CHECK(!motion.moving()); TelemetryHarness::ack(telemetry,r); telemetry.update(millis()); ccps.apply(millis());
        CHECK(motion.moving()); CHECK(TelemetryHarness::running(telemetry));
    }
};
void backend_start_ack_is_required() {
    BackendRig r; TelemetryHarness::submit(r.telemetry,r.command());
    r.tickBackend(); CHECK(!r.motion.moving()); CHECK(TelemetryHarness::waiting(r.telemetry));
}
void backend_completion_waits_for_ccps_settling() {
    BackendRig r; r.startBackend();
    for(unsigned i=0;i<10;++i) { r.pulses(5,5); r.tickBackend(); }
    CHECK(r.motion.state()==MotionController::State::Complete); CHECK(TelemetryHarness::running(r.telemetry));
    for(unsigned i=0;i<100;++i) r.tickBackend();
    CHECK(TelemetryHarness::idle(r.telemetry)); const auto result=TelemetryHarness::take(r.telemetry);
    CHECK(result.status==BackendClient::ResultStatus::Completed); CHECK(result.leftTicks==50&&result.rightTicks==50);
    CHECK(result.measured); CHECK(r.ccps.supervisor().history().size()==1);
}
void backend_duplicates_do_not_repeat_motion() {
    BackendRig r; r.startBackend(); const auto id=r.motion.movementId(); auto c=r.command();
    c.execute=false; TelemetryHarness::submit(r.telemetry,c); r.tickBackend();
    CHECK(r.motion.movementId()==id); CHECK(TelemetryHarness::running(r.telemetry));
}
void backend_identity_change_stops() {
    BackendRig r; r.startBackend(); auto c=r.command(); c.leftTicks=51;
    TelemetryHarness::submit(r.telemetry,c); CHECK(!r.motion.moving()); CHECK(TelemetryHarness::idle(r.telemetry));
    CHECK(TelemetryHarness::take(r.telemetry).status==BackendClient::ResultStatus::Failed);
}
void backend_late_ack_cannot_start() {
    BackendRig r; TelemetryHarness::submit(r.telemetry,r.command()); auto started=TelemetryHarness::take(r.telemetry);
    sim::us+=(AtlasConfig::CommandLeaseMs+1)*1000; TelemetryHarness::ack(r.telemetry,started);
    r.telemetry.update(millis()); CHECK(!r.motion.moving()); CHECK(TelemetryHarness::idle(r.telemetry));
}
void backend_wifi_loss_stops_without_inventing_lost() {
    BackendRig r; r.startBackend(); hostNetwork::connected=false; r.telemetry.update(millis());
    CHECK(!r.motion.moving()); CHECK(TelemetryHarness::idle(r.telemetry));
    CHECK(r.ccps.supervisor().decision().trust!=AtlasConfidence::Trust::Lost);
}
void backend_safety_loss_before_ack_cannot_start() {
    BackendRig r; TelemetryHarness::submit(r.telemetry,r.command()); auto started=TelemetryHarness::take(r.telemetry);
    sim::sensorValid=false; r.tick(); TelemetryHarness::ack(r.telemetry,started); r.telemetry.update(millis());
    CHECK(!r.motion.moving()); CHECK(TelemetryHarness::idle(r.telemetry));
}
void backend_pause_ack_resume_preserves_ticks() {
    BackendRig r; r.startBackend(); r.pulses(10,10); r.tickBackend(); auto c=r.command(); c.execute=false;
    c.control=BackendClient::Command::Control::Pause; strlcpy(c.controlId,"00000000-0000-4000-8000-000000000005",sizeof(c.controlId));
    TelemetryHarness::submit(r.telemetry,c); auto paused=TelemetryHarness::take(r.telemetry);
    CHECK(paused.status==BackendClient::ResultStatus::Paused); CHECK(!r.motion.moving());
    TelemetryHarness::ack(r.telemetry,paused); r.telemetry.update(millis()); CHECK(TelemetryHarness::paused(r.telemetry));
    for(unsigned i=0;i<50;++i) r.tickBackend();
    c=r.command(); c.execute=false; c.control=BackendClient::Command::Control::Resume;
    strlcpy(c.controlId,"00000000-0000-4000-8000-000000000006",sizeof(c.controlId));
    TelemetryHarness::submit(r.telemetry,c); auto resumed=TelemetryHarness::take(r.telemetry);
    CHECK(resumed.status==BackendClient::ResultStatus::Resumed); TelemetryHarness::ack(r.telemetry,resumed); r.telemetry.update(millis());
    r.ccps.apply(millis()); r.tickBackend(); CHECK(r.motion.moving());
    for(unsigned i=0;i<4;++i) { r.pulses(10,10); r.tickBackend(); }
    for(unsigned i=0;i<100;++i) r.tickBackend();
    CHECK(TelemetryHarness::idle(r.telemetry)); auto completed=TelemetryHarness::take(r.telemetry);
    CHECK(completed.status==BackendClient::ResultStatus::Completed); CHECK(completed.leftTicks==50&&completed.rightTicks==50);
    CHECK(r.ccps.supervisor().history().size()==2);
}
int main() {
    using Test=void(*)(); const Test tests[]={backend_start_ack_is_required,backend_completion_waits_for_ccps_settling,
        backend_duplicates_do_not_repeat_motion,backend_identity_change_stops,backend_late_ack_cannot_start,
        backend_wifi_loss_stops_without_inventing_lost,backend_safety_loss_before_ack_cannot_start,backend_pause_ack_resume_preserves_ticks};
    unsigned failures=0,index=0;
    for(auto test:tests) { ++index; try { test(); } catch(const std::exception& e) { ++failures; std::printf("telemetry integration FAIL %u: %s\n",index,e.what()); } }
    std::printf("telemetry_integration: %zu/%zu tests passed (transport stubbed)\n",sizeof(tests)/sizeof(Test)-failures,sizeof(tests)/sizeof(Test));
    return failures?1:0;
}
