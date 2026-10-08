#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace LeanDockCards {
inline constexpr double CardWidth=160, CardHeight=44, SlotWidth=184;
struct CpuTimes { uint64_t idle, kernel, user; };
// GetSystemTimes reports idle time as part of kernel time. Reject counter
// resets and zero-length intervals instead of presenting an invented sample.
inline std::optional<unsigned> CpuPercent(CpuTimes a, CpuTimes b) {
    if(b.idle<a.idle || b.kernel<a.kernel || b.user<a.user)return {};
    const auto idle=b.idle-a.idle, kernel=b.kernel-a.kernel, user=b.user-a.user;
    const double total=double(kernel)+double(user);
    if(total<=0 || double(idle)>total)return {};
    return unsigned(std::clamp(std::lround(100.0*(total-double(idle))/total),0l,100l));
}
inline bool Fits(double available,double apps,double tray,bool alreadyShown) {
    if(!std::isfinite(available)||!std::isfinite(apps)||!std::isfinite(tray) ||
       available<=0||apps<=0||tray<=0)return false;
    // Space for the original root padding/border and outer 12-DIP margins.
    // DesiredSize can be capped by the parent's measure constraint. Collapse
    // before that cap rather than mistaking clipped app content for a fit.
    // Four additional DIP on entry prevent fractional-DPI oscillation.
    return available>=apps+tray+25+SlotWidth+24+(alreadyShown?4:8);
}
inline bool ShouldSample(bool shown,bool metrics,bool sessionActive) {
    return shown && metrics && sessionActive;
}
inline bool AdvanceWheel(int delta,int& remainder) {
    remainder+=std::clamp(delta,-120,120);
    if(std::abs(remainder)<120)return false;
    remainder=0;return true;
}
}
