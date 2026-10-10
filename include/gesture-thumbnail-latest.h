#pragma once
namespace LeanDockGesture {
// The scope cannot leak into another thread or survive a native exception.
struct Scope {
    unsigned& depth;
    explicit Scope(unsigned& value) noexcept:depth(value){++depth;}
    ~Scope(){--depth;}
    Scope(Scope const&)=delete;
    Scope& operator=(Scope const&)=delete;
};
}
