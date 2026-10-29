#include "MovementHistory.h"
#include <cstdio>
using namespace AtlasConfidence;
#define CHECK(x) do { ++n; if(!(x)) { std::printf("history FAIL %d\n",__LINE__); return 1; } } while(0)
int main() {
    unsigned n=0; MovementHistory h; CHECK(!h.at(0)); CHECK(!h.summarize(1,1).valid);
    MovementRecord r; r.flags=MeasurementValid|ResidualValid; r.outcome=Outcome::Complete; r.motionConfidence=1;
    for(unsigned i=0;i<11;++i) { r.movementId=i; r.distanceResidualCm=static_cast<float>(i); h.push(r); }
    CHECK(h.size()==10); CHECK(h.at(0)->movementId==1); CHECK(h.at(9)->movementId==10); CHECK(!h.at(10));
    auto s=h.summarize(1,1); CHECK(s.valid&&s.eligible==10); CHECK(s.confidence==1); CHECK(s.residualSlope>0.9f);
    r.outcome=Outcome::Interrupted; r.flags=ObstacleInterruption; h.push(r);
    s=h.summarize(1,1); CHECK(s.obstacles==1); CHECK(s.interruptions==1); CHECK(s.confidence==1);
    CHECK(!h.summarize(0,1).valid); CHECK(sizeof(MovementRecord)==64);
    std::printf("movement_history: %u/%u checks passed\n",n,n);
}
