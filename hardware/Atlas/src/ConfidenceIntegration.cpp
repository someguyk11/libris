#include "ConfidenceIntegration.h"
#include <esp_timer.h>
using namespace AtlasConfidence;
SupervisorParameters ConfidenceIntegration::parameters() {
    using namespace AtlasConfig;
    auto p=ConfidenceCalibration;
    if(AtlasConfig::positive(LeftCountsPerWheelRevolution)) p.cmPerEdge[0]=Pi*WheelDiameterCm/LeftCountsPerWheelRevolution;
    if(AtlasConfig::positive(RightCountsPerWheelRevolution)) p.cmPerEdge[1]=Pi*WheelDiameterCm/RightCountsPerWheelRevolution;
    p.trackCm=WheelTrackCm; p.clockwiseScale=ClockwiseTurnScale; p.counterclockwiseScale=CounterclockwiseTurnScale;
    p.minimumDuty=MinimumDriveDuty; p.maximumDuty=MaxDuty;
    p.ultrasonic={SensorCount*SensorSlotIntervalMs,MaximumReadingAgeMs,static_cast<float>(MaximumReadingAgeMs)};
    p.imu={ImuSampleIntervalMs,ImuMaximumAgeMs,static_cast<float>(ImuMaximumAgeMs)};
    p.settleMs=CheckpointSettleMs; p.decisionAgeMs=MaximumControlGapMs;
    p.encoder.noProgressUs=StallTimeoutMs*1000;
    return p;
}
ConfidenceIntegration::ConfidenceIntegration(MotionController& m,SensorManager& s,ImuManager& i,SafetyController& safe,CheckpointManager& c,Navigation& nav)
    :motion_(m),sensors_(s),imu_(i),safety_(safe),checkpoints_(c),navigation_(nav),supervisor_(parameters()) {}
void ConfidenceIntegration::begin() {
    motion_.observe(event,this);
    ExecutionGate gate; gate.context=this;
    gate.authorize=[](void* self,uint32_t context,uint32_t step,uint32_t now) {
        auto& c=*static_cast<ConfidenceIntegration*>(self); c.context(context,step);
        return AtlasConfig::ConfidenceObservationOnly||c.supervisor_.prepare(context,step,now);
    };
    gate.endpoint=[](void* self) { return AtlasConfig::ConfidenceObservationOnly||static_cast<ConfidenceIntegration*>(self)->supervisor_.endpointReady(); };
    gate.settling=[](void* self) { return static_cast<ConfidenceIntegration*>(self)->supervisor_.settling(); };
    gate.verification=[](void* self,uint32_t now) { return static_cast<ConfidenceIntegration*>(self)->supervisor_.verify(now); };
    gate.checkpoint=[](void* self,bool accepted,bool rejected,uint32_t now) {
        auto& c=*static_cast<ConfidenceIntegration*>(self); c.supervisor_.checkpoint(accepted,rejected,now);
        return AtlasConfig::ConfidenceObservationOnly||c.supervisor_.endpointReady();
    };
    navigation_.executionGate(gate);
}
void ConfidenceIntegration::event(void* context,const MotionController::Event& e) {
    auto& self=*static_cast<ConfidenceIntegration*>(context);
    Lifecycle l; l.id=e.movementId; l.atMs=e.timestampMs; l.attempt=e.attempt;
    l.kind=static_cast<uint8_t>(e.progress.kind); l.fault=static_cast<uint8_t>(e.fault);
    l.event=static_cast<MotionEvent>(e.event); l.energized=e.energized; l.obstacle=e.fault==AtlasFault::Obstacle;
    l.target[0]=e.progress.leftTargetCm; l.target[1]=e.progress.rightTargetCm;
    l.travel[0]=e.progress.leftCm; l.travel[1]=e.progress.rightCm;
    for(unsigned i=0;i<2;++i) l.count[i]=e.encoder.count[i];
    if(AtlasConfig::ConfidenceObservationOnly&&(l.event==MotionEvent::Start||l.event==MotionEvent::Resume))
        self.supervisor_.prepare(self.context_,self.step_,e.timestampMs);
    self.supervisor_.observe(l);
}
void ConfidenceIntegration::update(uint32_t now) {
    if(sampled_&&now-lastUpdateMs_<AtlasConfig::ControlIntervalMs) { apply(now); return; }
    lastUpdateMs_=now;
    const uint64_t started=esp_timer_get_time();
    Evidence e; e.encoder=motion_.encoderSnapshot();
    imu_.setStationary(!motion_.moving()&&sampled_&&previous_.count[0]==e.encoder.count[0]&&previous_.count[1]==e.encoder.count[1]);
    previous_=e.encoder; sampled_=true;
    const auto imu=imu_.reading(now); e.imu=imu.navigation;
    e.configured=AtlasConfig::distanceCalibrated()&&AtlasConfig::turnsCalibrated()&&AtlasConfig::ImuNavigationParameters.valid();
    e.localSafe=safety_.localCanMove(); e.blocked=safety_.fault()==AtlasFault::Obstacle; e.moving=motion_.moving();
    const auto fault=safety_.fault()!=AtlasFault::None?safety_.fault():motion_.fault();
    e.criticalFault=fault!=AtlasFault::None&&fault!=AtlasFault::Obstacle&&fault!=AtlasFault::Sensor;
    for(unsigned i=0;i<4;++i) {
        const auto r=sensors_.reading(static_cast<AtlasConfig::SensorId>(i),now);
        e.ranges[i]=SensorConfidence::ultrasonic(r.valid,r.faulted,r.consecutiveFailures,now-r.timestampMs,
            {AtlasConfig::SensorCount*AtlasConfig::SensorSlotIntervalMs,AtlasConfig::MaximumReadingAgeMs,static_cast<float>(AtlasConfig::MaximumReadingAgeMs)});
    }
    // Sample the clock AFTER queue/sensor handoffs. The IMU worker may publish
    // between the encoder snapshot and queue peek; that is not future evidence.
    e.nowUs=esp_timer_get_time(); e.nowMs=millis();
    supervisor_.update(e);
    if(!AtlasConfig::ConfidenceObservationOnly) {
        const bool ready=supervisor_.endpointReady();
        checkpoints_.navigationEvidence(ready,ready,supervisor_.estimator().healthy());
    }
    apply(e.nowMs);
    const uint32_t elapsed=static_cast<uint32_t>(esp_timer_get_time()-started);
    if(elapsed>maximumSliceUs_) maximumSliceUs_=elapsed;
}
void ConfidenceIntegration::apply(uint32_t now) {
    if(!AtlasConfig::ConfidenceObservationOnly) safety_.supervisorConstraint(supervisor_.constraint(now),now);
}
