#include "ConfidenceSupervisor.h"
namespace AtlasConfidence {
bool SupervisorParameters::valid() const {
    return motion.valid()&&pose.valid()&&positive(encoder.maximumEdgesPerSecond)&&positive(encoder.progressMismatch)&&encoder.noProgressUs&&
        positive(cmPerEdge[0])&&positive(cmPerEdge[1])&&positive(trackCm)&&positive(clockwiseScale)&&positive(counterclockwiseScale)&&
        positive(maximumPositionVariance)&&positive(maximumYawVariance)&&positive(warningConfidence)&&warningConfidence<1&&
        positive(historyBias)&&positive(historySlope)&&decisionAgeMs&&decisionAgeMs<0x80000000u&&settleMs&&alignmentUs&&
        alignmentUs<0x80000000u&&encoder.noProgressUs<0x80000000u&&pose.maximumGapUs>=20000&&pose.maximumGapUs<0x80000000u&&
        positive(routeDistanceTolerance)&&positive(routeYawTolerance)&&
        minimumDuty&&maximumDuty>=minimumDuty&&cautionDuty>=minimumDuty&&cautionDuty<=maximumDuty&&warningSamples&&verificationSamples&&recoveryLimit&&
        freshness(0,ultrasonic)>0&&freshness(0,imu)>0;
}
ConfidenceSupervisor::ConfidenceSupervisor(const SupervisorParameters& p):p_(p),pose_(p.pose) {}
void ConfidenceSupervisor::lose(uint32_t reasons) { lost_=hold_=true; latchedReasons_|=reasons; terminalAccepted_=false; pose_.invalidate(); }
bool ConfidenceSupervisor::align(const ImuEvidence& imu,Aligned& a) const {
    if(sampleCount_<2) return false;
    const uint32_t t=static_cast<uint32_t>(imu.timestampUs);
    for(uint8_t i=1;i<sampleCount_;++i) {
        const auto& l=samples_[(nextSample_+8-sampleCount_+i-1)%8];
        const auto& r=samples_[(nextSample_+8-sampleCount_+i)%8];
        const uint32_t span=r.capturedUs-l.capturedUs,offset=t-l.capturedUs;
        if(!span||span>p_.alignmentUs||offset>span||r.spanUs>span||l.spanUs>span) continue;
        const float phase=static_cast<float>(offset)/span;
        a.imu=imu;
        // Preserve fractional ticks without converting long-lived absolute counters to float.
        for(unsigned side=0;side<2;++side) {
            const uint32_t delta=r.count[side]-l.count[side];
            if(delta>p_.encoder.maximumEdgesPerSecond*(span*1e-6f)+1) return false;
            const float ticks=delta*phase;
            a.count[side]=l.count[side]+static_cast<uint32_t>(ticks);
            a.fractionSide[side]=ticks-floorf(ticks);
        }
        return true;
    }
    return false;
}
void ConfidenceSupervisor::integrate(const Evidence& e,bool force) {
    if(lost_) return;
    if(!e.imu.valid||!e.imu.calibrated||!e.imu.continuous) {
        if(haveAligned_&&(active_||e.imu.gaps)) lose(Discontinuity|Missing);
        return;
    }
    if(haveAligned_&&e.imu.sequence==aligned_.imu.sequence) return;
    Aligned next;
    if(!align(e.imu,next)) {
        if(haveAligned_&&e.nowUs>aligned_.imu.timestampUs+p_.alignmentUs) lose(Discontinuity);
        return;
    }
    if(haveAligned_) {
        if(next.imu.generation!=aligned_.imu.generation||next.imu.timestampUs<=aligned_.imu.timestampUs) { lose(Discontinuity); return; }
        motionAligned_=next; latestImu_=next.imu;
        if(!force&&next.imu.timestampUs-aligned_.imu.timestampUs<20000) return;
        float travel[2];
        for(unsigned i=0;i<2;++i) travel[i]=(static_cast<float>(next.count[i]-aligned_.count[i])+next.fractionSide[i]-aligned_.fractionSide[i])*p_.cmPerEdge[i];
        if(!active_&&(travel[0]!=0||travel[1]!=0)) { lose(UnexpectedMotion); return; }
        const uint8_t kind=start_.kind;
        const float left=travel[0]*((kind==2||kind==4)?-1:1);
        const float right=travel[1]*((kind==2||kind==3)?-1:1);
        const uint64_t elapsed=next.imu.timestampUs-aligned_.imu.timestampUs;
        if(elapsed>UINT32_MAX||!pose_.advance((left+right)*0.5f,yawDelta(next.imu,aligned_.imu),left,right,
            static_cast<uint32_t>(elapsed),++poseSequence_)) lose(Numerical|Discontinuity);
    }
    aligned_=motionAligned_=next; latestImu_=next.imu; haveAligned_=true;
}
MotionEvidence ConfidenceSupervisor::movement(const Evidence& e,bool terminal,bool logical) const {
    MotionEvidence m;
    const auto& origin=logical?logicalStart_:start_;
    const auto& imuOrigin=logical?logicalImu_:startImu_;
    float travel[2];
    for(unsigned i=0;i<2;++i) {
        const float ticks=terminal?static_cast<float>(e.encoder.count[i]-origin.count[i]):
            static_cast<float>(motionAligned_.count[i]-origin.count[i])+motionAligned_.fractionSide[i];
        travel[i]=ticks*p_.cmPerEdge[i];
    }
    const uint8_t kind=origin.kind;
    const float leftSign=(kind==2||kind==4)?-1:1,rightSign=(kind==2||kind==3)?-1:1;
    m.left=travel[0]; m.right=travel[1];
    m.leftTarget=fmaxf(0,origin.target[0]-origin.travel[0]); m.rightTarget=fmaxf(0,origin.target[1]-origin.travel[1]);
    const bool turn=kind==3||kind==4;
    m.expectedDistance=turn?0:leftSign*(m.leftTarget+m.rightTarget)*0.5f;
    m.measuredDistance=(leftSign*travel[0]+rightSign*travel[1])*0.5f;
    const float remaining=0.5f*(m.leftTarget/origin.target[0]+m.rightTarget/origin.target[1]);
    m.expectedYaw=turn?(kind==3?-Pi/2:Pi/2)*remaining:(m.rightTarget-m.leftTarget)/p_.trackCm;
    m.measuredYaw=yawDelta(latestImu_,imuOrigin);
    const float scale=turn?(kind==3?p_.clockwiseScale:p_.counterclockwiseScale):1;
    m.encoderYaw=(rightSign*travel[1]-leftSign*travel[0])/(p_.trackCm*scale);
    m.measurementConfidence=decision_.sensor;
    m.continuous=haveAligned_&&!lost_&&latestImu_.generation==imuOrigin.generation;
    m.terminal=terminal; m.interrupted=pending_&&terminal_.event!=MotionEvent::Complete;
    m.fault=pending_&&!terminal_.obstacle?terminal_.fault:0;
    return m;
}
void ConfidenceSupervisor::finish(const Evidence& e) {
    const auto m=movement(e,true); motion_=MotionConfidence::evaluate(m,p_.motion);
    if(terminal_.event==MotionEvent::Complete&&start_.attempt) {
        const auto logical=MotionConfidence::evaluate(movement(e,true,true),p_.motion);
        motion_.accepted=motion_.accepted&&logical.accepted;
        motion_.reasons|=logical.reasons;
        motion_.confidence=fminf(motion_.confidence,logical.confidence);
    }
    MovementRecord r; r.movementId=start_.id; r.routeContext=context_; r.routeStep=step_; r.attempt=start_.attempt;
    r.durationMs=e.nowMs-start_.atMs; r.leftTicks=e.encoder.count[0]-start_.count[0]; r.rightTicks=e.encoder.count[1]-start_.count[1];
    r.expectedDistanceCm=m.expectedDistance; r.measuredDistanceCm=m.measuredDistance;
    r.expectedYawRad=m.expectedYaw; r.measuredYawRad=m.measuredYaw; r.encoderYawRad=m.encoderYaw;
    r.motionConfidence=motion_.confidence; r.distanceResidualCm=motion_.distanceResidual; r.yawResidualRad=motion_.yawResidual;
    r.reasonBits=static_cast<uint16_t>(motion_.reasons); r.kind=start_.kind; r.fault=terminal_.fault;
    r.outcome=terminal_.event==MotionEvent::Complete?Outcome::Complete:terminal_.fault&&!terminal_.obstacle?Outcome::Fault:Outcome::Interrupted;
    if(motion_.usable) r.flags|=MeasurementValid;
    if(r.outcome==Outcome::Complete) r.flags|=ResidualValid;
    if(terminal_.obstacle) r.flags|=ObstacleInterruption;
    if(motion_.reasons&EncoderImu) r.flags|=EncoderDisagreement;
    if(terminal_.energized||r.leftTicks||r.rightTicks) { history_.push(r); historySummary_=history_.summarize(p_.motion.distance,p_.motion.yaw); }
    terminalAccepted_=motion_.accepted;
    if(!motion_.accepted) { hold_=true; latchedReasons_|=motion_.reasons; }
    // Nominal progression is command-derived, never a measurement/covariance reset.
    const float remainingDistance=m.leftTarget+m.rightTarget;
    const float phase=m.interrupted&&remainingDistance>0?fminf(1,(m.left+m.right)/remainingDistance):1;
    const float angle=m.expectedYaw*phase, distance=m.expectedDistance*phase;
    nominal_.x+=distance*cosf(nominal_.yaw+angle/2); nominal_.y+=distance*sinf(nominal_.yaw+angle/2); nominal_.yaw=wrap(nominal_.yaw+angle);
    if(terminal_.event==MotionEvent::Complete) nominal_=logicalGoal_;
    const auto actual=pose_.pose();
    const float position=hypotf(actual.x-nominal_.x,actual.y-nominal_.y)/p_.routeDistanceTolerance;
    const float heading=wrap(actual.yaw-nominal_.yaw)/p_.routeYawTolerance;
    routeScore_=expf(-0.5f*(position*position+heading*heading));
    if(!isfinite(routeScore_)||position>1||fabsf(heading)>1) lose(Checkpoint|Uncertainty);
    active_=pending_=false; prepared_=false;
}
void ConfidenceSupervisor::observe(const Lifecycle& e) {
    if(e.event==MotionEvent::BeforeReset) return; // terminal snapshot already retained before state clearing.
    if(e.event==MotionEvent::Start||e.event==MotionEvent::Resume) {
        if(active_||pending_||!prepared_||e.atMs-preparedAtMs_>p_.decisionAgeMs||
           !positive(e.target[0])||!positive(e.target[1])||e.kind<1||e.kind>4) { lose(Context); return; }
        if(sampleCount_) {
            const auto& previous=samples_[(nextSample_+7)%8];
            if(e.count[0]!=previous.count[0]||e.count[1]!=previous.count[1]) lose(UnexpectedMotion);
        }
        start_=e; startImu_=latestImu_; active_=true; terminalAccepted_=false; prepared_=false;
        if(e.event==MotionEvent::Start) {
            logicalStart_=e; logicalImu_=latestImu_; logicalGoal_=nominal_;
            const float turn=e.kind==3?-Pi/2:e.kind==4?Pi/2:0;
            const float distance=e.kind<3?(e.kind==2?-1:1)*(e.target[0]+e.target[1])*0.5f:0;
            logicalGoal_.x+=distance*cosf(nominal_.yaw+turn/2);
            logicalGoal_.y+=distance*sinf(nominal_.yaw+turn/2); logicalGoal_.yaw=wrap(nominal_.yaw+turn);
        } else if(e.id!=logicalStart_.id) lose(Context);
        if(!haveAligned_) lose(Missing);
    } else if(active_&&!pending_&&e.id==start_.id&&e.attempt==start_.attempt) {
        terminal_=e; pending_=true;
    }
}
void ConfidenceSupervisor::update(const Evidence& e) {
    const EncoderSnapshot previous=sampleCount_?samples_[(nextSample_+7)%8]:e.encoder;
    const bool newSnapshot=!sampleCount_||e.encoder.capturedUs!=samples_[(nextSample_+7)%8].capturedUs;
    if(newSnapshot) { samples_[nextSample_]=e.encoder; nextSample_=(nextSample_+1)%8; if(sampleCount_<8) ++sampleCount_; }
    uint32_t reasons=latchedReasons_;
    bool ready=p_.valid()&&e.configured;
    if(!ready) reasons|=Configuration;
    if(p_.valid()) {
        if(static_cast<uint32_t>(e.nowUs)-e.encoder.capturedUs>p_.alignmentUs) { ready=false; reasons|=Stale; }
        if(!newSnapshot&&(e.encoder.count[0]!=previous.count[0]||e.encoder.count[1]!=previous.count[1])) lose(Discontinuity);
    }
    const auto imu=SensorConfidence::imu(e.imu,e.nowUs,p_.imu);
    const bool imuTimely=e.nowUs>=e.imu.timestampUs&&e.nowUs-e.imu.timestampUs<=p_.pose.maximumGapUs;
    float sensor=imu.confidence;
    if(!imu.usable) { ready=false; reasons|=imu.reasons; }
    if(!imuTimely) { ready=false; reasons|=Stale|Discontinuity; }
    for(const auto& r:e.ranges) { sensor=fminf(sensor,r.confidence); if(!r.usable) { ready=false; reasons|=r.reasons; } }
    if(newSnapshot&&sampleCount_>1&&p_.valid()) {
        bool expected[2]={};
        if(active_&&!pending_&&e.nowMs-start_.atMs>=p_.encoder.noProgressUs/1000) {
            for(unsigned i=0;i<2;++i) expected[i]=(e.encoder.count[i]-start_.count[i])*p_.cmPerEdge[i]+start_.travel[i]<start_.target[i]-p_.motion.distance;
        }
        const float leftProgress=active_?((e.encoder.count[0]-start_.count[0])*p_.cmPerEdge[0]+start_.travel[0])/start_.target[0]:NAN;
        const float rightProgress=active_?((e.encoder.count[1]-start_.count[1])*p_.cmPerEdge[1]+start_.travel[1])/start_.target[1]:NAN;
        const auto enc=SensorConfidence::encoder(previous,e.encoder,p_.encoder,e.configured,active_,expected,leftProgress,rightProgress);
        sensor=fminf(sensor,enc.confidence);
        if(!enc.usable) { ready=false; reasons|=enc.reasons; }
        if(enc.reasons&(Discontinuity|UnexpectedMotion)) lose(enc.reasons);
    }
    if(active_&&!ready) { hold_=true; latchedReasons_|=reasons; if(!imu.usable||!imuTimely) lose(Discontinuity); }
    if(e.criticalFault) { ready=false; hold_=true; reasons|=LocalSafety; }
    if(p_.valid()) integrate(e);
    if(!haveAligned_) { ready=false; reasons|=Missing; }
    const auto covariance=pose_.covariance();
    if(haveAligned_&&!lost_&&(!pose_.healthy()||!covariance.valid())) lose(Numerical);
    if(haveAligned_&&(covariance.maximumPositionVariance()>p_.maximumPositionVariance||covariance.tt>p_.maximumYawVariance)) lose(Uncertainty);
    decision_.sensor=sensor;
    if(active_&&ready&&!lost_&&e.nowMs-lastEvaluationMs_>=10) {
        lastEvaluationMs_=e.nowMs; motion_=MotionConfidence::evaluate(movement(e,pending_),p_.motion);
        if(!motion_.accepted) { hold_=true; latchedReasons_|=motion_.reasons; }
    }
    if(pending_&&e.nowMs-terminal_.atMs>=p_.settleMs) {
        const uint32_t afterSettle=e.nowMs-terminal_.atMs-p_.settleMs;
        const bool covered=e.nowUs>=latestImu_.timestampUs&&(e.nowUs-latestImu_.timestampUs)/1000<=afterSettle&&
            motionAligned_.count[0]==e.encoder.count[0]&&motionAligned_.count[1]==e.encoder.count[1]&&
            motionAligned_.fractionSide[0]==0&&motionAligned_.fractionSide[1]==0;
        if(covered||!ready||lost_) {
            // Flush the old movement's interval BEFORE changing direction or
            // declaring rest. No late coast may inherit a later command's sign.
            if(covered&&!lost_) integrate(e,true);
            finish(e);
        }
    }
    if(haveAligned_&&!active_&&fabsf(wrap(pose_.pose().yaw-nominal_.yaw))>p_.routeYawTolerance) lose(Heading|UnexpectedMotion);
    const auto& h=historySummary_;
    decision_.evaluatedAtMs=e.nowMs; ++decision_.generation; decision_.movementId=start_.id;
    decision_.sensor=sensor; decision_.motion=motion_.confidence; decision_.history=h.confidence;
    decision_.route=haveAligned_&&!lost_?routeScore_:0;
    decision_.scores=ready?SensorScore:0;
    if(motion_.usable) decision_.scores|=MotionScore;
    if(h.valid) decision_.scores|=HistoryScore;
    if(haveAligned_) decision_.scores|=RouteScore;
    float nav=decision_.route;
    if(motion_.usable) nav=fminf(nav,motion_.confidence);
    if(h.valid) nav=fminf(nav,h.confidence);
    decision_.navigation=nav; if(haveAligned_) decision_.scores|=NavigationScore;
    bool warning=ready&&((motion_.usable&&motion_.confidence<p_.warningConfidence)||(h.valid&&
        (fabsf(h.distanceBias)>p_.historyBias||fabsf(h.yawBias)>p_.historyBias||h.residualSlope>p_.historySlope)));
    if(newSnapshot) { if(warning&&warnings_<UINT8_MAX) ++warnings_; else if(!warning) warnings_=0; }
    if(warnings_>=p_.warningSamples&&p_.warningSamples) reasons|=History;
    reasons|=latchedReasons_;
    if(pending_) reasons|=Settling;
    if(e.blocked) reasons|=Obstruction;
    decision_.reasons=reasons;
    decision_.clearance=e.blocked?Clearance::Blocked:(!ready?Clearance::Unknown:Clearance::Clear);
    decision_.health=e.criticalFault?Health::Fault:ready?Health::Ready:Health::Inhibited;
    decision_.trust=lost_?Trust::Lost:hold_?(active_?Trust::Degraded:Trust::Verify):!ready?Trust::Initializing:
        (h.valid&&!warning?Trust::Trusted:Trust::Acceptable);
    decision_.dutyCeiling=(warnings_>=p_.warningSamples&&p_.warningSamples)?p_.cautionDuty:p_.maximumDuty;
    decision_.permissions=0;
    if(ready&&!lost_&&!hold_&&!pending_&&e.localSafe&&!e.blocked) decision_.permissions=active_?Continue:Start;
    if(ready&&!lost_&&!active_&&!pending_&&e.localSafe&&!e.blocked) {
        decision_.permissions|=Verify;
        if(e.imu.sequence!=verificationSequence_) { verificationSequence_=e.imu.sequence; if(verifiedSamples_<UINT8_MAX) ++verifiedSamples_; }
    } else verifiedSamples_=0;
    if(lost_||(!active_&&hold_&&motion_.usable&&!motion_.accepted)||
       (p_.recoveryLimit&&recoveryAttempts_>=p_.recoveryLimit)) {
        decision_.permissions|=Human;
        if(recoveryAttempts_&&recoveryAttempts_>=p_.recoveryLimit) decision_.reasons|=RecoveryLimit;
    }
}
bool ConfidenceSupervisor::prepare(uint32_t context,uint32_t step,uint32_t now) {
    if(!(decision_.permissions&Start)||now-decision_.evaluatedAtMs>p_.decisionAgeMs||active_||pending_||lost_||hold_) return false;
    context_=context; step_=step; prepared_=true; preparedAtMs_=now; return true;
}
bool ConfidenceSupervisor::verify(uint32_t now) {
    if(!(decision_.permissions&Verify)||now-decision_.evaluatedAtMs>p_.decisionAgeMs||verifiedSamples_<p_.verificationSamples||lost_||
       (motion_.usable&&!motion_.accepted)) return false;
    hold_=false; latchedReasons_=0; warnings_=0; verifiedSamples_=0; return true;
}
Constraint ConfidenceSupervisor::constraint(uint32_t now) const {
    Constraint c; c.evaluatedAtMs=decision_.evaluatedAtMs; c.validForMs=p_.decisionAgeMs;
    c.movementId=start_.id; c.dutyCeiling=decision_.dutyCeiling;
    // Lifecycle faults/settling can occur between periodic evaluations. Never
    // let a previously ALLOW diagnostic mask those immediate hard gates.
    c.allowed=!lost_&&!hold_&&!pending_&&(decision_.permissions&(Start|Continue))&&now-c.evaluatedAtMs<=c.validForMs; return c;
}
void ConfidenceSupervisor::checkpoint(bool accepted,bool rejected,uint32_t now) {
    if(rejected) { hold_=true; latchedReasons_|=Checkpoint; }
    if(accepted&&endpointReady()&&pose_.healthy()) {
        checkpoint_.estimated=pose_.pose(); checkpoint_.nominal=nominal_; checkpoint_.uncertainty=pose_.covariance();
        checkpoint_.context=context_; checkpoint_.step=step_; checkpoint_.atMs=now; checkpoint_.available=true;
    }
}
bool ConfidenceSupervisor::physicalRecoveryEligible() const {
    // Eligibility diagnostic only. Permissions NEVER include Recover or Correct.
    // V1 checkpoints have no independent physical attestation. They must not
    // make reversing/retracing appear physically justified by themselves.
    if(!checkpoint_.available||!checkpoint_.independentlyVerified||lost_||active_||pending_||recoveryAttempts_>=p_.recoveryLimit||!history_.size()) return false;
    return history_.at(0)->routeContext==checkpoint_.context&&history_.at(0)->routeStep<=checkpoint_.step&&
        pose_.healthy()&&decision_.health==Health::Ready&&decision_.clearance==Clearance::Clear;
}
void ConfidenceSupervisor::recoveryAttempt() { if(recoveryAttempts_<UINT8_MAX) ++recoveryAttempts_; }
}
