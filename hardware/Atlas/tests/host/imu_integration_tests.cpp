#include "ImuIntegration.h"
#include <cstdio>
#include <cmath>
using namespace AtlasConfidence;
#define CHECK(x) do { ++n; if(!(x)) { std::printf("imu FAIL %d\n",__LINE__); return 1; } } while(0)
int main() {
    unsigned n=0; ImuParameters p; CHECK(!p.valid());
    p.mounting[0]=p.mounting[4]=p.mounting[8]=1; p.stationaryRate=0.1f; p.accelerationToleranceMg=20;
    p.maximumNoise=0.01f; p.calibrationSamples=4; p.maximumGapUs=20000; p.calibrationValidityUs=100000000; CHECK(p.valid());
    ImuIntegration i(p); float w[3]={0,0,0.02f}, a[3]={0,0,1000};
    for(unsigned k=0;k<4;++k) i.sample(w,a,k*10000,true);
    CHECK(i.evidence().calibrated); w[2]=1.02f;
    for(unsigned k=4;k<=104;++k) i.sample(w,a,k*10000,false);
    CHECK(fabsf(i.evidence().yaw-1.005f)<0.0001f);
    const auto seq=i.evidence().sequence; CHECK(!i.sample(w,a,1040000,false));
    CHECK(!i.evidence().continuous); CHECK(i.evidence().sequence==seq);
    CHECK(!i.sample(w,a,1050000,false));
    ImuIntegration gap(p); for(unsigned k=0;k<4;++k) gap.sample(w,a,k*10000,true);
    CHECK(!gap.evidence().calibrated); // moving-rate samples may not calibrate.
    ImuIntegration mounted(p); w[2]=0;
    for(unsigned k=0;k<4;++k) mounted.sample(w,a,k*10000,true);
    CHECK(!mounted.sample(w,a,100000,true)); CHECK(mounted.evidence().gaps==1);
    p.mounting[8]=-1; CHECK(!p.valid());
    p.mounting[8]=1;
    ImuIntegration saturated(p); CHECK(!saturated.sample(w,a,0,true,true)); CHECK(!saturated.evidence().valid);
    ImuIntegration noise(p);
    for(unsigned k=0;k<4;++k) { w[2]=(k&1)?0.05f:-0.05f; noise.sample(w,a,k*10000,true); }
    CHECK(!noise.evidence().calibrated); CHECK(noise.evidence().gaps==1);
    p.calibrationValidityUs=50000; ImuIntegration expires(p); w[2]=0;
    for(unsigned k=0;k<4;++k) expires.sample(w,a,k*10000,true);
    CHECK(expires.evidence().calibrated);
    for(unsigned k=4;k<=9;++k) expires.sample(w,a,k*10000,true);
    CHECK(!expires.evidence().continuous);
    ImuIntegration invalid(p); w[0]=NAN; CHECK(!invalid.sample(w,a,0,true));
    std::printf("imu_integration: %u/%u checks passed\n",n,n);
}
