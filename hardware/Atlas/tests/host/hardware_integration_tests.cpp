// Real production control/safety/navigation/CCPS. No serial ports or physical I/O.
#include "ConfidenceIntegration.h"
#include <cstdio>
#include <stdexcept>
#include <cmath>
namespace sim {
uint64_t us=1000000;
bool sensorValid=true,pwmOk=true,imuValid=true;
float range=100,yaw=0;
struct Interrupt { void(*handler)(void*)=nullptr; void* context=nullptr; } interrupts[64];
uint32_t duty[64]={};
}
uint32_t millis() { return static_cast<uint32_t>(sim::us/1000); }
int64_t esp_timer_get_time() { return static_cast<int64_t>(sim::us); }
void digitalWrite(int,int) {}
int digitalRead(int) { return LOW; }
void pinMode(int,int) {}
void attachInterruptArg(int pin,void(*handler)(void*),void* context,int) { sim::interrupts[pin]={handler,context}; }
bool ledcAttach(int,uint32_t,uint8_t) { return true; }
bool ledcWrite(int pin,uint32_t duty) { sim::duty[pin]=duty; return sim::pwmOk; }
bool ledcDetach(int) { return true; }
bool SensorManager::begin() { ready_=true; return true; }
void SensorManager::update(uint32_t) {}
SensorManager::Reading SensorManager::reading(AtlasConfig::SensorId,uint32_t now) const {
    Reading r; r.valid=sim::sensorValid; r.distanceCm=sim::range; r.timestampMs=now;
    r.faulted=!r.valid; r.error=r.valid?Error::None:Error::Timeout; return r;
}
bool ImuManager::begin() { return true; }
ImuManager::Reading ImuManager::reading(uint32_t now) const {
    Reading r; r.valid=sim::imuValid; r.timestampMs=now; r.error=r.valid?Error::None:Error::Bus;
    r.navigation.timestampUs=sim::us; r.navigation.sequence=now+1; r.navigation.generation=1;
    r.navigation.yaw=sim::yaw; r.navigation.quality=1;
    r.navigation.valid=r.navigation.calibrated=r.navigation.continuous=sim::imuValid;
    return r;
}
#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); } while(0)
struct Rig {
    MotionController motion; SensorManager sensors; ImuManager imu;
    SafetyController safety{motion,sensors}; CheckpointManager cp;
    Routes routes{motion,sensors,cp}; ObstacleAvoidance obstacles;
    Navigation nav{motion,sensors,safety,routes,cp,obstacles};
    ConfidenceIntegration ccps{motion,sensors,imu,safety,cp,nav};
    Rig() {
        sim::us=1000000; sim::sensorValid=sim::pwmOk=sim::imuValid=true; sim::range=100; sim::yaw=0;
        CHECK(motion.begin()); CHECK(sensors.begin()); safety.begin(millis()); nav.begin(millis()); ccps.begin();
        for(unsigned i=0;i<25;++i) tick();
        CHECK(safety.canMove());
    }
    void tick(uint32_t dt=5) {
        sim::us+=dt*1000; safety.update(millis()); sensors.update(millis()); safety.update(millis());
        ccps.update(millis()); safety.update(millis()); nav.update(millis()); ccps.apply(millis());
        safety.update(millis()); motion.update(millis()); safety.update(millis()); nav.update(millis());
    }
    void pulses(unsigned left,unsigned right) {
        for(unsigned i=0;i<left;++i) sim::interrupts[AtlasConfig::LeftEncoderPin].handler(sim::interrupts[AtlasConfig::LeftEncoderPin].context);
        for(unsigned i=0;i<right;++i) sim::interrupts[AtlasConfig::RightEncoderPin].handler(sim::interrupts[AtlasConfig::RightEncoderPin].context);
    }
    void start(float cm=10) { CHECK(nav.driveDistance(cm)); ccps.apply(millis()); tick(); CHECK(motion.moving()); }
};
void normal_completion() {
    Rig r; r.start();
    for(unsigned i=0;i<8;++i) { r.pulses(6,6); r.tick(); }
    CHECK(r.motion.state()==MotionController::State::Complete);
    for(unsigned i=0;i<100;++i) r.tick();
    CHECK(r.nav.state()==Navigation::State::Completed);
    CHECK(r.ccps.supervisor().history().size()==1); CHECK(r.ccps.supervisor().lastCheckpoint().available);
}
void missing_hooks_deny_start() {
    Rig r; r.nav.executionGate({}); CHECK(!r.nav.driveDistance(10)); CHECK(!r.motion.moving());
}
void supervisor_stop_is_not_estop() {
    Rig r; r.start();
    AtlasConfidence::Constraint deny; r.safety.supervisorConstraint(deny,millis());
    CHECK(!r.motion.moving()); CHECK(r.motion.fault()==AtlasFault::None); CHECK(r.safety.fault()==AtlasFault::None);
}
void emergency_wins_over_allow() {
    Rig r; r.start(); r.safety.emergencyStop();
    r.ccps.apply(millis()); r.safety.update(millis());
    CHECK(r.safety.latched()); CHECK(!r.safety.canMove()); CHECK(r.motion.fault()==AtlasFault::EmergencyStop);
}
void obstacle_not_lost() {
    Rig r; r.start(); sim::range=10; r.tick();
    CHECK(!r.motion.moving()); CHECK(r.safety.fault()==AtlasFault::Obstacle);
    CHECK(r.ccps.supervisor().decision().clearance==AtlasConfidence::Clearance::Blocked);
    CHECK(r.ccps.supervisor().decision().trust!=AtlasConfidence::Trust::Lost);
}
void imu_loss_stops() {
    Rig r; r.start(); sim::imuValid=false; r.tick();
    CHECK(!r.motion.moving()); CHECK(r.ccps.supervisor().decision().trust==AtlasConfidence::Trust::Lost);
    CHECK(!r.nav.verifyConfidence(millis()));
}
void stale_constraint_stops() {
    Rig r; r.start(); sim::us+=51000; r.safety.update(millis());
    CHECK(!r.motion.moving()); CHECK(r.safety.fault()==AtlasFault::ControlOverrun);
}
void sensor_fault_wins() {
    Rig r; r.start(); sim::sensorValid=false; r.tick();
    CHECK(r.safety.fault()==AtlasFault::Sensor); CHECK(r.safety.latched()); CHECK(!r.motion.moving());
}
void encoder_stall_stops() {
    Rig r; r.start(); for(unsigned i=0;i<300&&r.motion.moving();++i) r.tick();
    CHECK(!r.motion.moving()); CHECK(r.safety.fault()==AtlasFault::StallLeft||r.ccps.supervisor().decision().reasons&AtlasConfidence::NoProgress);
}
void external_push_is_not_signed_odometry() {
    Rig r; r.pulses(1,0); r.tick();
    CHECK(r.ccps.supervisor().decision().trust==AtlasConfidence::Trust::Lost); CHECK(!r.safety.canMove());
}
void pause_coast_resume_preserves_logical_movement() {
    Rig r; r.start(); r.pulses(12,12); r.tick(); CHECK(r.nav.pause());
    r.pulses(4,4); for(unsigned i=0;i<60;++i) r.tick();
    CHECK(!r.motion.moving()); CHECK(r.nav.state()==Navigation::State::Paused);
    CHECK(r.ccps.supervisor().history().size()==1);
    CHECK(r.nav.resume()); r.tick();
    for(unsigned i=0;i<8;++i) { r.pulses(4,4); r.tick(); }
    for(unsigned i=0;i<110;++i) r.tick();
    CHECK(r.nav.state()==Navigation::State::Completed);
    const auto& history=r.ccps.supervisor().history(); CHECK(history.size()==2);
    CHECK(history.at(0)->movementId==history.at(1)->movementId); CHECK(history.at(1)->attempt==1);
    CHECK(history.at(0)->outcome==AtlasConfidence::Outcome::Interrupted);
    CHECK(history.at(1)->outcome==AtlasConfidence::Outcome::Complete);
    CHECK(history.summarize(2,0.2f).eligible==1);
}
void local_route_cannot_advance_before_endpoint_approval() {
    Rig r; CHECK(r.routes.beginRecording()); Routes::Segment segment;
    segment.kind=MotionController::Kind::Forward; segment.distanceCm=10;
    CHECK(r.routes.record(segment)); CHECK(r.routes.record(segment)); CHECK(r.routes.finishRecording());
    CHECK(r.nav.replay()); for(unsigned i=0;i<21;++i) r.tick();
    CHECK(r.motion.moving());
    for(unsigned i=0;i<8;++i) { r.pulses(6,6); r.tick(); }
    CHECK(r.routes.index()==0); CHECK(r.motion.state()==MotionController::State::Complete);
    for(unsigned i=0;i<20;++i) r.tick(); CHECK(r.routes.index()==0);
    for(unsigned i=0;i<100&&r.routes.index()==0;++i) r.tick(); CHECK(r.routes.index()==1);
    for(unsigned i=0;i<21;++i) r.tick(); CHECK(r.motion.moving());
    for(unsigned i=0;i<8;++i) { r.pulses(6,6); r.tick(); }
    for(unsigned i=0;i<110;++i) r.tick(); CHECK(r.nav.state()==Navigation::State::Completed);
    CHECK(r.ccps.supervisor().history().size()==2);
}
void one_wheel_already_at_target_can_resume() {
    Rig r; CHECK(r.nav.replayTicks(50,50,MotionController::Kind::Forward)); r.ccps.apply(millis()); r.tick();
    for(unsigned i=0;i<8;++i) { r.pulses(5,5); r.tick(); }
    r.pulses(10,5); r.tick(); CHECK(r.motion.moving()); CHECK(r.nav.pause());
    for(unsigned i=0;i<60;++i) r.tick(); CHECK(r.nav.resume()); r.tick();
    r.pulses(0,5); r.tick(); for(unsigned i=0;i<110;++i) r.tick();
    CHECK(r.nav.state()==Navigation::State::Completed);
}
void duty_cap_is_respected() {
    Rig r; r.start();
    auto c=r.ccps.supervisor().constraint(millis()); c.dutyCeiling=45;
    r.safety.supervisorConstraint(c,millis()); sim::us+=5000; r.safety.update(millis()); r.motion.update(millis());
    CHECK(r.motion.leftDuty()<=45); CHECK(r.motion.rightDuty()<=45);
    c.dutyCeiling=1; r.safety.supervisorConstraint(c,millis()); CHECK(!r.motion.moving());
}
void stale_decision_does_not_become_estop() {
    Rig r; r.start(); auto c=r.ccps.supervisor().constraint(millis());
    c.evaluatedAtMs=millis()-51; r.safety.supervisorConstraint(c,millis());
    CHECK(!r.motion.moving()); CHECK(r.motion.fault()==AtlasFault::None);
}
void target_overshoot_remains_a_local_fault() {
    Rig r; CHECK(r.nav.replayTicks(50,50,MotionController::Kind::Forward)); r.ccps.apply(millis()); r.tick();
    r.pulses(51,51); r.tick(); CHECK(r.safety.fault()==AtlasFault::EncoderTarget); CHECK(r.safety.latched());
}
void pwm_failure_remains_a_local_fault() {
    Rig r; r.start(); sim::pwmOk=false; r.tick(); CHECK(r.safety.fault()==AtlasFault::Pwm); CHECK(!r.motion.moving());
    sim::pwmOk=true;
}
void explicit_safety_reset_never_clears_lost() {
    Rig r; r.pulses(1,0); r.tick(); CHECK(r.ccps.supervisor().decision().trust==AtlasConfidence::Trust::Lost);
    CHECK(r.nav.resetSafety(millis())); r.tick(); CHECK(!r.safety.canMove());
    CHECK(r.ccps.supervisor().decision().trust==AtlasConfidence::Trust::Lost);
}
void cancel_before_output_does_not_add_a_movement() {
    Rig r; CHECK(r.nav.driveDistance(10)); r.nav.cancel();
    for(unsigned i=0;i<60;++i) r.tick(); CHECK(r.ccps.supervisor().history().size()==0);
}
void relative_90_degree_turn_uses_synchronized_evidence() {
    Rig r; CHECK(r.nav.turn90(true)); r.ccps.apply(millis()); r.tick();
    unsigned counts=0;
    for(unsigned i=0;i<9;++i) {
        counts+=4; r.pulses(4,4);
        sim::yaw=-2.0f*counts*(AtlasConfidence::Pi*AtlasConfig::WheelDiameterCm/100)/AtlasConfig::WheelTrackCm;
        r.tick();
        if(!r.motion.moving()) {
            const auto& d=r.ccps.supervisor().decision();
            std::printf("turn stopped at ticks=%u state=%u local=%u CCPS=0x%lx trust=%u\n",counts,
                static_cast<unsigned>(r.motion.state()),static_cast<unsigned>(r.safety.fault()),
                static_cast<unsigned long>(d.reasons),static_cast<unsigned>(d.trust));
        }
        CHECK(r.motion.moving());
    }
    ++counts; r.pulses(1,1); sim::yaw=-2.0f*counts*(AtlasConfidence::Pi*AtlasConfig::WheelDiameterCm/100)/AtlasConfig::WheelTrackCm;
    r.tick(); CHECK(r.motion.state()==MotionController::State::Complete);
    for(unsigned i=0;i<110;++i) r.tick();
    CHECK(r.nav.state()==Navigation::State::Completed); CHECK(r.ccps.supervisor().history().size()==1);
    CHECK(r.ccps.supervisor().decision().trust!=AtlasConfidence::Trust::Lost);
}
void prestart_push_is_not_commanded_progress() {
    Rig r; r.pulses(1,1); // after last evidence snapshot, before the new command.
    CHECK(r.nav.driveDistance(10)); r.ccps.apply(millis());
    CHECK(!r.motion.moving()); CHECK(r.motion.leftDuty()==0&&r.motion.rightDuty()==0);
    r.tick(); CHECK(r.ccps.supervisor().decision().trust==AtlasConfidence::Trust::Lost);
}
void late_coast_keeps_old_direction_across_command_boundary() {
    Rig r; r.start(); r.pulses(12,12); r.tick(); CHECK(r.nav.pause());
    for(unsigned i=0;i<39;++i) r.tick();
    r.pulses(1,1); r.tick(); CHECK(r.ccps.supervisor().history().size()==1);
    r.nav.cancel(); CHECK(r.nav.driveDistance(-10)); r.ccps.apply(millis()); r.tick();
    for(unsigned i=0;i<8;++i) { r.pulses(6,6); r.tick(); }
    for(unsigned i=0;i<110;++i) r.tick();
    CHECK(r.nav.state()==Navigation::State::Completed);
    const float expected=(13.0f-48.0f)*(AtlasConfidence::Pi*AtlasConfig::WheelDiameterCm/100);
    CHECK(fabsf(r.ccps.supervisor().estimator().pose().x-expected)<0.0001f);
}
int main() {
    using Test=void(*)(); const Test tests[]={normal_completion,missing_hooks_deny_start,supervisor_stop_is_not_estop,
        emergency_wins_over_allow,obstacle_not_lost,imu_loss_stops,stale_constraint_stops,sensor_fault_wins,encoder_stall_stops,external_push_is_not_signed_odometry,
        pause_coast_resume_preserves_logical_movement,local_route_cannot_advance_before_endpoint_approval,one_wheel_already_at_target_can_resume,
        duty_cap_is_respected,stale_decision_does_not_become_estop,target_overshoot_remains_a_local_fault,pwm_failure_remains_a_local_fault,
        explicit_safety_reset_never_clears_lost,cancel_before_output_does_not_add_a_movement,relative_90_degree_turn_uses_synchronized_evidence,
        prestart_push_is_not_commanded_progress,late_coast_keeps_old_direction_across_command_boundary};
    unsigned failures=0,index=0;
    for(auto test:tests) { ++index; try { test(); } catch(const std::exception& e) { ++failures; std::printf("hardware integration FAIL %u: %s\n",index,e.what()); } }
    std::printf("hardware_integration: %zu/%zu tests passed\n",sizeof(tests)/sizeof(Test)-failures,sizeof(tests)/sizeof(Test));
    return failures?1:0;
}
