#pragma once
#include "../../include/client-edge-policy-latest.h"

namespace nhc {
// Preserve the existing conservative fullscreen rule for other applications.
// The verified GPT client's normal, resizable maximization has no WS_CAPTION;
// its DWM frame can cover the monitor when the auto-hide edge changes.
inline bool SuppressFullscreen(LONG_PTR style,const RECT& frame,const RECT& monitor,
                               bool maximized,bool verifiedClient) {
    if((style&WS_CAPTION)==WS_CAPTION || monitor.right<=monitor.left ||
       monitor.bottom<=monitor.top)return false;
    const bool covers=frame.left<=monitor.left && frame.top<=monitor.top &&
                      frame.right>=monitor.right && frame.bottom>=monitor.bottom;
    return covers && !(verifiedClient &&
        LeanDockClientEdgePolicy::Eligible(true,false,maximized,style,frame,monitor));
}
}
