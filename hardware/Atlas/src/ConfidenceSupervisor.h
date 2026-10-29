#pragma once
#include "MotionConfidence.h"
#include "MovementHistory.h"
#include "PoseEstimator.h"
#include "ImuIntegration.h"
namespace AtlasConfidence {
struct SupervisorParameters {
    MotionLimits motion;
    EncoderLimits encoder;
    PoseNoise pose;
    Freshness ultrasonic,imu;
    float cmPerEdge[2]={},trackCm=0,clockwiseScale=0,counterclockwiseScale=0;
    float maximumPositionVariance=0,maximumYawVariance=0,warningConfidence=0,historyBias=0,historySlope=0;
    float routeDistanceTolerance=0,routeYawTolerance=0;
    uint32_t decisionAgeMs=0,settleMs=0,alignmentUs=0;
    uint16_t minimumDuty=0,maximumDuty=0,cautionDuty=0;
    uint8_t warningSamples=0,verificationSamples=0,recoveryLimit=0;
    bool valid() const;
};
struct Evidence {
    EncoderSnapshot encoder;
    ImuEvidence imu;
    Assessment ranges[4];
    uint32_t nowMs=0;
    uint64_t nowUs=0;
    bool configured=false,localSafe=false,blocked=false,moving=false,criticalFault=false;
};
enum class MotionEvent : uint8_t { Start, Resume, Complete, Stop, Fault, BeforeReset };
struct Lifecycle {
    uint32_t id=0,atMs=0,count[2]={};
    float target[2]={},travel[2]={};
    uint16_t attempt=0;
    uint8_t kind=0,fault=0;
    MotionEvent event=MotionEvent::Start;
    bool energized=false,obstacle=false;
};
struct TrustworthyCheckpoint {
    Pose estimated,nominal;
    Covariance uncertainty;
    uint32_t context=0,step=0,atMs=0;
    bool available=false,independentlyVerified=false;
};
// Pure single-owner supervisor. Hardware adapters supply snapshots/events.
class ConfidenceSupervisor {
public:
    explicit ConfidenceSupervisor(const SupervisorParameters& parameters);
    void update(const Evidence& evidence);
    void observe(const Lifecycle& event);
    bool prepare(uint32_t context,uint32_t step,uint32_t nowMs);
    bool verify(uint32_t nowMs);
    void checkpoint(bool accepted,bool rejected,uint32_t nowMs);
    bool endpointReady() const { return !active_&&!pending_&&!lost_&&!hold_&&terminalAccepted_; }
    bool settling() const { return pending_; }
    bool physicalRecoveryEligible() const;
    void recoveryAttempt(); // accounting only; never commands recovery motion.
    const Decision& decision() const { return decision_; }
    Constraint constraint(uint32_t nowMs) const;
    const MovementHistory& history() const { return history_; }
    const PoseEstimator& estimator() const { return pose_; }
    const TrustworthyCheckpoint& lastCheckpoint() const { return checkpoint_; }
private:
    struct Aligned { uint32_t count[2]={}; float fractionSide[2]={}; ImuEvidence imu; };
    bool align(const ImuEvidence& imu,Aligned& aligned) const;
    void integrate(const Evidence& evidence,bool force=false);
    MotionEvidence movement(const Evidence& evidence,bool terminal,bool logical=false) const;
    void finish(const Evidence& evidence);
    void lose(uint32_t reasons);
    SupervisorParameters p_;
    PoseEstimator pose_;
    MovementHistory history_;
    Decision decision_;
    TrustworthyCheckpoint checkpoint_;
    Pose nominal_,logicalGoal_;
    EncoderSnapshot samples_[8];
    uint8_t nextSample_=0,sampleCount_=0,warnings_=0,verifiedSamples_=0,recoveryAttempts_=0;
    Aligned aligned_,motionAligned_;
    ImuEvidence latestImu_,startImu_,logicalImu_;
    Lifecycle start_,terminal_,logicalStart_;
    MotionAssessment motion_;
    HistorySummary historySummary_;
    float routeScore_=1;
    uint32_t context_=0,step_=0,lastEvaluationMs_=0,latchedReasons_=0,poseSequence_=0,verificationSequence_=0,preparedAtMs_=0;
    bool haveAligned_=false,active_=false,pending_=false,prepared_=false,lost_=false,hold_=false,terminalAccepted_=true;
};
static_assert(sizeof(ConfidenceSupervisor)<4096,"CCPS persistent-state budget exceeded");
}
