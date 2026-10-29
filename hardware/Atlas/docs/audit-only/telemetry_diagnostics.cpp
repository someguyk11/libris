// AUDIT ONLY: coordinator fixture; no real backend, worker or serial connection.
#include "telemetry_fixture.inc"
static void completion_after_coast() {
    BackendRig r; r.startBackend();
    for(unsigned i=0;i<10;++i) { r.pulses(5,5); r.tickBackend(); }
    CHECK(r.motion.state()==MotionController::State::Complete); r.pulses(1,1);
    for(unsigned i=0;i<120;++i) r.tickBackend();
    auto result=TelemetryHarness::take(r.telemetry);
    std::printf("backend coast: status=%u ticks=%lu/%lu\n",static_cast<unsigned>(result.status),
        static_cast<unsigned long>(result.leftTicks),static_cast<unsigned long>(result.rightTicks));
    CHECK(result.status==BackendClient::ResultStatus::Failed);
}
static void lost_failure_is_copied() {
    BackendRig r; r.startBackend(); sim::imuValid=false; r.tickBackend();
    CHECK(!r.motion.moving());
    // The coordinator preserves a terminal settling interval before reporting.
    for(unsigned i=0;i<60;++i) r.tickBackend();
    auto result=TelemetryHarness::take(r.telemetry);
    CHECK(!r.motion.moving()); CHECK(result.status==BackendClient::ResultStatus::Failed);
    CHECK(result.confidence.trust==AtlasConfidence::Trust::Lost);
    CHECK(result.confidence.permissions&AtlasConfidence::Human);
    hostNetwork::connected=true; TelemetryHarness::ready(r.telemetry);
    for(unsigned i=0;i<20;++i) r.tickBackend();
    CHECK(!r.motion.moving()); CHECK(r.ccps.supervisor().decision().trust==AtlasConfidence::Trust::Lost);
}
int main() {
    unsigned passed=0;
    try { completion_after_coast(); ++passed; std::printf("PASS backend_coast_reject\n"); }
    catch(const std::exception& e) { std::printf("FAIL backend_coast_reject: %s\n",e.what()); }
    try { lost_failure_is_copied(); ++passed; std::printf("PASS lost_failure_copy_reconnect\n"); }
    catch(const std::exception& e) { std::printf("FAIL lost_failure_copy_reconnect: %s\n",e.what()); }
    std::printf("AUDIT telemetry: %u/2 scenarios pass\n",passed); return passed==2?0:1;
}
