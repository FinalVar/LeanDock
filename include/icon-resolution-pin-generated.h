#pragma once
struct NativeIconSourceCall { const char* kind; unsigned returnRva; };
inline constexpr NativeIconSourceCall NativeIconSourceCalls[] = {
    {"Cache", 166092u},
    {"Win32", 330957u},
    {"Packaged", 245369u},
    {"Placeholder", 1426326u},
    {"Website", 1429984u},
};
