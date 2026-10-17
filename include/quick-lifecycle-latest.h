#pragma once
#include <cstdint>

namespace LeanDockQuick {
// The shell can retire an idle host with exit code zero. Recover only that
// case, and retain a bounded restart budget even for nominally clean exits.
struct RestartPolicy {
    static constexpr std::uint64_t WindowMs=60000;
    static constexpr unsigned Limit=2;
    std::uint64_t windowStart{};
    unsigned recent{},recoveries{};
    bool blocked{};
    constexpr bool Retired(bool exitKnown,std::uint32_t exitCode,std::uint64_t now) {
        if(blocked)return false;
        if(!exitKnown||exitCode!=0){blocked=true;return false;}
        if(!recent||now-windowStart>=WindowMs){windowStart=now;recent=0;}
        if(recent>=Limit){blocked=true;return false;}
        ++recent;++recoveries;return true;
    }
};
}
