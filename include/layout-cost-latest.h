#pragma once
#include <windows.h>
#include <atomic>

// Always-on totals for LeanDock work done inside Explorer layout passes.
// SaveState publishes them in latest-backend.json; they never alter behaviour.
namespace LeanDockLayoutCost {
enum Kind : unsigned { Tray, Surface, Count };
struct Totals { std::atomic<unsigned long long> calls{0},ticks{0},worst{0}; };
inline Totals totals[Count];
inline unsigned long long Micros(unsigned long long ticks){
    LARGE_INTEGER f{};QueryPerformanceFrequency(&f);
    return f.QuadPart>0?ticks*1000000ull/static_cast<unsigned long long>(f.QuadPart):0;
}
struct Scope {
    Kind kind;LARGE_INTEGER start{};
    explicit Scope(Kind k):kind(k){QueryPerformanceCounter(&start);}
    ~Scope(){
        LARGE_INTEGER end{};QueryPerformanceCounter(&end);
        const auto ticks=static_cast<unsigned long long>(end.QuadPart-start.QuadPart);
        auto& t=totals[kind];
        t.calls.fetch_add(1,std::memory_order_relaxed);t.ticks.fetch_add(ticks,std::memory_order_relaxed);
        auto worst=t.worst.load(std::memory_order_relaxed);
        while(ticks>worst&&!t.worst.compare_exchange_weak(worst,ticks,std::memory_order_relaxed)){}
    }
    Scope(Scope const&)=delete;Scope& operator=(Scope const&)=delete;
};
}
