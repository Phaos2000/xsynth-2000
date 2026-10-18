// GPL-2.0-or-later. Per-user UI preference; never serialized with synth state.
#pragma once
#include "Theme.h"
#include <windows.h>
namespace xs::theme {
inline constexpr wchar_t PreferenceKey[]=L"Software\\Phaos\\XsynthExpansionDev\\UI";
inline Id load(const wchar_t *key) {
 if(!key)return Id::Steel;
 DWORD value=0,size=sizeof value;
 if(RegGetValueW(HKEY_CURRENT_USER,key,L"Theme",RRF_RT_REG_DWORD,nullptr,&value,&size)!=ERROR_SUCCESS || !valid(value))return Id::Steel;
 return Id(value);
}
inline bool save(const wchar_t *key,Id id) {
 if(!key || !valid(unsigned(id)))return false;
 HKEY handle{};
 if(RegCreateKeyExW(HKEY_CURRENT_USER,key,0,nullptr,0,KEY_SET_VALUE,nullptr,&handle,nullptr)!=ERROR_SUCCESS)return false;
 DWORD value=unsigned(id);
 auto status=RegSetValueExW(handle,L"Theme",0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof value);
 RegCloseKey(handle);return status==ERROR_SUCCESS;
}
} // namespace xs::theme
