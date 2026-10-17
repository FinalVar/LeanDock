#pragma once
#include <windows.h>
namespace LeanDockStartProtocol {
inline constexpr wchar_t Active[]=L"LeanDock.Start.Active";
inline constexpr wchar_t Owner[]=L"LeanDock.Start.Owner";
inline constexpr wchar_t Adjustments[]=L"LeanDock.Start.Adjustments";
inline constexpr wchar_t AttemptOwner[]=L"LeanDock.Start.AttemptOwner";
inline constexpr wchar_t InitResult[]=L"LeanDock.Start.InitResult";
inline UINT Signal(){static const UINT value=RegisterWindowMessageW(L"LeanDock.StartLayout.v1");return value;}
}
