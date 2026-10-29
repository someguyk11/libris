#include "SensorConfidence.h"
#include <cstdio>
#include <cstdlib>
using namespace AtlasConfidence;
#define CHECK(x) do { ++n; if(!(x)) { std::printf("sensor FAIL %d\n",__LINE__); return 1; } } while(0)
int main() {
    unsigned n=0; Freshness f{20,100,20};
    CHECK(SensorConfidence::evaluate(true,1,0,f).confidence==1);
    CHECK(!SensorConfidence::evaluate(false,1,0,f).usable);
    CHECK(!SensorConfidence::evaluate(true,NAN,0,f).usable);
    CHECK(!SensorConfidence::ultrasonic(true,false,0,101,f).usable);
    CHECK(!SensorConfidence::ultrasonic(true,true,3,0,f).usable);
    CHECK(SensorConfidence::ultrasonic(true,false,0,0,f).usable);
    EncoderSnapshot a,b; a.count[0]=UINT32_MAX-1; b.count[0]=1; b.capturedUs=10000;
    EncoderLimits l{1000,0.2f,100000}; bool expected[2]={false,false};
    CHECK(SensorConfidence::encoder(a,b,l,true,true,expected).usable);
    CHECK(SensorConfidence::encoder(a,b,l,true,false,expected).reasons&UnexpectedMotion);
    b.count[0]=1000; CHECK(SensorConfidence::encoder(a,b,l,true,true,expected).reasons&PulseRate);
    b=a; CHECK(!SensorConfidence::encoder(a,b,l,true,false,expected).usable);
    b.capturedUs=10000; CHECK(SensorConfidence::encoder(a,b,l,true,false,expected).usable);
    expected[0]=true; CHECK(SensorConfidence::encoder(a,b,l,true,true,expected).reasons&NoProgress);
    CHECK(SensorConfidence::encoder(a,b,l,false,false,expected).reasons&Configuration);
    ImuEvidence i; CHECK(!SensorConfidence::imu(i,100,f).usable);
    i.valid=i.calibrated=i.continuous=true; i.quality=1; CHECK(SensorConfidence::imu(i,100,f).usable);
    CHECK(!SensorConfidence::imu(i,101000,f).usable);
    expected[0]=false;
    const auto mismatch=SensorConfidence::encoder(a,b,l,true,true,expected,0.1f,0.2f);
    CHECK(mismatch.usable); CHECK(mismatch.quality<1&&mismatch.quality>0);
    CHECK(mismatch.confidence==mismatch.validity*mismatch.quality*mismatch.freshness);
    expected[0]=true; const auto absent=SensorConfidence::encoder(a,b,l,true,true,expected);
    CHECK(!absent.usable); CHECK(absent.confidence==absent.validity*absent.quality*absent.freshness);
    std::printf("sensor_confidence: %u/%u checks passed\n",n,n);
}
