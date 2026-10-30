#pragma once
#include <cstdint>
#include "icon-resolution-pin-generated.h"

// Separate the source-pixel budget from the native UI size. The five call
// sites are decoded and checked against the pinned Taskbar module at build
// time; every other caller, including layout, receives the original value.
namespace LeanDockIconResolution {
inline int SourceKind(std::uintptr_t returnRva) noexcept {
    for (unsigned i=0;i<sizeof(NativeIconSourceCalls)/sizeof(*NativeIconSourceCalls);++i)
        if (returnRva==NativeIconSourceCalls[i].returnRva) return static_cast<int>(i);
    return -1;
}
inline long SourceDips(long width,long height,bool stopping,int kind) noexcept {
    if(stopping||kind<0||width!=height||width<8||width>128)return width;
    // 2x leaves headroom above the 1.35x maximum optical compensation.
    // Windows retains resource selection, DPI, theme, caching and updates.
    return width*2;
}
}
