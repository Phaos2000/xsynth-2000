// GPL-2.0-or-later. New-instance choice, never decoder or saved-value defaults.
#pragma once
#include "Core.h"
namespace xs {
inline constexpr double NominalAmpDefault=.5;
inline State newInstanceState(){auto s=defaults();s[23]=NominalAmpDefault;return s;}
}
