#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
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
// Explorer's large image list can hold only the native-size picture for an
// icon, placed in a 256 px cell (MinerU: 64 px of artwork in 256). The native
// list then has the same picture at its real size. Only a large canvas counts,
// and an empty or uniform one says nothing.
inline bool UndersizedArtwork(std::span<const std::uint8_t> bgra,int width,int height) noexcept {
    if(width<128||height<128||width>1024||height>1024||bgra.size()<std::size_t(width)*height*4)return false;
    unsigned peak=0;
    for(std::size_t i=3;i<std::size_t(width)*height*4;i+=4)peak=std::max<unsigned>(peak,bgra[i]);
    if(!peak)return false;
    const unsigned threshold=std::max(16u,peak/2);
    int left=width,top=height,right=0,bottom=0;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(bgra[(std::size_t(y)*width+x)*4+3]>=threshold){
        left=std::min(left,x);top=std::min(top,y);right=std::max(right,x+1);bottom=std::max(bottom,y+1);
    }
    return right>left&&std::max(right-left,bottom-top)*5<std::max(width,height)*2;
}
}
