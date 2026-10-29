// AUDIT ONLY: unchanged real control stack with the original hardware test double.
#define main audit_original_hardware_main
#include "hardware_integration_tests.cpp"
#undef main
#include <algorithm>
static void exact_ticks_coast() {
    Rig r; CHECK(r.nav.replayTicks(50,50,MotionController::Kind::Forward)); r.ccps.apply(millis()); r.tick();
    for(unsigned i=0;i<10;++i) { r.pulses(5,5); r.tick(); }
    CHECK(r.motion.state()==MotionController::State::Complete);
    r.pulses(1,1); for(unsigned i=0;i<120;++i) r.tick();
    std::printf("coast exact ticks: nav=%u fault=%u record ticks=%lu/%lu\n",static_cast<unsigned>(r.nav.state()),
        static_cast<unsigned>(r.safety.fault()),static_cast<unsigned long>(r.ccps.supervisor().history().at(0)->leftTicks),
        static_cast<unsigned long>(r.ccps.supervisor().history().at(0)->rightTicks));
    CHECK(r.nav.state()!=Navigation::State::Completed);
}
static void local_reset_does_not_clear_lost() {
    Rig r; r.pulses(1,1); r.tick(); CHECK(r.ccps.supervisor().decision().trust==AtlasConfidence::Trust::Lost);
    CHECK(r.nav.resetSafety(millis())); r.nav.cancel(); for(unsigned i=0;i<100;++i) r.tick();
    CHECK(!r.nav.driveDistance(10)); CHECK(!r.nav.verifyConfidence(millis())); CHECK(!r.motion.moving());
}
static void reverse_asymmetric_ticks() {
    Rig r; CHECK(r.nav.replayTicks(50,58,MotionController::Kind::Backward)); r.ccps.apply(millis()); r.tick();
    unsigned l=0,rr=0;
    while(l<50||rr<58) {
        unsigned dl=std::min(5u,50-l),dr=std::min(6u,58-rr); l+=dl; rr+=dr; r.pulses(dl,dr);
        sim::yaw=(static_cast<float>(l)-rr)*(AtlasConfidence::Pi*AtlasConfig::WheelDiameterCm/100)/AtlasConfig::WheelTrackCm;
        r.tick(); if(!r.motion.moving()&&(l<50||rr<58)) break;
    }
    for(unsigned i=0;i<120;++i) r.tick();
    std::printf("reverse asym ticks: nav=%u reasons=0x%lx\n",static_cast<unsigned>(r.nav.state()),
        static_cast<unsigned long>(r.ccps.supervisor().decision().reasons));
    CHECK(r.nav.state()==Navigation::State::Completed);
}
static void long_obstacle_retains_trust() {
    Rig r; r.start(); sim::range=10; for(unsigned i=0;i<70;++i) r.tick();
    CHECK(r.ccps.supervisor().decision().trust!=AtlasConfidence::Trust::Lost);
    CHECK(r.ccps.supervisor().decision().clearance==AtlasConfidence::Clearance::Blocked);
    CHECK(!r.motion.moving()); CHECK(!r.nav.resume());
}
static void persistent_required_sensor_human() {
    Rig r; r.start(); sim::sensorValid=false;
    for(unsigned i=0;i<500;++i) r.tick();
    const auto& d=r.ccps.supervisor().decision();
    std::printf("persistent sensor: trust=%u permit=%u reasons=0x%lx\n",static_cast<unsigned>(d.trust),d.permissions,static_cast<unsigned long>(d.reasons));
    CHECK(!r.motion.moving()); CHECK(d.permissions&AtlasConfidence::Human);
}
int main() {
    using Test=void(*)(); const Test tests[]={exact_ticks_coast,local_reset_does_not_clear_lost,reverse_asymmetric_ticks,
        long_obstacle_retains_trust,persistent_required_sensor_human};
    const char* names[]={"exact_tick_coast_rejection","lost_reset_denied","reverse_asymmetric_ticks","blocked_retains_trust","persistent_sensor_human"};
    unsigned passed=0;
    for(unsigned i=0;i<5;++i) { try { tests[i](); ++passed; std::printf("PASS %s\n",names[i]); }
        catch(const std::exception& e) { std::printf("FAIL %s: %s\n",names[i],e.what()); } }
    std::printf("AUDIT integration: %u/5 scenarios pass\n",passed); return passed==5?0:1;
}
