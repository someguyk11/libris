#pragma once

#include <stdint.h>
#include <math.h>

// Hardware-independent contracts. Scores are engineering indices, NOT probabilities.
namespace AtlasConfidence {
constexpr float Pi = 3.14159265358979323846f;
enum class Trust : uint8_t { Initializing, Trusted, Acceptable, Degraded, Verify, Lost };
enum class Clearance : uint8_t { Unknown, Clear, Blocked };
enum class Health : uint8_t { Ready, Inhibited, Fault };
enum Reason : uint32_t {
    None=0, Configuration=1u<<0, Missing=1u<<1, Stale=1u<<2, Noise=1u<<3,
    NoProgress=1u<<4, PulseRate=1u<<5, Discontinuity=1u<<6, UnexpectedMotion=1u<<7,
    WheelDisagreement=1u<<8, Distance=1u<<9, Heading=1u<<10, Overshoot=1u<<11,
    EncoderImu=1u<<12, Uncertainty=1u<<13, LocalSafety=1u<<14, Context=1u<<15,
    Obstruction=1u<<16, Checkpoint=1u<<17, History=1u<<18, RecoveryLimit=1u<<19,
    Numerical=1u<<20, Settling=1u<<21, Unobserved=1u<<22
};
enum Permission : uint8_t { Start=1, Continue=2, Verify=4, Correct=8, Recover=16, Human=32 };
enum Score : uint8_t { SensorScore=1, MotionScore=2, HistoryScore=4, RouteScore=8, NavigationScore=16 };
enum class Outcome : uint8_t { Complete, Interrupted, Fault };
enum RecordFlag : uint8_t { MeasurementValid=1, ResidualValid=2, ObstacleInterruption=4, EncoderDisagreement=8 };
// Diagnostic tags only; full protocol identities still authorize every backend command.
inline uint32_t contextTag(const char* first,const char* second) {
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<2;++i) {
        const char* text=i?second:first;
        if(text) for(;*text;++text) { hash^=static_cast<uint8_t>(*text); hash*=16777619u; }
        hash^=0xff; hash*=16777619u;
    }
    return hash?hash:1;
}
inline bool positive(float v) { return isfinite(v) && v>0; }
inline float clamp01(float v) { return isfinite(v) ? fminf(1, fmaxf(0,v)) : 0; }
inline float wrap(float v) {
    if (!isfinite(v)) return NAN;
    float r=fmodf(v+Pi,2*Pi); if (r<0) r+=2*Pi; return r-Pi;
}
inline bool normalize(float value,float tolerance,float& result) {
    if (!isfinite(value)||!positive(tolerance)) { result=0; return false; }
    result=value/tolerance; return isfinite(result);
}
struct Freshness { uint32_t graceMs=0, hardMs=0; float tauMs=0; };
inline float freshness(uint32_t age,const Freshness& p) {
    if (!p.hardMs || p.graceMs>p.hardMs || !positive(p.tauMs) || age>p.hardMs) return 0;
    return age<=p.graceMs ? 1 : expf(-static_cast<float>(age-p.graceMs)/p.tauMs);
}
struct Assessment {
    float validity=0, quality=0, freshness=0, confidence=0;
    uint32_t reasons=Missing;
    bool usable=false;
};
struct EncoderSnapshot {
    uint32_t count[2]={}, lastEdgeUs[2]={}, capturedUs=0, spanUs=0;
    bool seen[2]={};
};
struct MotionEvidence {
    float expectedDistance=0, measuredDistance=0, expectedYaw=0, measuredYaw=0;
    float encoderYaw=0, left=0, right=0, leftTarget=0, rightTarget=0;
    float measurementConfidence=0;
    bool terminal=false, interrupted=false, continuous=false, progressing=true;
    uint8_t fault=0;
};
struct MotionLimits {
    float distance=0, yaw=0, wheelFraction=0, encoderYaw=0, excursion=0;
    bool valid() const { return positive(distance)&&positive(yaw)&&positive(wheelFraction)&&positive(encoderYaw)&&positive(excursion); }
};
struct MotionAssessment {
    float confidence=0, distanceResidual=0, yawResidual=0;
    uint32_t reasons=Missing;
    bool usable=false, accepted=false;
};
struct Decision {
    uint32_t evaluatedAtMs=0, generation=0, movementId=0, reasons=Missing;
    float sensor=0, motion=0, history=0, route=0, navigation=0;
    uint16_t dutyCeiling=0;
    Trust trust=Trust::Initializing;
    Clearance clearance=Clearance::Unknown;
    Health health=Health::Inhibited;
    uint8_t permissions=0, scores=0, reserved=0;
};
static_assert(sizeof(Decision)==44,"Decision wire-copy budget changed");
struct Constraint {
    uint32_t evaluatedAtMs=0, validForMs=0, movementId=0;
    uint16_t dutyCeiling=0;
    bool allowed=false;
    bool fresh(uint32_t now) const { return validForMs && now-evaluatedAtMs<=validForMs; }
};
// Dependency-inverted execution hooks. Missing production hooks deny motion.
struct ExecutionGate {
    void* context=nullptr;
    bool required=true;
    bool (*authorize)(void*,uint32_t,uint32_t,uint32_t)=nullptr;
    bool (*endpoint)(void*)=nullptr;
    bool (*settling)(void*)=nullptr;
    bool (*verification)(void*,uint32_t)=nullptr;
    bool (*checkpoint)(void*,bool,bool,uint32_t)=nullptr;
    bool permit(uint32_t route,uint32_t step,uint32_t now) const { return authorize?authorize(context,route,step,now):!required; }
    bool endpointReady() const { return endpoint?endpoint(context):!required; }
    bool isSettling() const { return settling&&settling(context); }
    bool verify(uint32_t now) const { return verification&&verification(context,now); }
    bool assessCheckpoint(bool accepted,bool rejected,uint32_t now) const { return checkpoint?checkpoint(context,accepted,rejected,now):!required; }
};
struct MovementRecord {
    uint32_t movementId=0, routeContext=0, routeStep=0, durationMs=0, leftTicks=0, rightTicks=0;
    float expectedDistanceCm=0, measuredDistanceCm=0, expectedYawRad=0, measuredYawRad=0;
    float encoderYawRad=0, motionConfidence=0, distanceResidualCm=0, yawResidualRad=0;
    uint16_t reasonBits=0, attempt=0;
    uint8_t kind=0; Outcome outcome=Outcome::Interrupted; uint8_t fault=0, flags=0;
};
static_assert(sizeof(MovementRecord)==64,"Ten-event RAM budget changed");
} // namespace AtlasConfidence
