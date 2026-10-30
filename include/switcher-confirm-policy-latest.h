#pragma once
#include <cstdint>
#include <mutex>

namespace NativeSwitcherPolicy {
// A short, single-use request owned by one actual corner activation. No timer:
// reading or consuming an expired ticket clears it. Never renew on UI events.
class Request {
    std::mutex mutex;
    std::uint64_t sequence{}, pending{}, deadline{};
public:
    std::uint64_t Arm(std::uint64_t now) {
        std::lock_guard guard(mutex);
        if (++sequence == 0) ++sequence;
        pending = sequence; deadline = now + 1500; return pending;
    }
    void Cancel() { std::lock_guard guard(mutex); pending = 0; }
    std::uint64_t Peek(std::uint64_t now) {
        std::lock_guard guard(mutex);
        if (now > deadline) pending = 0;
        return pending;
    }
    bool Consume(std::uint64_t ticket, std::uint64_t now) {
        std::lock_guard guard(mutex);
        if (now > deadline) pending = 0;
        if (!ticket || pending != ticket) return false;
        pending = 0; return true;
    }
};
enum class Decision { Wait, KeepOpen, Confirm };
inline Decision Decide(bool activeHost, unsigned count, int selected, bool invokeReady) {
    if (!activeHost || count == 0) return Decision::Wait;
    if (count != 2) return Decision::KeepOpen;
    if (selected < 0 || static_cast<unsigned>(selected) >= count) return Decision::Wait;
    return invokeReady ? Decision::Confirm : Decision::Wait;
}
}
