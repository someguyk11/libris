#include "SensorConfidence.h"
namespace AtlasConfidence {
Assessment SensorConfidence::evaluate(bool valid,float quality,uint32_t age,const Freshness& p,uint32_t reason) {
    Assessment r;
    r.validity=valid?1:0; r.quality=clamp01(quality); r.freshness=freshness(age,p);
    r.confidence=r.validity*r.quality*r.freshness;
    r.usable=valid&&isfinite(quality)&&quality>0&&r.freshness>0;
    r.reasons=!valid?reason:(!r.freshness?Stale:(!r.usable?Noise:None));
    return r;
}
Assessment SensorConfidence::encoder(const EncoderSnapshot& p,const EncoderSnapshot& c,
    const EncoderLimits& l,bool configured,bool active,bool expected[2],float leftProgress,float rightProgress) {
    Assessment r;
    if (!configured||!positive(l.maximumEdgesPerSecond)||!positive(l.progressMismatch)||!l.noProgressUs) {
        r.reasons=Configuration; return r;
    }
    uint32_t dt=c.capturedUs-p.capturedUs;
    if (!dt) {
        r.reasons=(c.count[0]!=p.count[0]||c.count[1]!=p.count[1])?Discontinuity:Unobserved; return r;
    }
    if (dt>=0x80000000u || c.spanUs>=dt) { r.reasons=Discontinuity; return r; }
    r.validity=1; r.quality=1; r.freshness=1; r.reasons=None;
    for (unsigned i=0;i<2;++i) {
        const uint32_t delta=c.count[i]-p.count[i]; // modular subtraction supports one rollover.
        if (delta>l.maximumEdgesPerSecond*(dt*1e-6f)+1) r.reasons|=PulseRate|Discontinuity;
        if (!active&&delta) r.reasons|=UnexpectedMotion;
        if (active&&expected[i]&&(!c.seen[i]||c.capturedUs-c.lastEdgeUs[i]>l.noProgressUs)) r.reasons|=NoProgress;
    }
    if(isfinite(leftProgress)&&isfinite(rightProgress)) {
        const float residual=(leftProgress-rightProgress)/l.progressMismatch;
        r.quality=expf(-0.5f*residual*residual);
    }
    if(r.reasons&(PulseRate|Discontinuity)) r.validity=0;
    if(r.reasons&NoProgress) r.quality=0;
    // Unexpected edges are trustworthy evidence of UNCOMMANDED motion, not
    // evidence that an encoder is broken. Navigation handles that hard gate.
    r.confidence=r.validity*r.quality*r.freshness;
    r.usable=r.confidence>0;
    return r;
}
Assessment SensorConfidence::ultrasonic(bool valid,bool faulted,uint16_t failures,uint32_t age,const Freshness& p) {
    // Range changes are not noise by definition. Never suppress a new obstruction.
    return evaluate(valid&&!faulted,1.0f/(1.0f+failures),age,p,Missing);
}
Assessment SensorConfidence::imu(const ImuEvidence& e,uint64_t now,const Freshness& p) {
    if (now<e.timestampUs) return evaluate(false,0,0,p,Discontinuity);
    const uint64_t age=(now-e.timestampUs)/1000;
    return evaluate(e.valid&&e.calibrated&&e.continuous&&isfinite(e.yaw)&&e.yawVariance>=0&&isfinite(e.yawVariance)&&
        isfinite(e.biasVariance)&&e.biasVariance>=0,
        e.quality,age>UINT32_MAX?UINT32_MAX:static_cast<uint32_t>(age),p,
        !e.continuous?Discontinuity:Missing);
}
}
