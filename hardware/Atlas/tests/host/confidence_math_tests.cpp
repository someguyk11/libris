#include "ConfidenceTypes.h"
#include <cstdio>
#include <cstdlib>
using namespace AtlasConfidence;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if (!(x)) { std::printf("FAIL line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
int main() {
    CHECK(clamp01(NAN)==0); CHECK(clamp01(INFINITY)==0); CHECK(clamp01(-1)==0); CHECK(clamp01(2)==1);
    CHECK(fabsf(wrap(Pi)+Pi)<1e-6f); CHECK(fabsf(wrap(-Pi)+Pi)<1e-6f); CHECK(!isfinite(wrap(INFINITY)));
    float n=9; CHECK(!normalize(1,0,n)); CHECK(n==0); CHECK(!normalize(NAN,1,n)); CHECK(normalize(2,2,n)&&n==1);
    Freshness p{20,100,40}; CHECK(freshness(20,p)==1); CHECK(freshness(60,p)>0&&freshness(60,p)<1);
    CHECK(freshness(101,p)==0); CHECK(freshness(0,{0,0,0})==0);
    Constraint c{100,20,0,0,true}; CHECK(c.fresh(100)); CHECK(c.fresh(120)); CHECK(!c.fresh(121)); CHECK(!c.fresh(99));
    Decision current; current.reasons=Heading; current.movementId=42;
    const Decision queued=current; current.reasons=Missing; current.movementId=43;
    CHECK(queued.reasons==Heading); CHECK(queued.movementId==42); CHECK(sizeof(queued)==44);
    CHECK(contextTag("session","route")==contextTag("session","route"));
    CHECK(contextTag("a","bc")!=contextTag("ab","c"));
    std::printf("confidence_math: %u/%u checks passed\n",checks,checks);
}
