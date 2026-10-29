#pragma once
#include "ConfidenceTypes.h"
namespace AtlasConfidence {
struct EncoderLimits {
    float maximumEdgesPerSecond=0, progressMismatch=0;
    uint32_t noProgressUs=0;
};
struct ImuEvidence {
    uint64_t timestampUs=0;
    uint32_t sequence=0, generation=0, gaps=0;
    int32_t turns=0;
    float yaw=0, yawVariance=0, biasVariance=0, quality=0;
    bool valid=false, calibrated=false, continuous=false;
};
class SensorConfidence {
public:
    static Assessment evaluate(bool valid,float quality,uint32_t age,const Freshness& freshness,uint32_t reason=Missing);
    static Assessment encoder(const EncoderSnapshot& previous,const EncoderSnapshot& current,
        const EncoderLimits& limits,bool configured,bool active,bool expected[2],float leftProgress=NAN,float rightProgress=NAN);
    static Assessment ultrasonic(bool valid,bool faulted,uint16_t failures,uint32_t age,const Freshness& freshness);
    static Assessment imu(const ImuEvidence& evidence,uint64_t nowUs,const Freshness& freshness);
};
}
