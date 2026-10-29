// AUDIT ONLY. Ten failure classes plus independent signed-history checks.
#include "supervisor_fixture.inc"
#undef CHECK
#include <stdexcept>
static void check(bool value) { if(!value) throw std::runtime_error("failed failure-response invariant"); }
static ConfidenceSupervisor running() {
    ConfidenceSupervisor s(parameters()); s.update(evidence(0)); s.update(evidence(10)); check(s.prepare(1,1,10));
    Lifecycle l; l.id=1; l.kind=1; l.target[0]=l.target[1]=10; l.atMs=10; s.observe(l); return s;
}
static void score_collapse() {
    auto s=running(); auto e=evidence(30); e.imu.quality=0; s.update(e);
    check(!s.constraint(30).allowed&&s.decision().trust==Trust::Lost&&(s.decision().permissions&Human));
}
static void range_missing() {
    auto s=running(); auto e=evidence(30); e.ranges[0]=Assessment{}; s.update(e);
    check(!s.constraint(30).allowed&&(s.decision().reasons&Missing));
}
static void imu_missing() {
    auto s=running(); auto e=evidence(30); e.imu.valid=false; s.update(e);
    check(!s.constraint(30).allowed&&s.decision().trust==Trust::Lost);
}
static void pulse_rate() {
    auto s=running(); auto e=evidence(30); e.encoder.count[0]=1000; s.update(e);
    check(!s.constraint(30).allowed&&(s.decision().reasons&PulseRate)&&s.decision().trust==Trust::Lost);
}
static void covariance_ceiling() {
    auto p=parameters(); p.maximumPositionVariance=1e-7f; ConfidenceSupervisor s(p);
    s.update(evidence(0)); s.update(evidence(10)); s.update(evidence(30));
    check(!s.constraint(30).allowed&&(s.decision().reasons&Uncertainty)&&(s.decision().permissions&Human));
}
static void encoder_gyro_conflict() {
    auto s=running(); auto e=evidence(30); e.encoder.count[0]=e.encoder.count[1]=25; e.imu.yaw=0.5f; s.update(e);
    check(!s.constraint(30).allowed&&(s.decision().reasons&EncoderImu));
}
static void repeated_checkpoint_rejection() {
    ConfidenceSupervisor s(parameters()); s.update(evidence(0)); s.update(evidence(10));
    for(unsigned ms=20;ms<=40;ms+=10) { s.checkpoint(false,true,ms); s.update(evidence(ms)); }
    check(!s.constraint(40).allowed&&(s.decision().reasons&Checkpoint)&&s.decision().trust==Trust::Verify);
}
static void local_critical_fault() {
    auto s=running(); auto e=evidence(30); e.criticalFault=true; s.update(e);
    check(!s.constraint(30).allowed&&s.decision().health==Health::Fault&&(s.decision().reasons&LocalSafety));
}
static void stale_imu() {
    auto s=running(); auto e=evidence(200); e.imu=evidence(10).imu; s.update(e);
    check(!s.constraint(200).allowed&&(s.decision().reasons&Stale)&&s.decision().trust==Trust::Lost);
}
static void invalid_configuration() {
    auto s=running(); auto e=evidence(30); e.configured=false; s.update(e);
    check(!s.constraint(30).allowed&&(s.decision().reasons&Configuration));
}
static void signed_history_and_invalid_attempt() {
    MovementHistory under,over;
    for(unsigned i=1;i<=11;++i) {
        MovementRecord r; r.movementId=i; r.flags=MeasurementValid|ResidualValid;
        r.outcome=Outcome::Complete; r.motionConfidence=0.7f; r.distanceResidualCm=-0.1f*i; r.yawResidualRad=0.01f*i;
        under.push(r); r.distanceResidualCm=-r.distanceResidualCm; over.push(r);
    }
    auto a=under.summarize(2,0.2f),b=over.summarize(2,0.2f);
    check(a.valid&&a.eligible==10&&a.distanceBias<0&&b.distanceBias>0&&a.yawBias>0&&a.residualSlope>0);
    MovementRecord interruption; interruption.movementId=11; interruption.outcome=Outcome::Interrupted;
    interruption.flags=ObstacleInterruption; under.push(interruption); a=under.summarize(2,0.2f);
    check(a.eligible==8&&a.obstacles==1&&a.interruptions==1);
}
int main() {
    struct Test { const char* name; void(*run)(); };
    const Test tests[]={{"score_collapse",score_collapse},{"required_range_missing",range_missing},
        {"imu_missing",imu_missing},{"implausible_encoder_rate",pulse_rate},{"uncertainty_limit",covariance_ceiling},
        {"encoder_gyro_conflict",encoder_gyro_conflict},{"repeated_checkpoint_rejection",repeated_checkpoint_rejection},
        {"local_critical_fault",local_critical_fault},{"stale_evidence",stale_imu},{"invalid_configuration",invalid_configuration},
        {"signed_history_bias_trend_attempt",signed_history_and_invalid_attempt}};
    unsigned passed=0;
    for(const auto& t:tests) { try { t.run(); ++passed; std::printf("PASS %s\n",t.name); }
        catch(const std::exception& e) { std::printf("FAIL %s: %s\n",t.name,e.what()); } }
    std::printf("AUDIT matrix: %u/%zu scenarios pass\n",passed,sizeof(tests)/sizeof(Test));
    return passed==sizeof(tests)/sizeof(Test)?0:1;
}
