#include "MotionConfidence.h"
#include <cstdio>
using namespace AtlasConfidence;
#define CHECK(x) do { ++n; if(!(x)) { std::printf("motion FAIL %d\n",__LINE__); return 1; } } while(0)
int main() {
    unsigned n=0; MotionLimits l{2,0.1f,0.2f,0.15f,0.4f}; MotionEvidence e;
    e.leftTarget=e.rightTarget=e.expectedDistance=10; e.left=e.right=e.measuredDistance=10;
    e.continuous=e.terminal=true; e.measurementConfidence=1;
    CHECK(MotionConfidence::evaluate(e,l).accepted);
    CHECK(MotionConfidence::evaluate(e,l).confidence==1);
    e.measuredDistance=9; CHECK(MotionConfidence::evaluate(e,l).accepted);
    e.measuredDistance=5; CHECK(MotionConfidence::evaluate(e,l).reasons&Distance);
    e.measuredDistance=10; e.left=15; CHECK(MotionConfidence::evaluate(e,l).reasons&Overshoot);
    e.left=10; e.measuredYaw=Pi; CHECK(MotionConfidence::evaluate(e,l).reasons&Heading);
    e.measuredYaw=2*Pi; CHECK(MotionConfidence::evaluate(e,l).reasons&Heading);
    e.measuredYaw=0; e.terminal=false; e.left=e.right=e.measuredDistance=3;
    CHECK(MotionConfidence::evaluate(e,l).accepted);
    e.interrupted=true; e.terminal=true; CHECK(MotionConfidence::evaluate(e,l).accepted);
    e.continuous=false; CHECK(!MotionConfidence::evaluate(e,l).accepted);
    e.continuous=true; e.measuredDistance=NAN; CHECK(MotionConfidence::evaluate(e,l).reasons&Numerical);
    CHECK(MotionConfidence::evaluate(e,{}).reasons&Configuration);
    std::printf("motion_confidence: %u/%u checks passed\n",n,n);
}
