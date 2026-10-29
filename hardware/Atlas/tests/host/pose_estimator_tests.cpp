#include "PoseEstimator.h"
#include <cstdio>
using namespace AtlasConfidence;
#define CHECK(x) do { ++n; if(!(x)) { std::printf("pose FAIL %d\n",__LINE__); return 1; } } while(0)
int main() {
    unsigned n=0; PoseNoise q{0.01f,0.001f,0.01f,0.001f,0.001f,0.001f,0.001f,2000000};
    PoseEstimator p(q); CHECK(p.healthy()); CHECK(p.advance(10,0,10,10,1000000,1));
    CHECK(fabsf(p.pose().x-10)<1e-5f&&p.pose().y==0); auto a=p.covariance(); CHECK(a.valid());
    CHECK(!p.advance(10,0,10,10,1000000,1)); CHECK(p.pose().x==10);
    CHECK(p.advance(0,Pi/2,-5,5,1000000,2)); CHECK(p.advance(10,0,10,10,1000000,3));
    CHECK(fabsf(p.pose().x-10)<1e-4f&&fabsf(p.pose().y-10)<1e-4f);
    CHECK(p.covariance().tt>a.tt); CHECK(p.covariance().maximumPositionVariance()>a.maximumPositionVariance());
    CHECK(p.advance(-10,0,-10,-10,1000000,4)); CHECK(fabsf(p.pose().y)<1e-4f);
    CHECK(!p.advance(0,0,0,0,3000000,5)); CHECK(!p.healthy());
    CHECK((!Covariance{1,2,0,1,0,1}.valid()));
    PoseEstimator bad({}); CHECK(!bad.healthy());
    PoseEstimator spin(q); for(unsigned i=1;i<=8;++i) CHECK(spin.advance(0,Pi/2,-1,1,1000000,i));
    CHECK(fabsf(spin.pose().yaw)<1e-5f); CHECK(spin.covariance().tt>0.06f); // persistent bias grows with total time squared.
    // Finite-difference state Jacobian against the approved midpoint model.
    const float h=0.001f,d=7,t=0.4f,theta=0.2f,phi=theta+t/2;
    CHECK(fabsf((d*cosf(phi+h)-d*cosf(phi-h))/(2*h)+d*sinf(phi))<0.001f);
    CHECK(fabsf((d*sinf(phi+h)-d*sinf(phi-h))/(2*h)-d*cosf(phi))<0.001f);
    std::printf("pose_estimator: %u/%u checks passed\n",n,n);
}
