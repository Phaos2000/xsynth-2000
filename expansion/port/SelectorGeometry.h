// GPL-2.0-or-later. Shared collapsed selector geometry; no input behavior.
#pragma once
#include <windows.h>
#include <cmath>
#include <algorithm>
namespace xs::selector {
inline constexpr int ArrowGutterWidth=12,ArrowWidth=5,ArrowHeight=3;
inline constexpr int TextLeftPadding=3,TextRightPadding=3,TextVerticalPadding=2,SeparatorInset=3;
inline int px(int n,double scale){return int(std::round(n*scale));}
inline RECT textRect(RECT r,double scale){r.left+=px(TextLeftPadding,scale);r.right-=px(ArrowGutterWidth+TextRightPadding,scale);r.top+=px(TextVerticalPadding,scale);r.bottom-=px(TextVerticalPadding,scale);return r;}
inline void drawArrow(HDC dc,RECT r,double scale,COLORREF ink,COLORREF separator){
 int gutter=r.right-px(ArrowGutterWidth,scale),cx=(gutter+r.right)/2,cy=(r.top+r.bottom)/2;
 HPEN pen=CreatePen(PS_SOLID,std::max(1,px(1,scale)),separator);auto oldPen=SelectObject(dc,pen);MoveToEx(dc,gutter,r.top+px(SeparatorInset,scale),nullptr);LineTo(dc,gutter,r.bottom-px(SeparatorInset,scale));SelectObject(dc,oldPen);DeleteObject(pen);
 POINT triangle[]={{cx-px(ArrowWidth/2,scale),cy-px(1,scale)},{cx+px(ArrowWidth/2,scale)+1,cy-px(1,scale)},{cx,cy+px(ArrowHeight-1,scale)}};HBRUSH brush=CreateSolidBrush(ink);auto oldBrush=SelectObject(dc,brush);oldPen=SelectObject(dc,GetStockObject(NULL_PEN));Polygon(dc,triangle,3);SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(brush);
}
}
