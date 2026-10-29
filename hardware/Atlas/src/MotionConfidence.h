#pragma once
#include "ConfidenceTypes.h"
namespace AtlasConfidence {
class MotionConfidence {
public:
    static MotionAssessment evaluate(const MotionEvidence& evidence,const MotionLimits& limits);
};
}
