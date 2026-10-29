#pragma once
#include "SensorConfidence.h"
namespace AtlasConfidence {
struct ImuParameters {
    float mounting[9]={}; // proper sensor-to-body rotation; zero is unconfigured.
    float stationaryRate=0, accelerationToleranceMg=0, maximumNoise=0;
    uint32_t calibrationSamples=0, maximumGapUs=0;
    uint64_t calibrationValidityUs=0; // measured bias-drift envelope; zero is unconfigured.
    bool valid() const;
};
// Worker-owned pure integration. No hardware, clock, queue or motor calls.
class ImuIntegration {
public:
    explicit ImuIntegration(const ImuParameters& p):parameters_(p) {}
    bool sample(const float rateRad[3],const float accelerationMg[3],uint64_t timestampUs,bool stationary,bool saturated=false);
    void invalidate();
    const ImuEvidence& evidence() const { return evidence_; }
private:
    ImuParameters parameters_;
    ImuEvidence evidence_;
    float mean_[3]={}, m2_[3]={}, previousRate_=0;
    uint32_t count_=0;
    uint64_t previousUs_=0;
    uint64_t calibratedAtUs_=0;
    bool haveTime_=false, broken_=false;
};
inline float yawDelta(const ImuEvidence& to,const ImuEvidence& from) {
    return static_cast<float>(static_cast<int64_t>(to.turns)-from.turns)*(2*Pi)+to.yaw-from.yaw;
}
}
