#pragma once
#include <cstdint>

namespace LeanDockStart {
// A completed, rolled-back initialization is different from a host crash or
// failed image admission. Retry the former only, without a resident poll.
struct AttachPolicy {
    static constexpr unsigned Limit=3;
    static constexpr std::uint64_t DelayMs=750,WindowMs=60000;
    unsigned attempts{},recoveries{};
    std::uint64_t firstAttempt{},lastAttempt{};
    bool stopped{};
    constexpr bool Begin(std::uint64_t now,bool nativeEvent) {
        if(stopped)return false;
        // A new shell event can reopen an exhausted readiness budget after a
        // minute. A timer never renews the budget by itself.
        if(attempts && nativeEvent && now-firstAttempt>=WindowMs)attempts=0;
        if(attempts>=Limit || (attempts && now-lastAttempt<DelayMs))return false;
        if(!attempts)firstAttempt=now;
        ++attempts;lastAttempt=now;return true;
    }
    constexpr bool Retryable()const{return !stopped && attempts<Limit;}
    constexpr void Success(){if(attempts>1)++recoveries;attempts=0;stopped=false;}
    constexpr void Stop(){stopped=true;}
};
}
