#pragma once
#include "ConfidenceTypes.h"
namespace AtlasConfidence {
struct HistorySummary {
    float confidence=0, distanceBias=0, yawBias=0, residualSlope=0;
    uint8_t eligible=0, interruptions=0, obstacles=0, disagreements=0;
    bool valid=false;
};
class MovementHistory {
public:
    static constexpr uint8_t Capacity=10;
    void push(const MovementRecord& record);
    const MovementRecord* at(uint8_t chronological) const;
    uint8_t size() const { return count_; }
    HistorySummary summarize(float distanceTolerance,float yawTolerance) const;
private:
    MovementRecord records_[Capacity];
    uint8_t next_=0,count_=0;
};
static_assert(sizeof(MovementHistory)<=648,"Fixed history exceeded RAM budget");
}
