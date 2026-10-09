#pragma once
#include "panel-position-latest.h"

namespace LeanDockQuick {
inline constexpr UINT_PTR SubclassId=0x4c445151;
inline constexpr wchar_t Active[]=L"LeanDock.Quick.Active";
inline constexpr wchar_t Owner[]=L"LeanDock.Quick.Owner";
inline constexpr wchar_t Adjustments[]=L"LeanDock.Quick.Adjustments";
inline UINT Signal(){static const UINT value=RegisterWindowMessageW(L"LeanDock.QuickPosition.v1");return value;}
enum Command:WPARAM { Enable=1,Disable,Query,OwnerGone };

// Change only position, before Windows commits it. Preserve native dimensions,
// visibility, activation and z-order; a hidden window keeps its Dock placement.
inline bool Adjust(WINDOWPOS& pos,const RECT& current,const RECT& dock,
                   const RECT& monitor,UINT dpi,RECT* requested=nullptr) {
    return LeanDockPlacement::Adjust(pos,current,dock,monitor,dpi,false,requested);
}
}
