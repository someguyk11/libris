#include "MovementHistory.h"
namespace AtlasConfidence {
void MovementHistory::push(const MovementRecord& r) {
    records_[next_]=r; next_=(next_+1)%Capacity; if(count_<Capacity) ++count_;
}
const MovementRecord* MovementHistory::at(uint8_t i) const {
    return i<count_?&records_[(next_+Capacity-count_+i)%Capacity]:nullptr;
}
HistorySummary MovementHistory::summarize(float td,float ta) const {
    HistorySummary s;
    if(!positive(td)||!positive(ta)) return s;
    float wsum=0, sx=0, sy=0, sxx=0, sxy=0;
    for(uint8_t i=0;i<count_;++i) {
        const auto& r=*at(i);
        if(r.outcome!=Outcome::Complete) ++s.interruptions;
        if(r.flags&ObstacleInterruption) ++s.obstacles;
        if(r.flags&EncoderDisagreement) ++s.disagreements;
        bool latest=true;
        for(uint8_t j=i+1;j<count_;++j) if(at(j)->movementId==r.movementId) latest=false;
        if(!latest||!(r.flags&MeasurementValid)||!(r.flags&ResidualValid)||r.outcome!=Outcome::Complete||
           !isfinite(r.distanceResidualCm)||!isfinite(r.yawResidualRad)||!isfinite(r.motionConfidence)) continue;
        const float w=i+1, x=s.eligible++, d=r.distanceResidualCm/td, a=r.yawResidualRad/ta;
        const float residual=sqrtf(d*d+a*a);
        wsum+=w; s.confidence+=w*clamp01(r.motionConfidence);
        s.distanceBias+=w*d; s.yawBias+=w*a;
        sx+=w*x; sy+=w*residual; sxx+=w*x*x; sxy+=w*x*residual;
    }
    if(!wsum) return s;
    s.valid=true; s.confidence/=wsum; s.distanceBias/=wsum; s.yawBias/=wsum;
    const float denominator=wsum*sxx-sx*sx;
    if(s.eligible>=3&&denominator>0) s.residualSlope=(wsum*sxy-sx*sy)/denominator;
    return s;
}
}
