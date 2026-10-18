// GPL-2.0-or-later. Development identity; do not change released class IDs.
#pragma once
#include <cstdint>
namespace xs {
constexpr uint32_t HostIdBase=0x1000;
constexpr uint32_t hostId(int index){return HostIdBase+uint32_t(index);}
constexpr int parameterIndex(uint32_t id){return id>=HostIdBase && id<HostIdBase+Count?int(id-HostIdBase):-1;}
} // namespace xs
