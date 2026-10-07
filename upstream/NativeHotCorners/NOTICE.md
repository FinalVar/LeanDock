# Implementation references and runtime notices

Independently written Win32 implementation. No HotCornersWin executable, library or source code is included. Behavioral references:

- https://github.com/flexits/HotCornersWin (MIT; inspected 2026-10-07)
- https://github.com/flexits/HotCornersWin/blob/main/src/HotCornersWin/HotCornersProcessor.cs
- https://github.com/flexits/HotCornersWin/blob/main/src/HotCornersWin/KeyboardSend.cs (extended Windows-key flag reference; no source copied)
- https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput

The native executable is built with Zig 0.13.0 using its MinGW toolchain and the Windows system C runtime. The C++ standard library is not linked. Applicable MinGW runtime notices are retained in licenses/mingw/latest.txt. No separate desktop runtime is distributed or installed.

This local project does not assign a new public open-source license to its own code.
