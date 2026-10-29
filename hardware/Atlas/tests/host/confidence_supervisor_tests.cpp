#include "ConfidenceSupervisor.h"
#include <cstdio>
using namespace AtlasConfidence;
#define CHECK(x) do { ++n; if(!(x)) { std::printf("supervisor FAIL %d\n",__LINE__); return 1; } } while(0)
static SupervisorParameters parameters() {
    SupervisorParameters p;
    p.motion={2,0.2f,0.2f,0.2f,0.5f}; p.encoder={10000,0.2f,1000000};
    p.pose={0.01f,0.001f,0.01f,0.001f,0.001f,0.001f,0.001f,100000};
    p.ultrasonic=p.imu={20,100,20}; p.cmPerEdge[0]=p.cmPerEdge[1]=0.1f;
    p.trackCm=10; p.clockwiseScale=p.counterclockwiseScale=1;
    p.maximumPositionVariance=100; p.maximumYawVariance=10; p.warningConfidence=0.5f;
    p.historyBias=p.historySlope=1; p.routeDistanceTolerance=10; p.routeYawTolerance=0.5f;
    p.decisionAgeMs=30; p.settleMs=20; p.alignmentUs=40000;
    p.minimumDuty=40; p.maximumDuty=70; p.cautionDuty=50; p.warningSamples=3; p.verificationSamples=2; p.recoveryLimit=2;
    return p;
}
static Evidence evidence(unsigned ms) {
    Evidence e; e.nowMs=ms; e.nowUs=ms*1000; e.encoder.capturedUs=ms*1000;
    e.imu.timestampUs=e.nowUs; e.imu.sequence=ms+1; e.imu.generation=1;
    e.imu.valid=e.imu.calibrated=e.imu.continuous=true; e.imu.quality=1;
    e.configured=e.localSafe=true;
    for(auto& r:e.ranges) r=SensorConfidence::evaluate(true,1,0,{20,100,20});
    return e;
}
int main() {
    unsigned n=0; const auto p=parameters(); CHECK(p.valid()); ConfidenceSupervisor s(p);
    s.update(evidence(0)); s.update(evidence(10)); CHECK(s.decision().permissions&Start);
    CHECK(!s.constraint(100).allowed); CHECK(s.prepare(1,1,10));
    Lifecycle start; start.id=1; start.kind=1; start.target[0]=start.target[1]=10; start.atMs=10;
    s.observe(start);
    auto e=evidence(20); e.blocked=true; e.localSafe=false; e.moving=true; s.update(e);
    CHECK(s.decision().clearance==Clearance::Blocked); CHECK(s.decision().trust!=Trust::Lost);
    CHECK(!(s.decision().permissions&Continue));
    e=evidence(30); e.moving=true; s.update(e); CHECK(s.decision().permissions&Continue);
    e=evidence(40); e.criticalFault=true; s.update(e); CHECK(s.decision().health==Health::Fault);
    CHECK(!s.constraint(40).allowed);
    ConfidenceSupervisor bad({}); bad.update(evidence(10)); CHECK(bad.decision().reasons&Configuration);
    CHECK(!bad.prepare(1,1,10));
    ConfidenceSupervisor lost(p); lost.update(evidence(0)); lost.update(evidence(10));
    e=evidence(20); e.encoder.count[0]=1; lost.update(e); CHECK(lost.decision().trust==Trust::Lost);
    CHECK(!lost.verify(20)); CHECK(lost.decision().permissions&Human);
    CHECK(!(lost.decision().permissions&(Correct|Recover)));
    lost.recoveryAttempt(); lost.recoveryAttempt(); lost.update(evidence(30)); CHECK(lost.decision().reasons&RecoveryLimit);
    // A complete synthetic event with encoder-aligned yaw, settled exactly once.
    ConfidenceSupervisor complete(p); complete.update(evidence(0)); complete.update(evidence(10));
    CHECK(complete.prepare(2,5,10)); complete.observe(start);
    e=evidence(20); e.moving=true; e.encoder.count[0]=e.encoder.count[1]=50; complete.update(e);
    e=evidence(30); e.encoder.count[0]=e.encoder.count[1]=100; complete.update(e);
    auto end=start; end.event=MotionEvent::Complete; end.energized=true; end.atMs=30; complete.observe(end);
    for(unsigned ms=40;ms<=60;ms+=10) { e=evidence(ms); e.encoder.count[0]=e.encoder.count[1]=100; complete.update(e); }
    CHECK(complete.history().size()==1); CHECK(complete.endpointReady());
    auto covariance=complete.estimator().covariance(); complete.checkpoint(true,false,60);
    CHECK(complete.lastCheckpoint().available); CHECK(complete.estimator().covariance().xx==covariance.xx);
    CHECK(!complete.physicalRecoveryEligible());
    CHECK(!complete.constraint(100).allowed);
    CHECK(!(complete.decision().permissions&(Correct|Recover)));
    CHECK(!complete.prepare(2,6,100));
    // Current failures dominate healthy history; healthy samples cannot erase Lost.
    e=evidence(70); e.encoder.count[0]=e.encoder.count[1]=100; e.criticalFault=true; complete.update(e);
    CHECK(complete.decision().health==Health::Fault); CHECK(!complete.constraint(70).allowed);
    ConfidenceSupervisor sameTime(p); sameTime.update(evidence(0)); sameTime.update(evidence(10));
    e=evidence(10); e.encoder.count[0]=1; sameTime.update(e); CHECK(sameTime.decision().trust==Trust::Lost);
    ConfidenceSupervisor rotation(p); rotation.update(evidence(0)); rotation.update(evidence(10));
    e=evidence(30); e.imu.yaw=1; rotation.update(e); CHECK(rotation.decision().trust==Trust::Lost);
    CHECK(rotation.decision().reasons&Heading); rotation.update(evidence(50)); CHECK(!rotation.verify(50));
    ConfidenceSupervisor corrupt(p); corrupt.update(evidence(0)); corrupt.update(evidence(10));
    e=evidence(30); e.imu.yaw=NAN; corrupt.update(e); CHECK(!(corrupt.decision().permissions&Start));
    auto tight=p; tight.maximumPositionVariance=0.000001f; ConfidenceSupervisor uncertainty(tight);
    uncertainty.update(evidence(0)); uncertainty.update(evidence(10)); uncertainty.update(evidence(30));
    CHECK(uncertainty.decision().trust==Trust::Lost); CHECK(uncertainty.decision().reasons&Uncertainty);
    ConfidenceSupervisor immediate(p); immediate.update(evidence(0)); immediate.update(evidence(10));
    CHECK(immediate.constraint(10).allowed); CHECK(immediate.prepare(1,1,10));
    auto invalidStart=start; invalidStart.kind=0; immediate.observe(invalidStart);
    CHECK(!immediate.constraint(10).allowed); CHECK(!immediate.prepare(1,1,10));
    auto gapParameters=p; gapParameters.pose.maximumGapUs=30000;
    ConfidenceSupervisor noDataReady(gapParameters); noDataReady.update(evidence(0)); noDataReady.update(evidence(10));
    CHECK(noDataReady.prepare(1,1,10)); noDataReady.observe(start);
    e=evidence(50); e.imu=evidence(10).imu; noDataReady.update(e); // DRDY stopped, bus still appears connected.
    CHECK(noDataReady.decision().trust==Trust::Lost); CHECK(noDataReady.decision().reasons&Stale);
    CHECK(!noDataReady.constraint(50).allowed);
    std::printf("confidence_supervisor: %u/%u checks passed; state=%zu bytes\n",n,n,sizeof(complete));
}
