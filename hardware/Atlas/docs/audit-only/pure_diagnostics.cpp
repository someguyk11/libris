// AUDIT ONLY: unchanged production math; synthetic evidence, no I/O.
#include "supervisor_fixture.inc"
#undef CHECK
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
static void require(bool b) { if(!b) throw std::runtime_error("invariant violated"); }
static void sensor_boundaries() {
    Freshness f{10,100,20};
    const float q[]={-1,0,0.01f,0.5f,1,2,NAN,INFINITY};
    for(float v:q) for(unsigned age:{0u,10u,11u,100u,101u,UINT32_MAX}) {
        auto r=SensorConfidence::evaluate(true,v,age,f);
        require(std::isfinite(r.confidence)&&r.confidence>=0&&r.confidence<=1);
        require(r.confidence==r.validity*r.quality*r.freshness);
        if(age>100||!std::isfinite(v)||v<=0) require(!r.usable);
    }
    EncoderSnapshot a,b; a.capturedUs=UINT32_MAX-999; b.capturedUs=1000;
    a.count[0]=UINT32_MAX-2; b.count[0]=2; bool expected[2]={};
    auto r=SensorConfidence::encoder(a,b,{10000,0.2f,100000},true,true,expected);
    require(r.usable);
}
static void history_rollover() {
    MovementHistory h; require(!h.summarize(1,1).valid);
    for(unsigned i=1;i<=11;++i) { MovementRecord r; r.movementId=i; r.motionConfidence=1;
        r.flags=MeasurementValid|ResidualValid; r.outcome=Outcome::Complete; h.push(r); }
    require(h.size()==10&&h.at(0)->movementId==2&&h.at(9)->movementId==11&&!h.at(10));
    auto r=*h.at(9); r.movementId=12; r.motionConfidence=NAN; h.push(r); require(h.summarize(1,1).eligible==9);
}
// Independent double-precision midpoint oracle. Jacobians are numerical,
// not copies of the production F/G/sensitivity equations.
using Vec=std::array<double,3>; using Mat=std::array<std::array<double,3>,3>;
struct Step { double l,r,t,dt; };
static Vec step(Vec s,const Step& u,double bias=0,double ls=0,double rs=0) {
    const double d=(u.l*(1+ls)+u.r*(1+rs))/2,t=u.t-bias*u.dt;
    const double phi=s[2]+t/2;
    return {s[0]+d*std::cos(phi),s[1]+d*std::sin(phi),s[2]+t};
}
static void covariance_oracle() {
    const PoseNoise n{0.01f,0.001f,0.02f,0.003f,0.004f,0.005f,0.006f,100000};
    PoseEstimator p(n); std::array<Step,80> path; Vec state{}; Mat random{};
    const double eps=1e-5;
    for(unsigned k=0;k<path.size();++k) {
        Step u{0.2*std::cos(k*0.2),0.3*std::sin(k*0.1),0.04*std::sin(k*0.3),0.02}; path[k]=u;
        Mat F{},G{};
        for(unsigned j=0;j<3;++j) { Vec plus=state,minus=state; plus[j]+=eps; minus[j]-=eps;
            Vec a=step(plus,u),b=step(minus,u); for(unsigned i=0;i<3;++i) F[i][j]=(a[i]-b[i])/(2*eps); }
        for(unsigned j=0;j<2;++j) { Step plus=u,minus=u;
            if(j==0) { plus.l+=eps; plus.r+=eps; minus.l-=eps; minus.r-=eps; }
            else { plus.t+=eps; minus.t-=eps; }
            Vec a=step(state,plus),b=step(state,minus); for(unsigned i=0;i<3;++i) G[i][j]=(a[i]-b[i])/(2*eps); }
        const double distance=(std::abs(u.l)+std::abs(u.r))/2;
        const double v[2]={n.distancePerCm*distance+n.positionPerSecond*u.dt,n.yawPerSecond*u.dt};
        const double phi=state[2]+u.t/2; const Vec lateral{-std::sin(phi),std::cos(phi),0};
        Mat next{};
        for(unsigned i=0;i<3;++i) for(unsigned j=0;j<3;++j) {
            for(unsigned x=0;x<3;++x) for(unsigned y=0;y<3;++y) next[i][j]+=F[i][x]*random[x][y]*F[j][y];
            for(unsigned x=0;x<2;++x) next[i][j]+=G[i][x]*v[x]*G[j][x];
            next[i][j]+=n.lateralPerCm*distance*lateral[i]*lateral[j];
        }
        random=next; state=step(state,u);
        require(p.advance((u.l+u.r)/2,u.t,u.l,u.r,20000,k+1));
    }
    Mat sensitivities{}; const double variance[]={n.biasVariance,n.leftScaleVariance,n.rightScaleVariance};
    for(unsigned j=0;j<3;++j) {
        Vec plus{},minus{};
        for(const auto& u:path) { plus=step(plus,u,j==0?eps:0,j==1?eps:0,j==2?eps:0);
            minus=step(minus,u,j==0?-eps:0,j==1?-eps:0,j==2?-eps:0); }
        for(unsigned i=0;i<3;++i) sensitivities[i][j]=(plus[i]-minus[i])/(2*eps);
    }
    const auto c=p.covariance(); const Mat actual{{{c.xx,c.xy,c.xt},{c.xy,c.yy,c.yt},{c.xt,c.yt,c.tt}}};
    double error=0;
    for(unsigned i=0;i<3;++i) for(unsigned j=0;j<3;++j) {
        double want=random[i][j]; for(unsigned k=0;k<3;++k) want+=sensitivities[i][k]*variance[k]*sensitivities[j][k];
        error=std::max(error,std::abs(want-actual[i][j]));
    }
    std::printf("finite-difference covariance oracle max absolute error %.9g\n",error);
    require(error<1e-5&&c.valid());
}
static void stopped_cadence() {
    auto n=parameters().pose; PoseEstimator a(n),b(n);
    for(unsigned i=1;i<=100;++i) require(a.advance(0,0,0,0,20000,i));
    for(unsigned i=1;i<=50;++i) require(b.advance(0,0,0,0,40000,i));
    require(std::abs(a.covariance().tt-b.covariance().tt)<1e-6);
    const auto c=a.covariance(); require(!a.advance(0,0,0,0,20000,100)); require(c.tt==a.covariance().tt);
}
static void full_rotation_rejected() {
    MotionEvidence e; e.continuous=true; e.measurementConfidence=1; e.terminal=true;
    e.expectedYaw=-Pi/2; e.measuredYaw=e.expectedYaw-2*Pi; e.encoderYaw=-Pi/2;
    e.left=e.right=e.leftTarget=e.rightTarget=1;
    const auto r=MotionConfidence::evaluate(e,parameters().motion);
    require(!r.accepted&&(r.reasons&Heading)&&(r.reasons&EncoderImu));
}
static void frozen_sequence_coverage() {
    const auto p=parameters(); ConfidenceSupervisor s(p); s.update(evidence(0)); s.update(evidence(10));
    for(unsigned ms=15;ms<=200;ms+=5) { auto e=evidence(ms); e.imu=evidence(10).imu; s.update(e); }
    require(!s.constraint(200).allowed);
}
static void blocked_not_lost() {
    ConfidenceSupervisor s(parameters()); s.update(evidence(0)); s.update(evidence(10));
    auto e=evidence(20); e.blocked=true; e.localSafe=false; s.update(e);
    require(s.decision().clearance==Clearance::Blocked&&s.decision().trust!=Trust::Lost&&!s.constraint(20).allowed);
}
static void backwards_unequal_expected_yaw() {
    // A valid signed reverse segment should agree with its own differential travel.
    ConfidenceSupervisor s(parameters()); s.update(evidence(0)); s.update(evidence(10)); require(s.prepare(1,1,10));
    Lifecycle l; l.kind=2; l.id=1; l.target[0]=10; l.target[1]=11.5; l.atMs=10; s.observe(l);
    auto e=evidence(30); e.moving=true; e.encoder.count[0]=100; e.encoder.count[1]=115;
    e.imu.yaw=-0.15f; s.update(e);
    std::printf("reverse unequal targets: reasons=0x%lx permissions=%u\n",static_cast<unsigned long>(s.decision().reasons),s.decision().permissions);
    require(s.constraint(30).allowed);
}
static void stale_quality_claim() {
    // Validate the public Evidence boundary, not memory corruption in a sensor ISR.
    ConfidenceSupervisor s(parameters()); s.update(evidence(0)); auto e=evidence(10);
    e.ranges[0].confidence=NAN; e.ranges[0].usable=true; s.update(e);
    std::printf("NaN pre-assessment: sensor=%g nav=%g permissions=%u\n",s.decision().sensor,s.decision().navigation,s.decision().permissions);
    require(!s.constraint(10).allowed);
}
int main() {
    struct Test { const char* name; void(*run)(); };
    const Test tests[]={{"sensor_boundaries",sensor_boundaries},{"history_rollover",history_rollover},
        {"covariance_finite_difference_oracle",covariance_oracle},{"stopped_cadence_and_duplicate",stopped_cadence},
        {"full_rotation_rejection",full_rotation_rejected},{"stale_sequence",frozen_sequence_coverage},
        {"blocked_not_lost",blocked_not_lost},{"reverse_unequal_target_sign",backwards_unequal_expected_yaw},
        {"nonfinite_public_assessment",stale_quality_claim}};
    unsigned passed=0;
    for(const auto& t:tests) { try { t.run(); ++passed; std::printf("PASS %s\n",t.name); }
        catch(const std::exception& e) { std::printf("FAIL %s: %s\n",t.name,e.what()); } }
    std::printf("AUDIT pure: %u/%zu scenarios pass\n",passed,sizeof(tests)/sizeof(Test));
    return passed==sizeof(tests)/sizeof(Test)?0:1;
}
