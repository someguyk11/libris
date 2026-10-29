#include "ImuIntegration.h"
namespace AtlasConfidence {
bool ImuParameters::valid() const {
    if (!positive(stationaryRate)||!positive(accelerationToleranceMg)||!positive(maximumNoise)||calibrationSamples<2||!maximumGapUs||calibrationValidityUs<=maximumGapUs) return false;
    for(unsigned i=0;i<3;++i) for(unsigned j=0;j<3;++j) {
        float dot=0; for(unsigned k=0;k<3;++k) { if(!isfinite(mounting[i*3+k])) return false; dot+=mounting[i*3+k]*mounting[j*3+k]; }
        if(fabsf(dot-(i==j?1.0f:0.0f))>1e-4f) return false;
    }
    const float* r=mounting;
    return fabsf(r[0]*(r[4]*r[8]-r[5]*r[7])-r[1]*(r[3]*r[8]-r[5]*r[6])+r[2]*(r[3]*r[7]-r[4]*r[6])-1)<1e-4f;
}
void ImuIntegration::invalidate() {
    evidence_.valid=false; evidence_.continuous=false; evidence_.quality=0;
    if (!broken_) { ++evidence_.gaps; ++evidence_.generation; }
    broken_=true; // require explicit restart/recalibration; never bridge the missing interval.
}
bool ImuIntegration::sample(const float raw[3],const float acc[3],uint64_t now,bool stationary,bool saturated) {
    if (!parameters_.valid()||saturated||broken_) { invalidate(); return false; }
    float w[3]={}; float an=0;
    for(unsigned i=0;i<3;++i) {
        if(!isfinite(raw[i])||!isfinite(acc[i])) { invalidate(); return false; }
        an+=acc[i]*acc[i]; for(unsigned j=0;j<3;++j) w[i]+=parameters_.mounting[i*3+j]*raw[j];
    }
    if(haveTime_&&(now<=previousUs_||now-previousUs_>parameters_.maximumGapUs)) { invalidate(); return false; }
    const float dt=haveTime_?static_cast<float>(now-previousUs_)*1e-6f:0;
    previousUs_=now; haveTime_=true;
    evidence_.timestampUs=now; ++evidence_.sequence;
    if (!evidence_.calibrated) {
        bool stable=stationary&&fabsf(sqrtf(an)-1000)<parameters_.accelerationToleranceMg;
        for(float v:w) stable=stable&&fabsf(v)<=parameters_.stationaryRate;
        if(!stable) { count_=0; for(unsigned i=0;i<3;++i) mean_[i]=m2_[i]=0; return false; }
        ++count_;
        for(unsigned i=0;i<3;++i) { float delta=w[i]-mean_[i]; mean_[i]+=delta/count_; m2_[i]+=delta*(w[i]-mean_[i]); }
        if(count_<parameters_.calibrationSamples) return false;
        for(unsigned i=0;i<3;++i) if(m2_[i]/(count_-1)>parameters_.maximumNoise*parameters_.maximumNoise) { invalidate(); return false; }
        evidence_.biasVariance=(m2_[2]/(count_-1))/count_;
        evidence_.calibrated=evidence_.continuous=evidence_.valid=true;
        evidence_.quality=1; ++evidence_.generation; previousRate_=w[2]-mean_[2]; calibratedAtUs_=now; return true;
    }
    if(now-calibratedAtUs_>parameters_.calibrationValidityUs) { invalidate(); return false; }
    const float rate=w[2]-mean_[2];
    const float angle=evidence_.yaw+0.5f*(rate+previousRate_)*dt;
    const float turns=floorf((angle+Pi)/(2*Pi));
    if(!isfinite(angle)||turns>=2147483648.0f||turns< -2147483648.0f) { invalidate(); return false; }
    const int64_t total=static_cast<int64_t>(evidence_.turns)+static_cast<int32_t>(turns);
    if(total>INT32_MAX||total<INT32_MIN) { invalidate(); return false; }
    evidence_.turns=static_cast<int32_t>(total);
    evidence_.yaw=wrap(angle);
    evidence_.yawVariance+=(m2_[2]/(count_-1))*dt*dt;
    previousRate_=rate; evidence_.valid=evidence_.continuous=true; evidence_.quality=1;
    return true;
}
}
