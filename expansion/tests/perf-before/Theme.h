// GPL-2.0-or-later. Authoritative palettes; shared procedural renderer, no geometry.
#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
namespace xs::theme {
enum class Id : unsigned { Steel, Win98, Industrial, Multimedia, Workstation2000, Sand };
inline constexpr unsigned Count=6;
inline constexpr const char *Names[]={"STEEL","98","INDUSTRIAL","MULTIMEDIA","2000","SAND"};
inline constexpr bool valid(unsigned id){return id<Count;}
struct Color {double r,g,b;};
constexpr Color unpack(uint32_t rgb){return {double((rgb>>16)&255),double((rgb>>8)&255),double(rgb&255)};}
inline uint32_t pack(Color c){return (uint32_t(c.r+.5)<<16)|(uint32_t(c.g+.5)<<8)|uint32_t(c.b+.5);}
inline Color blend(Color a,Color b,double t){return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};}
enum class DitherMode { FloydSteinberg };
struct Style {
 uint32_t chassisTop;
 uint32_t chassisBottom;
 uint32_t panelTop;
 uint32_t panelBottom;
 uint32_t headerTop;
 uint32_t headerBottom;
 uint32_t fieldFace;
 uint32_t fieldMid;
 uint32_t fieldShadow;
 uint32_t bevelHighlight;
 uint32_t bevelSecondary;
 uint32_t bevelShadow;
 uint32_t bevelDeepShadow;
 uint32_t primaryText;
 uint32_t secondaryText;
 uint32_t headerText;
 uint32_t readoutText;
 uint32_t knobHighlight;
 uint32_t knobMid;
 uint32_t knobShadow;
 uint32_t knobRim;
 uint32_t knobPointer;
 uint32_t activePrimary;
 uint32_t activeSecondary;
 uint32_t activeTertiary;
 uint32_t selectorTop;
 uint32_t selectorBottom;
 uint32_t selectorPressed;
 uint32_t selectorActiveBorder;
 uint32_t titleHighlight;
 uint32_t titleMid;
 uint32_t titleShadow;
 uint32_t titleDarkEdge;
 DitherMode ditherMode; double ditherStrength; int gradientSteps,knobSteps;
};
inline constexpr Style Styles[]={
 { // STEEL
  0x3E626A, // chassisTop
  0x19383E, // chassisBottom
  0x31535B, // panelTop
  0x173A43, // panelBottom
  0x1B4F5B, // headerTop
  0x07222A, // headerBottom
  0x082D35, // fieldFace
  0x326673, // fieldMid
  0x041216, // fieldShadow
  0xA5C3C2, // bevelHighlight
  0x83A9AB, // bevelSecondary
  0x0F2226, // bevelShadow
  0x09181D, // bevelDeepShadow
  0xE2E2D4, // primaryText
  0x7FD7D8, // secondaryText
  0x7FD7D8, // headerText
  0x7FD7D8, // readoutText
  0x7FB1A4, // knobHighlight
  0x4B857C, // knobMid
  0x174B4F, // knobShadow
  0x08181D, // knobRim
  0xEFF4E0, // knobPointer
  0x42B8D0, // activePrimary
  0x298A99, // activeSecondary
  0xD87747, // activeTertiary
  0x8B9996, // selectorTop
  0x345258, // selectorBottom
  0x305057, // selectorPressed
  0x42B8D0, // selectorActiveBorder
  0xC3DAD3, // titleHighlight
  0xBCC7C2, // titleMid
  0x172B2E, // titleShadow
  0x09191E, // titleDarkEdge
  DitherMode::FloydSteinberg,1.0,12,20
 },
 { // 98
  0xD2D2D2, // chassisTop
  0xAAAAAA, // chassisBottom
  0xC8C8C8, // panelTop
  0xB8B8B8, // panelBottom
  0x123CA4, // headerTop
  0x082773, // headerBottom
  0xF0F0F0, // fieldFace
  0xC7C7C7, // fieldMid
  0x777777, // fieldShadow
  0xFFFFFF, // bevelHighlight
  0xDFDFDF, // bevelSecondary
  0x808080, // bevelShadow
  0x3F3F3F, // bevelDeepShadow
  0x111111, // primaryText
  0x333333, // secondaryText
  0xF5F5F5, // headerText
  0x111111, // readoutText
  0xE2E2E2, // knobHighlight
  0xB7B7B7, // knobMid
  0x737373, // knobShadow
  0x303030, // knobRim
  0x111111, // knobPointer
  0x0A4AD1, // activePrimary
  0x1768E8, // activeSecondary
  0x33AFC0, // activeTertiary
  0xD8D8D8, // selectorTop
  0xB5B5B5, // selectorBottom
  0x999999, // selectorPressed
  0x0A4AD1, // selectorActiveBorder
  0xF3F3F0, // titleHighlight
  0xBEBEB9, // titleMid
  0x6C6C68, // titleShadow
  0x2C2C2A, // titleDarkEdge
  DitherMode::FloydSteinberg,0.45,18,24
 },
 { // INDUSTRIAL
  0x262A2B, // chassisTop
  0x101314, // chassisBottom
  0x202526, // panelTop
  0x15191A, // panelBottom
  0x191E1F, // headerTop
  0x090C0D, // headerBottom
  0x070909, // fieldFace
  0x101313, // fieldMid
  0x000000, // fieldShadow
  0x62696A, // bevelHighlight
  0x3D4445, // bevelSecondary
  0x090B0C, // bevelShadow
  0x000000, // bevelDeepShadow
  0xC4C5BE, // primaryText
  0x92958F, // secondaryText
  0xF29A21, // headerText
  0xF5A623, // readoutText
  0x676D6D, // knobHighlight
  0x404647, // knobMid
  0x161A1B, // knobShadow
  0x050606, // knobRim
  0xFF8316, // knobPointer
  0xF28A12, // activePrimary
  0xFFB12B, // activeSecondary
  0xA95709, // activeTertiary
  0x1B2020, // selectorTop
  0x101313, // selectorBottom
  0x2A2F2F, // selectorPressed
  0xA85A0E, // selectorActiveBorder
  0xF2A126, // titleHighlight
  0xC06C09, // titleMid
  0x633506, // titleShadow
  0x201200, // titleDarkEdge
  DitherMode::FloydSteinberg,0.35,20,24
 },
 { // MULTIMEDIA
  0x3A7A83, // chassisTop
  0x1F4F5A, // chassisBottom
  0x397882, // panelTop
  0x285C66, // panelBottom
  0x475AA2, // headerTop
  0x283675, // headerBottom
  0x15164D, // fieldFace
  0x212365, // fieldMid
  0x090A2D, // fieldShadow
  0x8DD5D5, // bevelHighlight
  0x688BAE, // bevelSecondary
  0x172D3D, // bevelShadow
  0x0B1725, // bevelDeepShadow
  0xE2EEF0, // primaryText
  0xB3CDE0, // secondaryText
  0xD3BFFF, // headerText
  0xD5C8FF, // readoutText
  0xA69AE5, // knobHighlight
  0x6D66BC, // knobMid
  0x343071, // knobShadow
  0x161440, // knobRim
  0xECE9FF, // knobPointer
  0x55DDE3, // activePrimary
  0x9B6CE8, // activeSecondary
  0xBD75E8, // activeTertiary
  0x4B4B9B, // selectorTop
  0x27276D, // selectorBottom
  0x353582, // selectorPressed
  0x704FB1, // selectorActiveBorder
  0xC7CDFF, // titleHighlight
  0x847CDF, // titleMid
  0x514A9B, // titleShadow
  0x242052, // titleDarkEdge
  DitherMode::FloydSteinberg,1.0,10,16
 },
 { // 2000
  0xD7DCE2, // chassisTop
  0xABB3BC, // chassisBottom
  0xC9CDD2, // panelTop
  0xB9BEC4, // panelBottom
  0x526B98, // headerTop
  0x344E7C, // headerBottom
  0xF1F2F3, // fieldFace
  0xD6D9DC, // fieldMid
  0x92989F, // fieldShadow
  0xFFFFFF, // bevelHighlight
  0xE6E8EA, // bevelSecondary
  0x7C838A, // bevelShadow
  0x42494F, // bevelDeepShadow
  0x151719, // primaryText
  0x4B5056, // secondaryText
  0xF4F5F7, // headerText
  0x151719, // readoutText
  0xDADBDD, // knobHighlight
  0xA9ADB1, // knobMid
  0x62676B, // knobShadow
  0x33373A, // knobRim
  0x1E2022, // knobPointer
  0x365FA8, // activePrimary
  0x3F76CB, // activeSecondary
  0x5E97C4, // activeTertiary
  0xD8DBDF, // selectorTop
  0xB2B7BD, // selectorBottom
  0x989FA6, // selectorPressed
  0x365FA8, // selectorActiveBorder
  0xF7F7F4, // titleHighlight
  0xBFC2BF, // titleMid
  0x777B7C, // titleShadow
  0x3E4345, // titleDarkEdge
  DitherMode::FloydSteinberg,0.2,28,28
 },
 { // SAND
  0xD4C7B3, // chassisTop
  0xB6A58C, // chassisBottom
  0xD0C2AD, // panelTop
  0xC2B299, // panelBottom
  0x84735B, // headerTop
  0x655641, // headerBottom
  0xE8DEC9, // fieldFace
  0xC5B69D, // fieldMid
  0x8C7B63, // fieldShadow
  0xF4EBD9, // bevelHighlight
  0xDED0B8, // bevelSecondary
  0x81715A, // bevelShadow
  0x4C4133, // bevelDeepShadow
  0x211D18, // primaryText
  0x5A5042, // secondaryText
  0xF1E7D4, // headerText
  0x211D18, // readoutText
  0xDED0B5, // knobHighlight
  0xB7A584, // knobMid
  0x76664E, // knobShadow
  0x443A2D, // knobRim
  0x272118, // knobPointer
  0x8F7047, // activePrimary
  0xB18A55, // activeSecondary
  0x665039, // activeTertiary
  0xD6C8AE, // selectorTop
  0xB7A68D, // selectorBottom
  0xA4937A, // selectorPressed
  0x796247, // selectorActiveBorder
  0xE7D9BE, // titleHighlight
  0xA58D69, // titleMid
  0x6D583D, // titleShadow
  0x392E21, // titleDarkEdge
  DitherMode::FloydSteinberg,0.7,14,20
 },
};
inline const Style &style(Id id){return Styles[valid(unsigned(id))?unsigned(id):0];}
// Preserve original tokens exactly in STEEL, including original gradient endpoints.
inline uint32_t rgb(uint32_t steel,Id id){
 if(id==Id::Steel)return steel;
 const auto &p=style(id);
 switch(steel){
 case 0x041216: return p.fieldShadow;
 case 0x07222A: return p.headerBottom;
 case 0x08181D: return p.knobRim;
 case 0x081B21: return p.bevelDeepShadow;
 case 0x082D35: return p.fieldFace;
 case 0x09181D: return p.bevelDeepShadow;
 case 0x09191E: return p.titleDarkEdge;
 case 0x091D23: return p.bevelDeepShadow;
 case 0x0A222A: return p.knobShadow;
 case 0x0C1F24: return p.bevelShadow;
 case 0x0D2F39: return p.headerBottom;
 case 0x0F2226: return p.bevelShadow;
 case 0x122B33: return p.selectorBottom;
 case 0x132A30: return p.bevelShadow;
 case 0x163133: return p.knobRim;
 case 0x172B2E: return p.knobShadow;
 case 0x173A43: return p.panelBottom;
 case 0x174B4F: return p.knobShadow;
 case 0x19383E: return p.chassisBottom;
 case 0x1B4F5B: return p.headerTop;
 case 0x1F414A: return p.bevelShadow;
 case 0x298A99: return p.activeSecondary;
 case 0x2A555F: return p.bevelSecondary;
 case 0x305057: return p.selectorPressed;
 case 0x31535B: return p.panelTop;
 case 0x32636D: return p.knobMid;
 case 0x326673: return p.fieldMid;
 case 0x345258: return p.selectorBottom;
 case 0x3E626A: return p.chassisTop;
 case 0x406C78: return p.bevelSecondary;
 case 0x42B8D0: return p.activePrimary;
 case 0x496C74: return p.headerTop;
 case 0x497E89: return p.bevelSecondary;
 case 0x4A737B: return p.knobMid;
 case 0x5B848D: return p.bevelSecondary;
 case 0x7FB1A4: return p.knobHighlight;
 case 0x7FD7D8: return p.readoutText;
 case 0x83A9AB: return p.bevelSecondary;
 case 0x8B9996: return p.selectorTop;
 case 0xA5C3C2: return p.bevelHighlight;
 case 0xACC7C3: return p.bevelHighlight;
 case 0xBCC7C2: return p.knobHighlight;
 case 0xC3DAD3: return p.bevelHighlight;
 case 0xD87747: return p.knobPointer;
 case 0xE2E2D4: return p.primaryText;
 case 0xEFF4E0: return p.knobPointer;
 default:return steel;
 }
}
inline Color colour(Color c,Id id){return id==Id::Steel?c:unpack(rgb(pack(c),id));}
inline uint32_t title(uint32_t source,Id id){
 if(id==Id::Steel)return source;
 const auto &p=style(id);auto c=unpack(source);
 double t=std::clamp((.2126*c.r+.7152*c.g+.0722*c.b)/255.,0.,1.)*3;
 const uint32_t stops[]={p.titleDarkEdge,p.titleShadow,p.titleMid,p.titleHighlight};
 int segment=std::min(2,int(t));return pack(blend(unpack(stops[segment]),unpack(stops[segment+1]),t-segment));
}
} // namespace xs::theme
