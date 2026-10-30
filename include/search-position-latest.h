#pragma once
#include "start-position-latest.h"
#include "start-menu-state-latest.h"

namespace LeanDockSearch {
inline constexpr UINT_PTR SubclassId=0x4c445153;
inline constexpr wchar_t Active[]=L"LeanDock.Search.Active";
inline constexpr wchar_t Owner[]=L"LeanDock.Search.Owner";
inline constexpr wchar_t Adjustments[]=L"LeanDock.Search.Adjustments";
inline constexpr wchar_t HookPid[]=L"LeanDock.Search.HookPid";
inline constexpr wchar_t HookSignal[]=L"LeanDock.Search.HookSignal";
inline constexpr wchar_t HookMessage[]=L"LeanDock.Search.HookMessage";
inline constexpr wchar_t InitStage[]=L"LeanDock.Search.InitStage";
inline constexpr wchar_t InitError[]=L"LeanDock.Search.InitError";
inline UINT Signal(){static const UINT value=RegisterWindowMessageW(L"LeanDock.SearchPosition.v1");return value;}
enum Command:WPARAM {Enable=1,Disable,Query,OwnerGone};
inline bool Host(HWND w) {return LeanDockStartState::Identify(w)==LeanDockStartState::Kind::Search;}
inline bool Adjust(WINDOWPOS& pos,const RECT& current,const RECT& dock,
                   const RECT& monitor,UINT dpi,RECT* requested=nullptr) {
    return LeanDockStartPlacement::Adjust(pos,current,dock,monitor,dpi,requested);
}
}
