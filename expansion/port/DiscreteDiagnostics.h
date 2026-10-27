#pragma once
#include <cstdint>
namespace xs::discreteDiagnostics {
#ifdef XSYNTH_DISCRETE_DIAGNOSTICS
inline uint64_t counts[12]{};
#endif
}
#ifdef XSYNTH_DISCRETE_DIAGNOSTICS
#define XS_DISCRETE_COUNT(i,n) (xs::discreteDiagnostics::counts[i]+=(n))
#else
#define XS_DISCRETE_COUNT(i,n) ((void)0)
#endif
