#include "PoseEstimator.h"
namespace AtlasConfidence {
bool Covariance::valid() const {
    const float v[]={xx,xy,xt,yy,yt,tt}; for(float a:v) if(!isfinite(a)) return false;
    if(xx<0||yy<0||tt<0) return false;
    // Scale into a dimensionless correlation matrix before numerical PSD tests.
    const float d[3]={sqrtf(xx),sqrtf(yy),sqrtf(tt)};
    if((!d[0]&&(xy!=0||xt!=0))||(!d[1]&&(xy!=0||yt!=0))||(!d[2]&&(xt!=0||yt!=0))) return false;
    float a=d[0]&&d[1]?xy/(d[0]*d[1]):0;
    float b=d[0]&&d[2]?xt/(d[0]*d[2]):0;
    float c=d[1]&&d[2]?yt/(d[1]*d[2]):0;
    return fabsf(a)<=1.00001f&&fabsf(b)<=1.00001f&&fabsf(c)<=1.00001f&&1+2*a*b*c-a*a-b*b-c*c>=-1e-5f;
}
float Covariance::maximumPositionVariance() const {
    return 0.5f*(xx+yy+sqrtf((xx-yy)*(xx-yy)+4*xy*xy));
}
bool PoseNoise::valid() const {
    const float v[]={biasVariance,leftScaleVariance,rightScaleVariance};
    for(float a:v) if(!positive(a)) return false; // zero means unconfigured calibration uncertainty.
    return positive(distancePerCm)&&positive(yawPerSecond)&&positive(lateralPerCm)&&positive(positionPerSecond)&&maximumGapUs;
}
Covariance PoseEstimator::covariance() const {
    Covariance p=random_; const float variance[]={noise_.biasVariance,noise_.leftScaleVariance,noise_.rightScaleVariance};
    for(unsigned i=0;i<3;++i) {
        const float x=sensitivity_[0][i],y=sensitivity_[1][i],t=sensitivity_[2][i],v=variance[i];
        p.xx+=v*x*x; p.xy+=v*x*y; p.xt+=v*x*t; p.yy+=v*y*y; p.yt+=v*y*t; p.tt+=v*t*t;
    }
    return p;
}
bool PoseEstimator::advance(float d,float t,float left,float right,uint32_t us,uint32_t sequence) {
    if(seen_&&sequence==sequence_) return false; // idempotent consumption, not a fault.
    if(!healthy_||!us||us>noise_.maximumGapUs||!isfinite(d)||!isfinite(t)||!isfinite(left)||!isfinite(right)) { healthy_=false; return false; }
    const float dt=us*1e-6f, phi=pose_.yaw+t*0.5f,c=cosf(phi),s=sinf(phi),a=-d*s,b=d*c;
    const float g[3][2]={{c,-0.5f*d*s},{s,0.5f*d*c},{0,1}};
    const float travel=(fabsf(left)+fabsf(right))*0.5f;
    const float vd=noise_.distancePerCm*travel+noise_.positionPerSecond*dt,vt=noise_.yawPerSecond*dt;
    float q[6]; unsigned n=0;
    const unsigned row[]={0,0,0,1,1,2},col[]={0,1,2,1,2,2};
    for(unsigned i=0;i<6;++i) q[n++]=g[row[i]][0]*g[col[i]][0]*vd+g[row[i]][1]*g[col[i]][1]*vt;
    const float lateral=noise_.lateralPerCm*travel;
    q[0]+=lateral*s*s; q[1]-=lateral*s*c; q[3]+=lateral*c*c;
    const auto p=random_;
    random_={p.xx+2*a*p.xt+a*a*p.tt+q[0],p.xy+b*p.xt+a*p.yt+a*b*p.tt+q[1],p.xt+a*p.tt+q[2],
        p.yy+2*b*p.yt+b*b*p.tt+q[3],p.yt+b*p.tt+q[4],p.tt+q[5]};
    for(unsigned j=0;j<3;++j) {
        sensitivity_[0][j]+=a*sensitivity_[2][j]; sensitivity_[1][j]+=b*sensitivity_[2][j];
        const float dd=j==1?left*0.5f:j==2?right*0.5f:0;
        const float da=j==0?-dt:0;
        for(unsigned i=0;i<3;++i) sensitivity_[i][j]+=g[i][0]*dd+g[i][1]*da;
    }
    pose_.x+=d*c; pose_.y+=d*s; pose_.yaw=wrap(pose_.yaw+t);
    sequence_=sequence; seen_=true;
    healthy_=isfinite(pose_.x)&&isfinite(pose_.y)&&isfinite(pose_.yaw)&&covariance().valid();
    return healthy_;
}
}
