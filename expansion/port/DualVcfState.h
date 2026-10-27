// GPL-2.0-or-later. Independent TPT state; no oscillator/source routing.
#pragma once
namespace xs {
struct SecondaryFilter {
 float highFilter[4]{},highG=0,highStep=0,resonanceStrength=0,resonanceStep=0,damping=2,dampingStep=0;
 float cachedCutoff=-1,cachedHighCoefficient=0,cachedDamping=-1,cachedCalibration=1;
 int cachedFilterType=-1,cachedFilterStages=0;
 SignalBlend<4> types;SignalBlend<2> slope;
};
struct DualFilterState {
 SecondaryFilter b;float oldA[4]{},oldB[4]{};
 int route=0,from=0,remaining=0;
};
}
