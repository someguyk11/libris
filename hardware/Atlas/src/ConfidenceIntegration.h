#pragma once
#include "ConfidenceSupervisor.h"
#include "MotionController.h"
#include "SensorManager.h"
#include "ImuManager.h"
#include "SafetyController.h"
#include "CheckpointManager.h"
#include "Navigation.h"

// Only adapter layer knows hardware classes. No CCPS worker or network calls.
class ConfidenceIntegration {
public:
    ConfidenceIntegration(MotionController& motion,SensorManager& sensors,ImuManager& imu,SafetyController& safety,CheckpointManager& checkpoints,Navigation& navigation);
    void begin();
    void update(uint32_t nowMs);
    void apply(uint32_t nowMs);
    const AtlasConfidence::ConfidenceSupervisor& supervisor() const { return supervisor_; }
    AtlasConfidence::ConfidenceSupervisor& supervisor() { return supervisor_; }
    uint32_t maximumSliceUs() const { return maximumSliceUs_; }
    void context(uint32_t context,uint32_t step) { context_=context; step_=step; }
private:
    static void event(void* context,const MotionController::Event& event);
    static AtlasConfidence::SupervisorParameters parameters();
    MotionController& motion_; SensorManager& sensors_; ImuManager& imu_; SafetyController& safety_; CheckpointManager& checkpoints_;
    Navigation& navigation_;
    AtlasConfidence::ConfidenceSupervisor supervisor_;
    AtlasConfidence::EncoderSnapshot previous_;
    uint32_t context_=1,step_=0,lastUpdateMs_=0,maximumSliceUs_=0;
    bool sampled_=false;
};
