#pragma once
#include "ConfidenceTypes.h"
namespace AtlasConfidence {
struct Pose { float x=0,y=0,yaw=0; };
struct Covariance {
    float xx=0,xy=0,xt=0,yy=0,yt=0,tt=0;
    bool valid() const;
    float maximumPositionVariance() const;
};
struct PoseNoise {
    float distancePerCm=0,yawPerSecond=0,lateralPerCm=0,positionPerSecond=0;
    float biasVariance=0,leftScaleVariance=0,rightScaleVariance=0;
    uint32_t maximumGapUs=0;
    bool valid() const;
};
class PoseEstimator {
public:
    explicit PoseEstimator(const PoseNoise& noise):noise_(noise),healthy_(noise.valid()) {}
    bool advance(float distance,float yaw,float left,float right,uint32_t dtUs,uint32_t sequence);
    const Pose& pose() const { return pose_; }
    Covariance covariance() const;
    bool healthy() const { return healthy_; }
    void invalidate() { healthy_=false; } // freeze last estimate, never invent motion after continuity is lost.
    // Coordinate-frame transformations/checkpoints never reset this state.
private:
    PoseNoise noise_;
    Pose pose_;
    Covariance random_;
    float sensitivity_[3][3]={};
    uint32_t sequence_=0;
    bool seen_=false,healthy_=false;
};
}
