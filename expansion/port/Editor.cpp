// GPL-2.0-or-later. Deterministic rendered controls; no image-generation
// assets.
#include "Editor.h"
#include "Version.h"
#include "TitleBitmap.h"
#include "Layout.h"
#include "DssiDialog.h"
#include "PresetBrowser.h"
#include "FactoryMenu.h"
#include "UiGeometry.h"
#include "ContextRegions.h"
#include "SelectorKind.h"
#include <commdlg.h>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <windowsx.h>
#include <shellapi.h>
namespace xs {
namespace {
HINSTANCE module() {
  HMODULE h{};
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&module), &h);
  return h;
}
struct Button {
  int x, y, id;
  const char *label;
};
constexpr Button buttons[] = {{ui::LoadPatch.x,ui::LoadPatch.y-48,-1,"LOAD PATCH"},{ui::SavePatch.x,ui::SavePatch.y-48,-2,"SAVE PATCH"}};
using Color=theme::Color;
Color mix(Color a, Color b, double t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
std::vector<Color> ramp(Color a, Color b, int steps) {
  std::vector<Color> colors;
  for (int i = 0; i < steps; ++i) {
    auto c = mix(a, b, double(i) / (steps - 1));
    colors.push_back({std::round(c.r), std::round(c.g), std::round(c.b)});
  }
  return colors;
}
} // namespace
struct Editor::Impl {
  Editor *owner;
  HWND window{};
  HDC dc{};
  HBITMAP image{}, old{};
  uint32_t *pixels{};
  int width = NativeWidth, height = NativeHeight;
  double scale = 1;
  ULONG_PTR gdiplus{};
  int drag = -100, startY = 0, startX=0;
  bool draggingAmount=false;
  int pressedAction = 0;
  int pressedParam=-1,lastPressedParam=-1;
  bool swallowLeftRelease=false;
  double startValue = 0;
  State last{};
  factory::Browser presets;
  std::string lastPresetLabel;
  std::wstring importedName;
  std::array<Control,18> routes=matrixControls();
  struct SectionCopy { std::array<double,9> values{}; int kind=0,count=0; } copied;
  uint32_t randomSeed=0x5068616f;
  HFONT font{}, header{}, presetFont{}, linkFont{};
  std::vector<uint32_t> background;
  bool dithering = true;
  bool aboutOpen = false;
  bool modalActive = false;
  unsigned highlightedRoutes=0,lastHighlightedRoutes=0;
  bool lastAboutOpen=false;
  int lastPressedAction=0;
  theme::Id themeId=theme::Id::Steel;
  std::wstring preferenceKey;
  bool themeOverride=false;
  void finishPointer(){
    if(drag>=0){int id=drag;drag=-100;owner->end(id);}
    draggingAmount=false;pressedAction=0;pressedParam=-1;
    if(GetCapture()==window)ReleaseCapture();
    if(window)InvalidateRect(window,nullptr,FALSE);
  }
  struct PopupScope {
    Impl& ui;bool prior;
    explicit PopupScope(Impl& value):ui(value),prior(value.modalActive){ui.modalActive=true;ui.finishPointer();}
    ~PopupScope(){
      ui.finishPointer();
      // Consume already-queued tail events owned by the popup/dismissal click.
      // No timers: the next new click remains immediately actionable.
      if(ui.window&&IsWindow(ui.window)){
        MSG tail{};
        while(PeekMessageW(&tail,ui.window,WM_LBUTTONDOWN,WM_LBUTTONDBLCLK,PM_REMOVE)){}
        while(PeekMessageW(&tail,ui.window,WM_RBUTTONDOWN,WM_RBUTTONDBLCLK,PM_REMOVE)){}
      }
      ui.modalActive=prior;
      if(!prior&&ui.window&&IsWindow(ui.window)&&(GetAsyncKeyState(VK_LBUTTON)&0x8000)){
        ui.swallowLeftRelease=true;SetCapture(ui.window);
        if(GetCapture()!=ui.window)ui.swallowLeftRelease=false;
      }
    }
  };
  const theme::Style &style() const {return theme::style(themeId);}
  static COLORREF ref(uint32_t rgb){return RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255);}
  COLORREF ink(COLORREF steel) const {
    return ref(theme::rgb((uint32_t(GetRValue(steel))<<16)|(uint32_t(GetGValue(steel))<<8)|GetBValue(steel),themeId));
  }
  Color tone(Color steel) const {return theme::colour(steel,themeId);}
  void changeTheme(theme::Id id,bool persist) {
    if(!theme::valid(unsigned(id)))return;
    if(themeId!=id){themeId=id;background.clear();if(window)InvalidateRect(window,nullptr,FALSE);}
    if(persist && !preferenceKey.empty() && !theme::save(preferenceKey.c_str(),id))
      MessageBoxW(window,L"The theme changed for this session, but Windows could not save the preference.",XSYNTH_WIDE(XSYNTH_PRODUCT_NAME),MB_OK|MB_ICONINFORMATION);
  }
  static constexpr wchar_t AboutUrl[] = L"https://phaos.neocities.org/";
  static constexpr wchar_t AboutEmail[] = L"phaos2000@pm.me";
  static constexpr wchar_t AboutMailto[] = L"mailto:phaos2000@pm.me";
  explicit Impl(Editor *e,const wchar_t *key) : owner(e),themeId(theme::load(key)),preferenceKey(key?key:L"") {
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&gdiplus, &input, nullptr);
  }
  ~Impl() {
    release();
    Gdiplus::GdiplusShutdown(gdiplus);
  }
  void release() {
    if (dc) {
      SelectObject(dc, old);
      DeleteObject(image);
      DeleteDC(dc);
      dc = nullptr;
    }
    if (font)
      DeleteObject(font);
    if (header)
      DeleteObject(header);
    if (presetFont)
      DeleteObject(presetFont);
    if (linkFont)
      DeleteObject(linkFont);
    font = header = presetFont = linkFont = nullptr;
  }
  int z(double n) const { return int(std::round(n * scale)); }
  void prepare() {
    release();
    background.clear();
    scale = width / double(NativeWidth);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    dc = CreateCompatibleDC(nullptr);
    image =
        CreateDIBSection(dc, &info, DIB_RGB_COLORS, reinterpret_cast<void **>(&pixels), nullptr, 0);
    old = HBITMAP(SelectObject(dc, image));
    font = CreateFontW(-z(10), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                       DEFAULT_PITCH, L"Tahoma");
    header = CreateFontW(-z(10), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                         DEFAULT_PITCH, L"Tahoma");
    presetFont = CreateFontW(-z(ui::PresetFontPixels), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            NONANTIALIASED_QUALITY, DEFAULT_PITCH, L"Tahoma");
    LOGFONTW linkStyle{};GetObjectW(font,sizeof(linkStyle),&linkStyle);
    linkStyle.lfUnderline=TRUE;linkFont=CreateFontIndirectW(&linkStyle);
  }
  void pixel(int x, int y, Color c) {
    if (x >= 0 && y >= 0 && x < width && y < height)
      pixels[y * width + x] = (uint32_t(std::clamp(c.r, 0., 255.)) << 16) |
                              (uint32_t(std::clamp(c.g, 0., 255.)) << 8) |
                              uint32_t(std::clamp(c.b, 0., 255.));
  }
  // Actual Floyd–Steinberg diffusion: right 7/16; below-left 3/16;
  // below 5/16; below-right 1/16. Error buffers are reset per material.
  template <class Shader>
  void material(double x, double y, double w, double h, const std::vector<Color> &palette, Shader shader) {
    GdiFlush();
    int l = z(x), t = z(y + 48), ww = z(w), hh = z(h);
    std::vector<Color> err(ww + 2), next(ww + 2);
    for (int row = 0; row < hh; ++row) {
      std::fill(next.begin(), next.end(), Color{});
      for (int col = 0; col < ww; ++col) {
        Color src;
        bool shade = true;
        bool visible = shader((col + .5) / scale, (row + .5) / scale, src, shade);
        if (!visible)
          continue;
        if (!dithering || !shade) {
          pixel(l + col, t + row, src);
          continue;
        }
        Color c{src.r + err[col + 1].r, src.g + err[col + 1].g, src.b + err[col + 1].b};
        // Quantize to this material's authored color ramp, rather than an
        // independent RGB cube that introduces unrelated colored speckles.
        Color q = palette.front();
        double nearest = 1.e30;
        for (const auto &candidate : palette) {
          double dr = c.r - candidate.r, dg = c.g - candidate.g, db = c.b - candidate.b;
          double distance = dr * dr + dg * dg + db * db;
          if (distance < nearest) {
            nearest = distance;
            q = candidate;
          }
        }
        pixel(l + col, t + row, q);
        Color diff{c.r - q.r, c.g - q.g, c.b - q.b};
        auto add = [&](Color &e, double n) {
          e.r += diff.r * n * style().ditherStrength;
          e.g += diff.g * n * style().ditherStrength;
          e.b += diff.b * n * style().ditherStrength;
        };
        add(err[col + 2], 7. / 16);
        add(next[col], 3. / 16);
        add(next[col + 1], 5. / 16);
        add(next[col + 2], 1. / 16);
      }
      err.swap(next);
    }
  }
  void gradient(double x, double y, double w, double h, Color a, Color b) {
    a=tone(a);b=tone(b);
    material(x, y, w, h, ramp(a, b, style().gradientSteps), [&](double, double yy, Color &c, bool &) {
      c = mix(a, b, yy / h);
      return true;
    });
  }
  void fill(int x, int y, int w, int h, COLORREF color) {
    RECT r{z(x), z(y + 48), z(x + w), z(y + h + 48)};
    auto brush = CreateSolidBrush(ink(color));
    FillRect(dc, &r, brush);
    DeleteObject(brush);
  }
  void line(double x, double y, double xx, double yy, COLORREF c, int thickness = 1) {
    auto pen = CreatePen(PS_SOLID, std::max(1, z(thickness)), ink(c));
    auto oldpen = SelectObject(dc, pen);
    MoveToEx(dc, z(x), z(y + 48), nullptr);
    LineTo(dc, z(xx), z(yy + 48));
    SelectObject(dc, oldpen);
    DeleteObject(pen);
  }
  void bevel(int x, int y, int w, int h, bool inset = false) {
    auto light = RGB(165, 195, 194), dark = RGB(15, 34, 38);
    if (inset)
      std::swap(light, dark);
    fill(x, y, w, 1, light);
    fill(x, y, 1, h, light);
    fill(x + w - 1, y, 1, h, dark);
    fill(x, y + h - 1, w, 1, dark);
  }
  void well(int x, int y, int w, int h) {
    fill(x, y, w, h, RGB(8, 45, 53));
    fill(x, y, w, 1, RGB(4, 18, 22));
    fill(x, y, 1, h, RGB(4, 18, 22));
    fill(x + w - 1, y, 1, h, RGB(50, 102, 115));
    fill(x, y + h - 1, w, 1, RGB(50, 102, 115));
  }
  void screw(int x, int y) {
    auto brush = CreateSolidBrush(ink(RGB(74, 115, 123)));
    auto pen = CreatePen(PS_SOLID, std::max(1, z(1)), ink(RGB(8, 27, 33)));
    auto oldBrush = SelectObject(dc, brush), oldPen = SelectObject(dc, pen);
    Ellipse(dc, z(x - 3), z(y + 48 - 3), z(x + 3), z(y + 48 + 3));
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
    line(x - 1, y - 2, x + 1, y - 2, RGB(172, 199, 195));
    line(x - 1, y + 1, x + 1, y - 1, RGB(12, 31, 36));
  }
  void text(int x, int y, int w, int h, const char *s, COLORREF c = RGB(226, 226, 212),
            bool bold = false) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, ink(c));
    auto f = SelectObject(dc, bold ? header : font);
    RECT r{z(x), z(y + 48), z(x + w), z(y + h + 48)};
    DrawTextA(dc, s, -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dc, f);
  }
  void knob(int x, int y, int id, const char *label, const State &s, bool staticOnly = false, int valueWidth = 54) {
    double n = s[id];
    n=stepped(id,n);
    if (staticOnly) {
      for (int k = 0; k < 11; ++k) {
        double angle = (1.25 - (k / 10.) * 1.5) * Pi;
        double cx = x + 15 + 21 * std::cos(angle), cy = y + 15 - 21 * std::sin(angle);
        auto brush = CreateSolidBrush(ink(RGB(66, 184, 208)));
        auto ob = SelectObject(dc, brush);
        auto op = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, z(cx - 1.25), z(cy - 1.25 + 48), z(cx + 1.5), z(cy + 1.5 + 48));
        SelectObject(dc, op);
        SelectObject(dc, ob);
        DeleteObject(brush);
      }
      const auto dark=tone({23,75,79}),light=tone({127,177,164}),mid=theme::unpack(style().knobMid);
      const auto rim=tone({8,24,29}),edgeDark=tone({10,34,42}),edgeLight=tone({50,99,109});
      const auto bevelLight=tone({188,199,194}),bevelDark=tone({23,43,46});
      auto shadeKnob=[&](double t){return themeId==theme::Id::Steel?mix(dark,light,t):
        t<.5?mix(dark,mid,t*2):mix(mid,light,(t-.5)*2);};
      auto palette=ramp(dark,light,style().knobSteps);
      if(themeId!=theme::Id::Steel)for(size_t i=0;i<palette.size();++i){auto c=shadeKnob(double(i)/(palette.size()-1));palette[i]={std::round(c.r),std::round(c.g),std::round(c.b)};}
      material(x - 3, y - 3, 36, 36, palette,
               [&](double xx, double yy, Color &c, bool &shade) {
                 double dx = xx - 18, dy = yy - 18, r = std::sqrt(dx * dx + dy * dy);
                 if (r > 18)
                   return false;
                 if (r > 17) {
                   c = rim;
                   shade = false;
                   return true;
                 }
                 if (r > 16) {
                   c = mix(edgeDark,edgeLight, std::clamp((dy + 17) / 34, 0., 1.));
                   shade = false;
                   return true;
                 }
                 if (r > 14) {
                   shade = false;
                   c = mix(bevelLight,bevelDark,
                           std::clamp((dy + .3 * dx + 16) / 32, 0., 1.));
                   return true;
                 }
                 double lighting = std::clamp(
                     .56 - dx / 65 - dy / 60 + .07 * std::exp(-std::pow((dy + 9) / 3, 2)), 0., 1.);
                 c = shadeKnob(lighting);
                 return true;
               });
      return;
    }
    double angle = (1.25 - n * 1.5) * Pi;
    line(x + 16 + 3 * std::cos(angle), y + 16 - 3 * std::sin(angle), x + 16 + 13 * std::cos(angle),
         y + 16 - 13 * std::sin(angle), RGB(22, 49, 51), 2);
    line(x + 15 + 3 * std::cos(angle), y + 15 - 3 * std::sin(angle), x + 15 + 13 * std::cos(angle),
         y + 15 - 13 * std::sin(angle), stepsFor(id) ? RGB(216, 119, 71) : RGB(239, 244, 224), 2);
    const Control geometry{id,x,y,40,label,ControlKind::Knob,valueWidth};
    const auto labelRect=labelBounds(geometry),valueRect=fieldBounds(geometry);
    text(labelRect.x,labelRect.y,labelRect.w,labelRect.h,label);
    const int valueX=valueRect.x,valueY=valueRect.y;
    well(valueX,valueY,valueRect.w,valueRect.h);
    char value[64]{}; valueText(id,s,value,sizeof value);
    text(valueX,valueY,valueWidth,moduleGrid::ValueHeight,value, RGB(127, 215, 216));
    if(modulated(s,id)) {
      auto bounds=markerBounds(x,y,label);
      auto brush=CreateSolidBrush(ink(RGB(66,184,208)));FillRect(dc,&bounds,brush);DeleteObject(brush);
    }
  }
  RECT markerBounds(int x,int y,const char *label) {
    auto previous=SelectObject(dc,font);SIZE extent{};
    GetTextExtentPoint32A(dc,label,int(std::strlen(label)),&extent);SelectObject(dc,previous);
    int left=z(x+15)+int(std::ceil(extent.cx/2.))+z(3),top=z(y+48+39);
    return {left,top,left+std::max(1,z(3)),top+std::max(1,z(3))};
  }
  void hoverRoutes(POINT point) {
    unsigned mask=0;auto state=owner->read();
    for(const auto &c:mainControls)if(c.kind==ControlKind::Knob) {
      unsigned routes=modulationRoutes(state,effectiveId(c,state));if(!routes)continue;
      auto bounds=markerBounds(c.x,c.y,c.label);InflateRect(&bounds,z(2),z(2));
      if(contains(bounds,point)){mask=routes;break;}
    }
    if(mask!=highlightedRoutes){highlightedRoutes=mask;InvalidateRect(window,nullptr,FALSE);}
    if(mask){TRACKMOUSEEVENT track{sizeof track,TME_LEAVE,window,0};TrackMouseEvent(&track);}
  }
  void valueText(int id,const State &s,char *out,size_t size) {
    if(id==26 && s[LfoRandom]>0) { std::snprintf(out,size,"%s",s[LfoRandom]<.75?"S&H":"SMOOTH");return; }
    if(id==Keytrack || id==Velocity1 || id==Velocity2 || id==XDepth) {
      std::snprintf(out,size,"%.0f%%",s[id]*100);return;
    }
    if(id>=34) { extraText(id,s[id],out,size);return; }
    if(id>=24 && id<27) {
      const char *waves[]={"SINE","TRI","SAW UP","SAW DN","SQUARE","PULSE"};
      std::snprintf(out,size,"%s",waves[discrete(id,s[id])]);
    } else if(id==28)std::snprintf(out,size,"%s",s[id]<.5?"12 dB/oct":"24 dB/oct");
    else if(id>=27)std::snprintf(out,size,"%s",s[id]<.5?"OFF":"ON");
    else if(id==0 || id==2)std::snprintf(out,size,"%+.1f st",12*std::log2(physical(id,s[id])));
    else if(id==4)std::snprintf(out,size,"%.2f Hz",physical(id,s[id]));
    else if(id==8) { double c=physical(id,s[id]);std::snprintf(out,size,"%.0f ms",c>=1?0:-256000/(44100*std::log1p(-c))); }
    else if(id==9 || id==10 || id==12 || id==15 || id==16 || id==18)std::snprintf(out,size,"%.1f ms",-4605.17/(44100*std::log1p(-physical(id,s[id]))));
    else if(id==21)std::snprintf(out,size,"%.0f Hz",440*physical(id,s[id]));
    else std::snprintf(out,size,"%.0f%%",s[id]*100);
  }
  void selector(const Control &c,const State &s) {
    const auto labelRect=labelBounds(c),field=fieldBounds(c);
    text(labelRect.x,labelRect.y,labelRect.w,labelRect.h,c.label);
    bool dropdown=isDropdown(c,s),pressed=pressedParam==c.id;
    if(dropdown)well(field.x,field.y,field.w,field.h);
    else {
      // Match the existing half-open field edges at fractional DPI, too.
      gradient(field.x,field.y,(z(field.x+field.w)-z(field.x))/scale,
               (z(field.y+48+field.h)-z(field.y+48))/scale,{139,153,150},{52,82,88});
      bevel(field.x,field.y,field.w,field.h,pressed);
    }
    char value[96]{}; valueText(c.id,s,value,sizeof value);
    // Reserve the arrow gutter symmetrically: value, label and field now share
    // their exact centre, including narrow Voices and modulation source fields.
    text(c.x+moduleGrid::SelectorTextInset,c.y+2+(pressed?1:0),c.w-2*moduleGrid::SelectorTextInset,
         moduleGrid::SelectorHeight-4,value,RGB(127,215,216));
    if(dropdown){line(c.x+c.w-8,c.y+7,c.x+c.w-5,c.y+10,RGB(127,215,216));
    line(c.x+c.w-5,c.y+10,c.x+c.w-2,c.y+7,RGB(127,215,216));}
  }
  void toggle(const Control &c,const State &s) {
    bool on=s[c.id]>=.5;
    gradient(c.x,c.y,c.w,22,{139,153,150},{52,82,88});
    bevel(c.x,c.y,c.w,22,on||pressedParam==c.id);
    gradient(c.x+3,c.y+3,c.w-6,16,on?Color{66,184,208}:Color{48,80,87},
             on?Color{41,138,153}:Color{18,43,51});
    bevel(c.x+3,c.y+3,c.w-6,16,true);
    text(c.x+3,c.y+4+(pressedParam==c.id?1:0),c.w-6,14,on?"ON":"OFF",on?RGB(8,45,53):RGB(226,226,212));
    const auto labelRect=labelBounds(c);
    text(labelRect.x,labelRect.y,labelRect.w,labelRect.h,c.label);
  }
  void amount(const Control &c,const State &s) {
    well(c.x,c.y,c.w,22);
    int destination=discrete(c.id-1,s[c.id-1]);
    char value[32]{};
    bool pitch=destination==0 || destination==3,octaves=destination==8 || destination==10;
    std::snprintf(value,sizeof value,pitch?"%+.1f st":octaves?"%+.2f oct":"%+.1f%%",(s[c.id]*2-1)*(pitch?48:octaves?4:100));
    text(c.x+2,c.y+1,c.w-4,16,value,RGB(127,215,216));
    int center=c.x+c.w/2,endpoint=c.x+5+int(std::round(s[c.id]*(c.w-10)));
    line(c.x+5,c.y+18,c.x+c.w-5,c.y+18,RGB(50,102,115));
    line(center,c.y+16,center,c.y+21,RGB(165,195,194));
    if(endpoint!=center)fill(std::min(center,endpoint),c.y+18,std::abs(endpoint-center),2,RGB(66,184,208));
  }
  const Control *controlAt(int x,int y) const {
    for(const auto &c:mainControls) {
      if(c.kind==ControlKind::Selector || c.kind==ControlKind::Toggle) { if(x>=c.x && x<c.x+c.w && y>=c.y && y<c.y+(c.kind==ControlKind::Toggle?22:18))return &c; }
      else if(x>=c.x-5 && x<c.x+40 && y>=c.y-5 && y<c.y+63)return &c;
    }
    for(const auto &c:routes)if(x>=c.x && x<c.x+c.w && y>=c.y && y<c.y+(c.kind==ControlKind::Amount?22:18))return &c;
    return nullptr;
  }
  void select(const Control &c) {
    auto state=owner->read(); int id=effectiveId(c,state);
    bool lfoWave=id==26; int count=selectorCount(c,state);
    int current=lfoWave && state[LfoRandom]>0?5+discrete(LfoRandom,state[LfoRandom]):discrete(id,state[id]);
    int chosen=(current+1)%count;
    if(isDropdown(c,state)) {
      HMENU menu=CreatePopupMenu();
      for(int i=0;i<count;++i) {
        char label[96]{}; auto candidate=state;
        if(lfoWave) { candidate[LfoRandom]=i>=6?(i-5)/2.:0; if(i<6)candidate[26]=i/5.; }
        else candidate[id]=double(i)/(count-1);
        valueText(id,candidate,label,sizeof label);
        AppendMenuA(menu,MF_STRING|(i==current?MF_CHECKED:0),i+1,label);
      }
      POINT point{z(c.x),z(c.y+48+18)};ClientToScreen(window,&point);
      PopupScope modal(*this);
      UINT result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,window,nullptr);
      DestroyMenu(menu); if(!result)return;chosen=int(result)-1;
    }
    auto change=[&](int p,double value){owner->begin(p);edit(p,value);owner->end(p);};
    if(lfoWave) { change(LfoRandom,chosen>=6?(chosen-5)/2.:0); if(chosen<6)change(26,chosen/5.); }
    else change(id,double(chosen)/(count-1));
  }
  int titleScale() const { return std::max(1, int(std::floor(scale))); }
  // Center the visible bitmap in the 48-unit header; match its left inset.
  // An odd pixel remainder stays below the logo (at most one pixel difference).
  int titleInset() const { return std::max(0, (z(48) - title::Height * titleScale()) / 2); }
  int titleBaseline() const { return titleInset() + title::Height * titleScale(); }
  int versionCenter() const { return titleInset() + title::Width * titleScale() + z(25); }
  RECT logoBounds() const {
    return {titleInset(), titleInset(), titleInset() + title::Width * titleScale(),
            titleBaseline()};
  }
  RECT aboutBounds() const { return {z(262), z(100), z(698), z(516)}; }
  RECT themeBounds() const {auto b=ui::ThemeButton;return {z(b.x),z(b.y),z(b.x+b.w),z(b.y+b.h)};}
  RECT aboutLinkBounds(const wchar_t *label,int y) {
    auto previous = SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, label, int(std::wcslen(label)), &size);
    SelectObject(dc, previous);
    return {(width - size.cx) / 2, z(y), (width + size.cx + 1) / 2, z(y+16)};
  }
  static bool contains(RECT r, POINT p) { return PtInRect(&r, p) != FALSE; }
  void bitmapTitle(int left, int top, int step, bool shadow = false) {
    // Native source pixels only. Integer replication keeps every edge, shade,
    // highlight and cell advance intact, even at fractional host DPI factors.
    GdiFlush();
    // One native pixel of hard underside/right-edge shadow, clipped to the
    // original title footprint. Padding and every opaque source pixel survive.
    // No filtering, alpha blur, resampling or change to the authored bitmap.
    if(shadow)for(int y=0;y<title::Height-1;++y)
      for(int x=0;x<title::Width-1;++x) {
        if(!(title::Pixels[y*title::Width+x]>>24))continue;
        for(int dy=0;dy<step;++dy)for(int dx=0;dx<step;++dx) {
          int xx=left+(x+1)*step+dx,yy=top+(y+1)*step+dy;
          if(xx>=0 && xx<width && yy>=0 && yy<height)pixels[yy*width+xx]=theme::rgb(0x09191e,themeId);
        }
      }
    for (int y = 0; y < title::Height; ++y)
      for (int x = 0; x < title::Width; ++x) {
        const auto color = title::Pixels[y * title::Width + x];
        if (!(color >> 24)) continue;
        for (int dy = 0; dy < step; ++dy)
          for (int dx = 0; dx < step; ++dx) {
            const int xx = left + x * step + dx, yy = top + y * step + dy;
            if (xx >= 0 && xx < width && yy >= 0 && yy < height)
              pixels[yy * width + xx] = theme::title(color & 0xffffff,themeId);
          }
      }
  }
  void logo() { bitmapTitle(titleInset(), titleInset(), titleScale(), true); }
  void headerMaterial(double x,double y,double w,double h,bool contained=false) {
    gradient(x,y,w,h,{27,79,91},{7,34,42});
    if(contained){RECT top{z(x),z(y+48),z(x+w),z(y+48)+std::max(1,z(1))},bottom{z(x),z(y+48+h)-std::max(1,z(1)),z(x+w),z(y+48+h)};auto brush=CreateSolidBrush(ink(RGB(9,29,35)));FillRect(dc,&top,brush);DeleteObject(brush);brush=CreateSolidBrush(ink(RGB(73,126,137)));FillRect(dc,&bottom,brush);DeleteObject(brush);return;}
    line(x,y,x+w,y,RGB(9,29,35));
    line(x,y+h,x+w,y+h,RGB(73,126,137));
  }
  void headerMaterialPixels(Bounds b) {headerMaterial(b.x/scale,b.y/scale-48,b.w/scale,b.h/scale,true);}
  void themeButton() {
    const auto b=ui::ThemeButton;const bool pressed=pressedAction==-3;
    gradient(b.x,b.y-48,b.w,b.h,{139,153,150},{52,82,88});bevel(b.x,b.y-48,b.w,b.h,pressed);
    std::string label=std::string("THEME: ")+theme::Names[unsigned(themeId)];
    text(b.x,b.y-44+(pressed?1:0),b.w,16,label.c_str());
  }
  void aboutText(int y, const wchar_t *value, COLORREF color = RGB(226, 226, 212),bool link=false) {
    auto previous = SelectObject(dc, link?linkFont:font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, ink(color));
    RECT r{z(274), z(y), z(686), z(y + 16)};
    DrawTextW(dc, value, -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dc, previous);
  }
  void aboutPanel() {
    // A stepped steel rim raises the face above the surrounding modules.
    // Keep every edge inside the existing panel bounds, with no blurred shadow.
    fill(262, 52, 436, 416, RGB(9, 25, 30));
    const COLORREF light[] = {RGB(195, 218, 211), RGB(131, 169, 171),
                              RGB(91, 132, 141), RGB(64, 108, 120)};
    const COLORREF dark[] = {RGB(9, 24, 29), RGB(19, 42, 48),
                             RGB(31, 65, 74), RGB(42, 85, 95)};
    for (int i = 1; i <= 4; ++i) {
      fill(262 + i, 52 + i, 436 - 2 * i, 1, light[i - 1]);
      fill(262 + i, 52 + i, 1, 416 - 2 * i, light[i - 1]);
      fill(697 - i, 52 + i, 1, 416 - 2 * i, dark[i - 1]);
      fill(262 + i, 467 - i, 436 - 2 * i, 1, dark[i - 1]);
    }
    gradient(267, 57, 426, 406, {49, 83, 91}, {23, 58, 67});
    for (int x : {270, 690})
      for (int y : {62, 458})
        screw(x, y);
    const auto plaque=ui::plaque(width,scale),art=ui::aboutLogo(width,scale);
    // Invert only the coordinate transform; bitmap replication remains integer.
    headerMaterialPixels(plaque);
    bitmapTitle(art.x,art.y,titleScale(),true);
    aboutText(ui::CreditY[0], L"Version " XSYNTH_WIDE(XSYNTH_VERSION));
    aboutText(ui::CreditY[1], L"Developed by Phaos", RGB(127, 215, 216));
    aboutText(ui::CreditY[2], AboutUrl, RGB(66, 184, 208),true);
    aboutText(ui::AboutEmailY, AboutEmail, RGB(66, 184, 208),true);
    aboutText(ui::CreditY[3], L"2026");
    aboutText(ui::CreditY[4], L"Based on Xsynth 1.0.2");
    aboutText(ui::CreditY[5], L"by Steve J. Brookes, 1999");
    aboutText(ui::CreditY[6], L"Original Xsynth code");
    aboutText(ui::CreditY[7], L"Copyright \u00a9 1999 S. J. Brookes");
    aboutText(ui::CreditY[8], L"Xsynth 2000 synthesis development,");
    aboutText(ui::CreditY[9], L"Windows VST3 implementation and GUI");
    aboutText(ui::CreditY[10], L"Copyright \u00a9 2026 Phaos");
    aboutText(ui::CreditY[11], L"Based on the GPL-licensed Xsynth source.");
    themeButton();
  }
  void headerButtons(bool pressed) {
    for(const auto &b:buttons) {
      gradient(b.x,b.y,104,24,{139,153,150},{52,82,88});bevel(b.x,b.y,104,24,pressed && pressedAction==b.id);
      text(b.x,b.y+4,104,16,b.label);
    }
  }
  void presetStrip(const std::string& label) {
    auto button=[&](Bounds b,bool previous){
      gradient(b.x,b.y-48,b.w,b.h,{139,153,150},{52,82,88});bevel(b.x,b.y-48,b.w,b.h);
      int center=b.x+b.w/2,yy=b.y-48+b.h/2;
      for(int row=-4;row<=4;++row){int length=5-std::abs(row);int x=previous?center+2-length:center-2;line(x,yy+row,x+length,yy+row,RGB(226,226,212));}
    };
    button(ui::PresetPrevious,true);button(ui::PresetNext,false);
    const auto b=ui::PresetField;well(b.x,b.y-48,b.w,b.h);
    auto oldFont=SelectObject(dc,presetFont);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,ref(style().readoutText));
    RECT rect{z(b.x+ui::PresetTextLeft),z(b.y+3),z(b.x+b.w-ui::PresetTextRight),z(b.y+b.h-3)};
    auto displayed=dssi::wide(label);
    DrawTextW(dc,displayed.c_str(),-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);SelectObject(dc,oldFont);
    line(b.x+b.w-13,b.y-48+10,b.x+b.w-10,b.y-48+13,ref(style().readoutText));line(b.x+b.w-10,b.y-48+13,b.x+b.w-7,b.y-48+10,ref(style().readoutText));
  }
  void factoryPreset(int index) {
    if(index!=factory::Init&&(index<0||index>=factory::Count))return;
    applyPatch(index==factory::Init?factory::initState():factory::Presets[index].state);presets.select(index);
    importedName=index==factory::Init?L"INIT":dssi::wide(factory::Presets[index].name);
    for(auto& c:importedName)if(c<32||wcschr(L"<>:\"/\\|?*",c))c=L'_';
    InvalidateRect(window,nullptr,FALSE);
  }
  void presetMenu() {
    PopupScope modal(*this);
    HMENU menu=factory::createMenu(presets.current());
    POINT point{z(ui::PresetField.x),z(ui::PresetField.y+ui::PresetField.h)};ClientToScreen(window,&point);
    UINT chosen=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,window,nullptr);DestroyMenu(menu);
    if(chosen==factory::InitCommand)factoryPreset(factory::Init);
    else if(chosen>=1000&&chosen<1000+factory::Count)factoryPreset(int(chosen)-1000);
  }
  void headerStrip() {
    gradient(2, -46, NativeWidth-4, 44, {73, 108, 116}, {13, 47, 57});
    bevel(2, -46, NativeWidth-4, 44);
    headerButtons(false);
    logo();
    auto previousFont = SelectObject(dc, font);
    auto previousAlignment = SetTextAlign(dc, TA_CENTER | TA_BASELINE);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, themeId==theme::Id::Steel?RGB(226,226,212):ref(style().headerText));
    // The user requested the original visible version, separately from port metadata.
    TextOutA(dc, versionCenter(), titleBaseline(), XSYNTH_VERSION_LABEL, int(sizeof(XSYNTH_VERSION_LABEL)-1));
    SetTextAlign(dc, previousAlignment);
    SelectObject(dc, previousFont);
  }
  void render(const State &s) {
    if (!dc)
      prepare();
    const auto presetLabel=presets.label(s);
    if(!background.empty() && s==last && presetLabel==lastPresetLabel && aboutOpen==lastAboutOpen &&
       pressedAction==lastPressedAction && pressedParam==lastPressedParam && highlightedRoutes==lastHighlightedRoutes)return;
    GdiFlush();
    if (background.empty()) {
      gradient(0,-48,NativeWidth,NativeHeight,{62,98,106},{25,56,62});
      for(const auto &p:panels) {
        gradient(p.x+2,p.y+2,p.w-4,p.h-4,{49,83,91},{23,58,67});
        const auto shell=faceGrid::shell(p),headerRect=faceGrid::header(p);
        bevel(shell.x,shell.y,shell.w,shell.h);
        headerMaterial(headerRect.x,headerRect.y,headerRect.w,headerRect.h);
        text(headerRect.x,headerRect.y,headerRect.w,headerRect.h,p.label,ref(style().headerText),true);
        screw(p.x+6,p.y+11);screw(p.x+p.w-6,p.y+11);
        screw(p.x+6,p.y+p.h-6);screw(p.x+p.w-6,p.y+p.h-6);
      }
      for(const auto &c:mainControls)if(c.kind==ControlKind::Knob)knob(c.x,c.y,effectiveId(c,s),c.label,s,true);
      headerStrip();
      GdiFlush();background.assign(pixels,pixels+width*height);
    } else std::copy(background.begin(),background.end(),pixels);
    for(const auto &c:mainControls) {
      if(c.kind==ControlKind::Knob)knob(c.x,c.y,effectiveId(c,s),c.label,s,false,c.valueWidth);
      else if(c.kind==ControlKind::Toggle)toggle(c,s);
      else selector(c,s);
    }
    for(const auto &c:routes) {
      if(c.kind==ControlKind::Amount)amount(c,s);
      else { auto field=c;field.label="";selector(field,s); }
    }
    const char *headings[]={"SOURCE","DESTINATION","AMOUNT"};
    for(int half=0;half<2;++half)for(int column=0;column<3;++column)
      text(half*matrixGrid::HalfWidth+matrixGrid::FieldX[column],MatrixFirstRow-15,matrixGrid::FieldWidth[column],12,headings[column],ref(style().secondaryText));
    for(int i=0;i<6;++i) {
      int offset=matrixGrid::origin(i),y=matrixGrid::row(i);char number[4]{};std::snprintf(number,sizeof number,"%d",i+1);
      text(offset+10,y,14,moduleGrid::SelectorHeight,number,ref(style().secondaryText));text(offset+132,y,14,moduleGrid::SelectorHeight,">");
    }
    if(modulated(s,-1))fill(144,8,3,3,RGB(66,184,208));
    if(modulated(s,-2))fill(452,8,3,3,RGB(66,184,208));
    if(pressedAction)headerButtons(true);
    presetStrip(presetLabel);
    for(int slot=0;slot<6;++slot)if(highlightedRoutes&(1u<<slot)) {
      int offset=matrixGrid::origin(slot),y=matrixGrid::row(slot);
      line(offset+28,y-2,offset+470,y-2,RGB(127,215,216));
      line(offset+28,y+24,offset+470,y+24,RGB(127,215,216));
    }
    if (aboutOpen)
      aboutPanel();
    last = s;lastAboutOpen=aboutOpen;lastPressedAction=pressedAction;lastPressedParam=pressedParam;lastHighlightedRoutes=highlightedRoutes;lastPresetLabel=presetLabel;
  }
  void edit(int id, double value) {
    value = std::clamp(value, 0., 1.);
    value=stepped(id,value);
    owner->change(id, value);
    if (window)
      InvalidateRect(window, nullptr, FALSE);
  }
  void sectionMenu(int x,int y) {
    const int section=context::sectionAt(x,y,width,height);
    if(section<0)return;
    PopupScope modal(*this);
    static constexpr int ids[4][9]={{0,1,24,Engine1,-1,-1,-1,-1,-1},
      {2,3,25,Engine2,-1,-1,-1,-1,-1},{9,10,11,12,Curve1,Velocity1,13,14,-1},
      {15,16,17,18,Curve2,Velocity2,19,20,Loop2}};
    int count=section<2?4:section==2?8:9,kind=section<2?1:2;
    HMENU menu=CreatePopupMenu(),mutate=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING,1,L"Copy");
    AppendMenuW(menu,MF_STRING|(copied.kind==kind?0:MF_GRAYED),2,L"Paste");
    AppendMenuW(menu,MF_STRING|(copied.kind==kind?0:MF_GRAYED),3,kind==1?L"Paste to other oscillator":L"Paste to other envelope");
    if(kind==1)AppendMenuW(menu,MF_STRING,4,L"Swap VCO1 / VCO2");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING,5,L"Reset");AppendMenuW(menu,MF_STRING,6,L"Randomize");
    AppendMenuW(mutate,MF_STRING,10,L"5%");AppendMenuW(mutate,MF_STRING,11,L"10%");
    AppendMenuW(mutate,MF_STRING,12,L"20%");AppendMenuW(mutate,MF_STRING,13,L"40%");
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(mutate),L"Mutate...");
    POINT point{};GetCursorPos(&point);
    UINT command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,point.x,point.y,0,window,nullptr);
    DestroyMenu(menu); if(!command)return;
    auto state=owner->read();
    auto change=[&](int id,double value){owner->begin(id);edit(id,value);owner->end(id);};
    if(command==1) { copied.kind=kind;copied.count=count;for(int i=0;i<count;++i)copied.values[i]=state[ids[section][i]];return; }
    if(command==4) { for(int i=0;i<4;++i){change(ids[0][i],state[ids[1][i]]);change(ids[1][i],state[ids[0][i]]);}return; }
    if(command==2 || command==3) {
      int target=command==3?(section^1):section,n=target<2?4:target==2?8:9;
      for(int i=0;i<n;++i)change(ids[target][i],i<copied.count?copied.values[i]:defaults()[ids[target][i]]);
      return;
    }
    auto random=[&]{randomSeed^=randomSeed<<13;randomSeed^=randomSeed>>17;randomSeed^=randomSeed<<5;return (randomSeed>>8)/16777216.;};
    for(int i=0;i<count;++i) {
      int id=ids[section][i]; double value=state[id];
      if(command==5)value=defaults()[id];
      else if(command==6)value=random();
      else {
        double amount=command==10?.05:command==11?.1:command==12?.2:.4;
        if(stepsFor(id)) { if(random()<amount)value=random(); }
        else value+=(random()*2-1)*amount;
      }
      change(id,value);
    }
  }
  void applyPatch(const State& s) {
    // Parsing and all validation have completed before any parameter is edited.
    for(int i=0;i<Count;++i) {
      if(i==30||i==31||i==AllNotesOff||i==AllSoundsOff||i==Pressure)continue;
      owner->begin(i);edit(i,s[i]);owner->end(i);
    }
  }
  void importDssi() {
    PopupScope modal(*this);
    wchar_t path[MAX_PATH]{};OPENFILENAMEW f{sizeof f};
    f.hwndOwner=window;f.lpstrTitle=L"Import Xsynth-DSSI Bank...";
    f.lpstrFilter=L"Xsynth-DSSI banks (including extensionless banks)\0*.*\0\0";
    f.lpstrFile=path;f.nMaxFile=MAX_PATH;f.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
    if(!GetOpenFileNameW(&f))return;
    try {
      std::ifstream input(path,std::ios::binary|std::ios::ate);
      if(!input||input.tellg()<0||input.tellg()>dssi::MaxBytes) {
        MessageBoxW(window,L"Could not read bank (maximum 4 MiB).",XSYNTH_WIDE(XSYNTH_PRODUCT_NAME),MB_ICONERROR);return;
      }
      std::string text(size_t(input.tellg()),'\0');input.seekg(0);
      if(!input.read(text.data(),text.size()))return;
      auto bank=dssi::parse(text);
      if(!bank.error.empty()){MessageBoxW(window,dssi::wide(bank.error).c_str(),L"Xsynth-DSSI import",MB_ICONERROR);return;}
      int selected=dssi::choose(window,bank);
      if(selected<0)return;
      auto& entry=bank.entries[size_t(selected)];
      applyPatch(entry.state);
      presets.loaded(entry.state,entry.patch.name);
      importedName=dssi::wide(entry.patch.name);
      for(auto& c:importedName)if(c<32||wcschr(L"<>:\"/\\|?*",c))c=L'_';
      // Bank text and diagnostics die here; playback receives only normal State.
    } catch(const std::exception&) {
      MessageBoxW(window,L"Could not import this bank.",XSYNTH_WIDE(XSYNTH_PRODUCT_NAME),MB_ICONERROR);
    }
  }
  void loadMenu() {
    PopupScope modal(*this);
    HMENU menu=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING,1,L"Load Patch...");
    AppendMenuW(menu,MF_STRING,2,L"Import Xsynth-DSSI Bank...");
    POINT point{z(ui::LoadPatch.x),z(ui::LoadPatch.y+ui::LoadPatch.h)};ClientToScreen(window,&point);
    UINT choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,window,nullptr);
    DestroyMenu(menu);
    if(choice==1)patch(false);else if(choice==2)importDssi();
  }
  void patch(bool save) {
    PopupScope modal(*this);
    wchar_t path[MAX_PATH]{};
    if(save&&!importedName.empty())wcsncpy_s(path,importedName.c_str(),_TRUNCATE);
    OPENFILENAMEW f{sizeof f};
    f.hwndOwner = window;
    f.lpstrFilter = L"Xsynth 2000 presets\0*.XseDev\0Original Xsynth patches\0*.Xpatch\0\0";
    f.lpstrFile = path;
    f.nMaxFile = MAX_PATH;
    f.lpstrDefExt = L"XseDev";
    f.Flags =
        OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (!(save ? GetSaveFileNameW(&f) : GetOpenFileNameW(&f)))
      return;
    if (save) {
      const wchar_t *extension=wcsrchr(path,L'.');
      auto b = extension && _wcsicmp(extension,L".Xpatch")==0 ? exportPatch(owner->read()) : encode(owner->read());
      std::ofstream out(path, std::ios::binary);
      out.write(reinterpret_cast<const char *>(b.data()), b.size());
      out.close();
      if (!out)
        MessageBoxW(window, L"Could not save patch.", XSYNTH_WIDE(XSYNTH_PRODUCT_NAME), MB_ICONERROR);
      else {presets.loaded(owner->read(),std::filesystem::path(path).stem().u8string());importedName=std::filesystem::path(path).stem().wstring();InvalidateRect(window,nullptr,FALSE);}
    } else {
      std::ifstream in(path, std::ios::binary | std::ios::ate);
      if (!in || (in.tellg() != 209 && in.tellg() != StateBytes && in.tellg() != 8+LegacyCount*8)) {
        MessageBoxW(window, L"Expected an original Xsynth patch or an Xsynth 2000 preset.", XSYNTH_WIDE(XSYNTH_PRODUCT_NAME), MB_ICONERROR);
        return;
      }
      auto length=size_t(in.tellg());in.seekg(0);
      std::vector<uint8_t> b(length);
      in.read(reinterpret_cast<char *>(b.data()), length);
      State s;
      if (!(length==209?importPatch(b,s):decode(b.data(),b.size(),s))) {
        MessageBoxW(window, L"Invalid Xsynth patch data.", XSYNTH_WIDE(XSYNTH_PRODUCT_NAME), MB_ICONERROR);
        return;
      }
      applyPatch(s);
      presets.loaded(s,std::filesystem::path(path).stem().u8string());
      importedName=std::filesystem::path(path).stem().wstring();
    }
  }
  int hit(int x,int y) { auto c=controlAt(x,y);return c?effectiveId(*c,owner->read()):-100; }
  static LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    auto *self = reinterpret_cast<Impl *>(GetWindowLongPtrW(w, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
      self = static_cast<Impl *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
      SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
      self->window = w;
    }
    if (!self)
      return DefWindowProcW(w, msg, wp, lp);
    // A synchronous native popup owns all pointer events, not only right-click.
    if(self->modalActive&&(msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP||msg==WM_LBUTTONDBLCLK||
       msg==WM_RBUTTONDOWN||msg==WM_RBUTTONUP||msg==WM_RBUTTONDBLCLK||
       msg==WM_MOUSEMOVE||msg==WM_MOUSEWHEEL))return 0;
    if(self->swallowLeftRelease&&(msg==WM_LBUTTONDOWN||msg==WM_LBUTTONDBLCLK||msg==WM_LBUTTONUP)){
      if(msg==WM_LBUTTONUP){self->swallowLeftRelease=false;self->finishPointer();}return 0;
    }
    switch (msg) {
    case WM_DPICHANGED:
    case WM_DPICHANGED_AFTERPARENT:
      if (self->owner->dpiChanged)
        self->owner->dpiChanged();
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_PRINTCLIENT:
      self->render(self->owner->read());
      BitBlt(reinterpret_cast<HDC>(wp), 0, 0, self->width, self->height,
             self->dc, 0, 0, SRCCOPY);
      return 0;
    case WM_SETCURSOR:
      if (LOWORD(lp) == HTCLIENT) {
        POINT p{};
        GetCursorPos(&p);
        ScreenToClient(w, &p);
        bool clickable = contains(self->logoBounds(), p) ||
            self->aboutOpen;
        if(!self->aboutOpen){int x=int(p.x/self->scale),y=int(p.y/self->scale);for(auto b:{ui::PresetPrevious,ui::PresetField,ui::PresetNext})if(x>=b.x&&x<b.x+b.w&&y>=b.y&&y<b.y+b.h)clickable=true;}
        SetCursor(LoadCursor(nullptr, clickable ? IDC_HAND : IDC_ARROW));
        return TRUE;
      }
      break;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      auto target = BeginPaint(w, &ps);
      self->render(self->owner->read());
      BitBlt(target, 0, 0, self->width, self->height, self->dc, 0, 0, SRCCOPY);
      EndPaint(w, &ps);
      return 0;
    }
    case WM_TIMER:
      if (self->last != self->owner->read())
        InvalidateRect(w, nullptr, FALSE);
      return 0;
    case WM_LBUTTONDOWN: {
      POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      if (contains(self->logoBounds(), point) && self->drag < 0) {
        self->finishPointer();
        self->aboutOpen = !self->aboutOpen;
        self->highlightedRoutes=0;
        InvalidateRect(w, nullptr, FALSE);
        return 0;
      }
      if (self->aboutOpen) {
        if(contains(self->themeBounds(),point)) {
          self->pressedAction=-3;SetCapture(w);
          self->changeTheme(theme::Id((unsigned(self->themeId)+1)%theme::Count),true);
          InvalidateRect(w,nullptr,FALSE);
          return 0;
        }
        if (contains(self->aboutLinkBounds(AboutUrl,ui::CreditY[2]), point)) {
          // Fixed HTTPS destination only; never interpret patch or host-supplied input.
          ShellExecuteW(w, L"open", AboutUrl, nullptr, nullptr, SW_SHOWNORMAL);
        } else if (contains(self->aboutLinkBounds(AboutEmail,ui::AboutEmailY),point)) {
          ShellExecuteW(w,L"open",AboutMailto,nullptr,nullptr,SW_SHOWNORMAL);
        } else {
          self->aboutOpen = false;
          InvalidateRect(w, nullptr, FALSE);
        }
        return 0; // No click-through to knobs, switches or patch actions.
      }
      int x = int(GET_X_LPARAM(lp) / self->scale), y = int(GET_Y_LPARAM(lp) / self->scale) - 48;
      auto inside=[&](Bounds b){return x>=b.x&&x<b.x+b.w&&y+48>=b.y&&y+48<b.y+b.h;};
      if(inside(ui::PresetPrevious)){self->factoryPreset(self->presets.adjacent(-1));return 0;}
      if(inside(ui::PresetNext)){self->factoryPreset(self->presets.adjacent(1));return 0;}
      if(inside(ui::PresetField)){self->presetMenu();return 0;}
      const auto *control=self->controlAt(x,y);
      if(control && (control->kind==ControlKind::Selector || control->kind==ControlKind::Toggle)) {
        if(!isDropdown(*control,self->owner->read())){self->pressedParam=control->id;SetCapture(w);}
        self->select(*control);InvalidateRect(w,nullptr,FALSE);return 0;
      }
      auto id = self->hit(x, y);
      if (id >= 0) {
        self->drag = id;
        self->startY = GET_Y_LPARAM(lp);
        self->startX = GET_X_LPARAM(lp);
        self->draggingAmount=control && control->kind==ControlKind::Amount;
        self->startValue = self->owner->read()[id];
        self->owner->begin(id);
        SetCapture(w);
        return 0;
      }
      for (auto &b : buttons)
        if (x >= b.x && x <= b.x + 104 && y >= b.y && y <= b.y + 24) {
          if (b.id < 0) {
            self->pressedAction = b.id;
            InvalidateRect(w, nullptr, FALSE);
            UpdateWindow(w);
            if(b.id==-2)self->patch(true);else self->loadMenu();
            self->pressedAction = 0;
            InvalidateRect(w, nullptr, FALSE);
          } else {
            self->owner->begin(b.id);
            self->edit(b.id, self->owner->read()[b.id] >= .5 ? 0 : 1);
            self->owner->end(b.id);
          }
          break;
        }
      return 0;
    }
    case WM_RBUTTONUP:
      if(!self->aboutOpen && !self->modalActive && self->drag<0 && IsWindowEnabled(w))
        self->sectionMenu(GET_X_LPARAM(lp),GET_Y_LPARAM(lp));
      return 0;
    case WM_MOUSELEAVE:
      if(self->highlightedRoutes){self->highlightedRoutes=0;InvalidateRect(w,nullptr,FALSE);}
      return 0;
    case WM_MOUSEMOVE:
      if(self->drag<0 && !self->aboutOpen)self->hoverRoutes(POINT{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)});
      if (self->aboutOpen)
        return 0;
      if (self->drag >= 0) {
        double value = self->startValue +
            (self->draggingAmount ? GET_X_LPARAM(lp)-self->startX : self->startY-GET_Y_LPARAM(lp)) /
            ((self->draggingAmount ? 200 : 150)*self->scale*((wp & MK_SHIFT) ? 10 : 1));
        // Manual coarse Pitch edits only: 48 semitones span normalized [0,1].
        // Imported/state/automation values and Shift fine edits remain continuous.
        if ((self->drag == 0 || self->drag == 2) && !(wp & MK_SHIFT))
          value = std::round(value * 48.0) / 48.0;
        self->edit(self->drag, value);
      }
      return 0;
    case WM_LBUTTONUP:
      if(self->pressedParam>=0){self->pressedParam=-1;if(GetCapture()==w)ReleaseCapture();InvalidateRect(w,nullptr,FALSE);}
      if(self->pressedAction==-3){self->pressedAction=0;ReleaseCapture();InvalidateRect(w,nullptr,FALSE);}
      if (self->drag >= 0) {
        self->owner->end(self->drag);
        self->drag = -100;
        ReleaseCapture();
      }
      return 0;
    case WM_CAPTURECHANGED:
      self->pressedParam=-1;self->swallowLeftRelease=false;
      if(self->pressedAction==-3){self->pressedAction=0;InvalidateRect(w,nullptr,FALSE);}
      if (self->drag >= 0) {
        self->owner->end(self->drag);
        self->drag = -100;
      }
      return 0;
    case WM_CANCELMODE:
    case WM_KILLFOCUS:
      self->swallowLeftRelease=false;self->finishPointer();
      break;
    case WM_MOUSEWHEEL: {
      if (self->aboutOpen)
        return 0;
      POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      ScreenToClient(w, &pt);
      int id = self->hit(int(pt.x / self->scale), int(pt.y / self->scale) - 48);
      if (id >= 0) {
        self->owner->begin(id);
        self->edit(id, self->owner->read()[id] +
                           (GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1) * (stepsFor(id)?1./stepsFor(id):.01));
        self->owner->end(id);
      }
      return 0;
    }
    case WM_LBUTTONDBLCLK: {
      // CS_DBLCLKS replaces the second DOWN of two rapid physical clicks.
      // Discrete actions still activate once per click; continuous controls
      // retain their existing double-click-to-default gesture.
      int x=int(GET_X_LPARAM(lp)/self->scale),y=int(GET_Y_LPARAM(lp)/self->scale)-48;
      const auto* control=self->controlAt(x,y);
      if(self->aboutOpen||y<0||(control&&(control->kind==ControlKind::Selector||control->kind==ControlKind::Toggle)))
        return proc(w,WM_LBUTTONDOWN,wp,lp);
      int id =
          self->hit(int(GET_X_LPARAM(lp) / self->scale), int(GET_Y_LPARAM(lp) / self->scale) - 48);
      if (id >= 0) {
        self->owner->begin(id);
        self->edit(id, defaults()[id]);
        self->owner->end(id);
        if(id==26) {
          self->owner->begin(LfoRandom);self->edit(LfoRandom,0);self->owner->end(LfoRandom);
        }
      }
      return 0;
    }
    case WM_DESTROY:
      KillTimer(w, 1);
      self->window = nullptr;
      return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
  }
};
Editor::Editor(const wchar_t *key) : impl(std::make_unique<Impl>(this,key)) {}
void Editor::setTheme(theme::Id id){impl->themeOverride=true;impl->changeTheme(id,false);}
theme::Id Editor::currentTheme() const {return impl->themeId;}
Editor::~Editor() { close(); }
HWND Editor::open(HWND parent, int w, int h) {
  if(!impl->themeOverride && !impl->preferenceKey.empty())impl->changeTheme(theme::load(impl->preferenceKey.c_str()),false);
  WNDCLASSW cls{};
  cls.hInstance = module();
  cls.lpfnWndProc = Impl::proc;
  cls.lpszClassName = L"XsynthExpansionDevEditor";
  cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
  cls.style = CS_DBLCLKS;
  RegisterClassW(&cls);
  impl->width = w;
  impl->height = h;
  impl->prepare();
  auto hwnd = CreateWindowExW(0, cls.lpszClassName, XSYNTH_WIDE(XSYNTH_PRODUCT_NAME), WS_CHILD | WS_VISIBLE, 0, 0, w, h,
                              parent, nullptr, module(), impl.get());
  if (hwnd)
    SetTimer(hwnd, 1, 33, nullptr);
  return hwnd;
}
void Editor::close() {
  impl->aboutOpen = false;
  impl->highlightedRoutes=0;
  if (impl->window) {
    if (impl->drag >= 0) {
      end(impl->drag);
      impl->drag = -100;
    }
    DestroyWindow(impl->window);
  }
  // A DLL's window classes must not outlive its window procedures. If another
  // view is open, this fails harmlessly; the last view unregisters the class.
  UnregisterClassW(L"XsynthExpansionDevEditor", module());
  impl->release();
  impl->background.clear();impl->background.shrink_to_fit();
}
void Editor::resize(int w, int h) {
  if (w <= 0 || h <= 0 || (impl->dc && w==impl->width && h==impl->height))
    return;
  impl->width = w;
  impl->height = h;
  impl->prepare();
  if (impl->window) {
    SetWindowPos(impl->window, nullptr, 0, 0, w, h, SWP_NOZORDER | SWP_NOMOVE);
    InvalidateRect(impl->window, nullptr, FALSE);
  }
}
bool Editor::snapshot(const wchar_t *path, int percent, bool dithering, bool about) {
  if(impl->dithering!=dithering){impl->dithering=dithering;impl->background.clear();}
  resize(NativeWidth * percent / 100, NativeHeight * percent / 100);
  const bool previousAbout = impl->aboutOpen;
  impl->aboutOpen = about;
  impl->render(read());
  impl->aboutOpen = previousAbout;
  Gdiplus::Bitmap b(impl->image, nullptr);
  UINT size = 0, count = 0;
  Gdiplus::GetImageEncodersSize(&count, &size);
  std::vector<uint8_t> bytes(size);
  auto codecs = reinterpret_cast<Gdiplus::ImageCodecInfo *>(bytes.data());
  Gdiplus::GetImageEncoders(count, size, codecs);
  for (UINT i = 0; i < count; ++i)
    if (std::wcscmp(codecs[i].MimeType, L"image/png") == 0)
      return b.Save(path, &codecs[i].Clsid, nullptr) == Gdiplus::Ok;
  return false;
}
} // namespace xs

