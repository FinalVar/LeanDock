#pragma once
struct NativeIconSourceCall { const char* kind; unsigned returnRva; };
inline constexpr NativeIconSourceCall NativeIconSourceCalls[] = {
    {"Cache", 481183u},
    {"Win32", 350113u},
    {"Packaged", 319731u},
    {"Placeholder", 1503675u},
    {"Website", 1506768u},
};
