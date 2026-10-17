#pragma once
#include <windows.h>

namespace LeanDockClipOwnership {
// Preserve the upstream ownership rule except for an observed vertical shift
// of our last full-height rectangle. A tablet fold reduces the height; it is
// not equivalent to this same-sized, partially off-window region. Never claim
// a different horizontal extent, complex region or region from an older size.
constexpr bool ShiftedFullHeight(int regionKind,const RECT& current,
                                 const RECT& applied,int width,int height) noexcept {
    return regionKind==SIMPLEREGION && width>0 && height>0 &&
        applied.left>=0 && applied.right>applied.left && applied.right<=width &&
        applied.top==0 && applied.bottom==height &&
        current.left==applied.left && current.right==applied.right &&
        current.top<0 && current.bottom>0 &&
        static_cast<long long>(current.bottom)-current.top==height;
}
}
