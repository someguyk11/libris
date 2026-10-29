#include "MotionConfidence.h"
namespace AtlasConfidence {
MotionAssessment MotionConfidence::evaluate(const MotionEvidence& e,const MotionLimits& l) {
    MotionAssessment r;
    if(!l.valid()) { r.reasons=Configuration; return r; }
    const float values[]={e.expectedDistance,e.measuredDistance,e.expectedYaw,e.measuredYaw,e.encoderYaw,e.left,e.right,e.leftTarget,e.rightTarget,e.measurementConfidence};
    for(float v:values) if(!isfinite(v)) { r.reasons=Numerical; return r; }
    r.reasons=None;
    if(!e.continuous||e.measurementConfidence<=0) r.reasons|=Discontinuity;
    if(e.fault) r.reasons|=LocalSafety;
    if(!e.progressing&&!e.interrupted) r.reasons|=NoProgress;
    if(e.leftTarget<0||e.rightTarget<0) r.reasons|=Context;
    const float lp=positive(e.leftTarget)?e.left/e.leftTarget:1;
    const float rp=positive(e.rightTarget)?e.right/e.rightTarget:1;
    if(positive(e.leftTarget)&&positive(e.rightTarget)&&fabsf(lp-rp)>l.wheelFraction) r.reasons|=WheelDisagreement;
    if(e.left>e.leftTarget+l.distance||e.right>e.rightTarget+l.distance) r.reasons|=Overshoot;
    if(fabsf(e.measuredYaw-e.encoderYaw)>l.encoderYaw) r.reasons|=EncoderImu;
    const float target=e.leftTarget+e.rightTarget;
    const float phase=target>0?fminf(1,fmaxf(0,(e.left+e.right)/target)):1;
    const float expected=e.terminal&&!e.interrupted?e.expectedYaw:phase*e.expectedYaw;
    r.yawResidual=wrap(e.measuredYaw-expected);
    if(fabsf(r.yawResidual)>l.yaw||fabsf(e.measuredYaw-expected)>l.excursion) r.reasons|=Heading;
    // Interrupted distance is an unexecuted remainder, not an execution failure.
    r.distanceResidual=e.terminal&&!e.interrupted?e.measuredDistance-e.expectedDistance:0;
    if(fabsf(r.distanceResidual)>l.distance) r.reasons|=Distance;
    r.usable=e.continuous&&e.measurementConfidence>0;
    const float zd=r.distanceResidual/l.distance, za=r.yawResidual/l.yaw;
    r.confidence=r.usable?clamp01(e.measurementConfidence)*expf(-0.5f*(zd*zd+za*za)):0;
    r.accepted=r.reasons==None;
    return r;
}
}
