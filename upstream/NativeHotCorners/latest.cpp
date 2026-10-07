#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <wtsapi32.h>
#include <stdio.h>
#include <wchar.h>
#include "core/latest.h"
#include "actions/latest.h"

static const wchar_t* kClass = L"NativeHotCorners.1.Window";
static const wchar_t* kMutex = L"Local\\NativeHotCorners.1.Instance";
enum { kEvaluate = WM_APP + 1, kControl = WM_APP + 2, kDwell = 1 };
enum { Status = 1, Pause, Resume, Reload, Quit };
static HWND window;
static nhc::Config config;
static nhc::Engine engine;
static nhc::Rect monitors[64];
static HMONITOR monitorIds[64];
static int monitorCount;
static bool paused, locked, evalPending, rawRegistered, timerArmed;
static uint64_t timerDue, started, rawEvents, mouseEvaluations, activations[4], failures, blockedEntries;
static wchar_t folder[MAX_PATH], iniPath[MAX_PATH], logPath[MAX_PATH], statusPath[MAX_PATH];
static DWORD lastErrorCode;

static void MakePath(wchar_t* result, const wchar_t* name) { swprintf(result, MAX_PATH, L"%s\\%s", folder, name); }
static void Log(const char* event, DWORD code = 0) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(logPath, GetFileExInfoStandard, &data) && data.nFileSizeLow > 131072) {
        wchar_t previous[MAX_PATH]; MakePath(previous, L"previous.log");
        MoveFileExW(logPath, previous, MOVEFILE_REPLACE_EXISTING);
    }
    HANDLE file = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME t; GetLocalTime(&t);
    char line[256];
    int n = snprintf(line, sizeof(line), "%04u-%02u-%02uT%02u:%02u:%02u event=%s code=%lu\r\n",
                     t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, event, (unsigned long)code);
    DWORD written; WriteFile(file, line, n, &written, nullptr); CloseHandle(file);
}
static const char* ActionName(nhc::Action action) {
    return action == nhc::TaskView ? "task-view" : action == nhc::ShowDesktop ? "show-desktop" : action == nhc::StartMenu ? "start-menu" : "none";
}
static bool ReadNumber(const wchar_t* key, int def, int min, int max, int& out) {
    wchar_t fallback[24], value[80], *end;
    swprintf(fallback, 24, L"%d", def);
    GetPrivateProfileStringW(L"HotCorners", key, fallback, value, 80, iniPath);
    long parsed = wcstol(value, &end, 10);
    if (end == value || *end || parsed < min || parsed > max) return false;
    out = (int)parsed; return true;
}
static bool LoadConfig() {
    nhc::Config next;
    int delay, cooldown, pixels, rearm, fullscreen;
    if (!ReadNumber(L"DelayMs", 0, 0, 1500, delay) ||
        !ReadNumber(L"CooldownMs", 0, 0, 3000, cooldown) ||
        !ReadNumber(L"CornerPixels", 3, 1, 12, pixels) ||
        !ReadNumber(L"RearmPixels", 32, 8, 128, rearm) ||
        !ReadNumber(L"PauseOnFullscreen", 1, 0, 1, fullscreen) || rearm <= pixels) return false;
    wchar_t value[80];
    GetPrivateProfileStringW(L"HotCorners", L"Monitors", L"all", value, 80, iniPath);
    if (_wcsicmp(value, L"primary") && _wcsicmp(value, L"all")) return false;
    next.primaryOnly = !_wcsicmp(value, L"primary");
    const wchar_t* keys[] = { L"TopLeft", L"TopRight", L"BottomLeft", L"BottomRight" };
    const wchar_t* defaults[] = { L"none", L"none", L"start-menu", L"task-view" };
    for (int c = 0; c < 4; ++c) {
        GetPrivateProfileStringW(L"HotCorners", keys[c], defaults[c], value, 80, iniPath);
        if (!_wcsicmp(value, L"task-view")) next.actions[c] = nhc::TaskView;
        else if (!_wcsicmp(value, L"show-desktop")) next.actions[c] = nhc::ShowDesktop;
        else if (!_wcsicmp(value, L"start-menu")) next.actions[c] = nhc::StartMenu;
        else if (!_wcsicmp(value, L"none")) next.actions[c] = nhc::None;
        else return false;
    }
    next.delay = delay; next.cooldown = cooldown; next.pixels = pixels; next.rearm = rearm; next.fullscreen = fullscreen != 0;
    config = next; return true;
}
static BOOL CALLBACK EnumMonitor(HMONITOR h, HDC, RECT*, LPARAM) {
    MONITORINFO info = { sizeof(info) };
    if (!GetMonitorInfoW(h, &info)) return TRUE;
    if (config.primaryOnly && !(info.dwFlags & MONITORINFOF_PRIMARY)) return TRUE;
    if (monitorCount >= 64) return FALSE;
    monitors[monitorCount] = { info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right, info.rcMonitor.bottom };
    monitorIds[monitorCount++] = h;
    return TRUE;
}
static void Reset() {
    KillTimer(window, kDwell); timerArmed = false; timerDue = 0; engine.Reset();
}
static void RefreshMonitors() {
    Reset(); monitorCount = 0; EnumDisplayMonitors(nullptr, nullptr, EnumMonitor, 0);
}
static bool DefaultInputDesktop() {
    HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!desktop) return false;
    wchar_t name[128]; DWORD needed;
    bool ok = GetUserObjectInformationW(desktop, UOI_NAME, name, sizeof(name), &needed) && !_wcsicmp(name, L"Default");
    CloseDesktop(desktop); return ok;
}
static bool ModifierOrButton() {
    const int keys[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2, VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN };
    for (int key : keys) if (GetAsyncKeyState(key) & 0x8000) return true;
    return false;
}
static bool Fullscreen(HMONITOR target) {
    if (!config.fullscreen) return false;
    HWND foreground = GetForegroundWindow();
    if (!foreground || !IsWindowVisible(foreground) || IsIconic(foreground) || foreground == GetShellWindow()) return false;
    wchar_t name[128]; GetClassNameW(foreground, name, 128);
    // Desktop/taskbar/Task View must remain usable; do not use notification
    // 'busy' state, which can remain busy after an ordinary fullscreen app exits.
    if (!wcscmp(name, L"Progman") || !wcscmp(name, L"WorkerW") ||
        !wcscmp(name, L"Shell_TrayWnd") || !wcscmp(name, L"Shell_SecondaryTrayWnd") ||
        !wcscmp(name, L"MultitaskingViewFrame") || !wcscmp(name, L"XamlExplorerHostIslandWindow")) return false;
    if (MonitorFromWindow(foreground, MONITOR_DEFAULTTONULL) != target) return false;
    if ((GetWindowLongPtrW(foreground, GWL_STYLE) & WS_CAPTION) == WS_CAPTION) return false;
    MONITORINFO mi = { sizeof(mi) }; RECT rect;
    if (!GetMonitorInfoW(target, &mi)) return false;
    if (FAILED(DwmGetWindowAttribute(foreground, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect))) && !GetWindowRect(foreground, &rect)) return false;
    return rect.left <= mi.rcMonitor.left && rect.top <= mi.rcMonitor.top &&
           rect.right >= mi.rcMonitor.right && rect.bottom >= mi.rcMonitor.bottom;
}
static bool Blocked(nhc::Target target) {
    if (paused || locked || ModifierOrButton() || !DefaultInputDesktop()) return true;
    return target.monitor >= 0 && target.monitor < monitorCount && Fullscreen(monitorIds[target.monitor]);
}
static void SyncTimer() {
    if (engine.waiting.monitor < 0) {
        if (timerArmed) KillTimer(window, kDwell);
        timerArmed = false; timerDue = 0; return;
    }
    if (timerArmed && timerDue == engine.due) return;
    uint64_t now = GetTickCount64();
    UINT wait = engine.due > now ? (UINT)(engine.due - now) : 1;
    timerArmed = SetTimer(window, kDwell, wait, nullptr) != 0;
    timerDue = engine.due;
    if (!timerArmed) { lastErrorCode = GetLastError(); engine.Consume(engine.waiting); Log("timer-failed", lastErrorCode); }
}
static bool LaunchAction(nhc::Action action) {
    INPUT keys[4]; UINT count = nhc::BuildActionInput(action, keys);
    if (!count) return false;
    if (ModifierOrButton() || (count == 4 && (GetAsyncKeyState(keys[1].ki.wVk) & 0x8000))) {
        lastErrorCode = ERROR_BUSY; return false;
    }
    SetLastError(ERROR_SUCCESS);
    UINT sent = SendInput(count, keys, sizeof(INPUT));
    lastErrorCode = sent == count ? 0 : GetLastError();
    if (sent == count) return true;
    if (!lastErrorCode) lastErrorCode = ERROR_GEN_FAILURE;
    // Release only keys whose injected key-down was accepted but key-up was
    // not, so partial submission cannot leave our Windows/Tab key held down.
    INPUT releases[2]; UINT releaseCount = 0;
    if (count == 4 && sent == 2) releases[releaseCount++] = keys[2];
    if (sent > 0 && sent < count) releases[releaseCount++] = keys[count - 1];
    if (releaseCount) SendInput(releaseCount, releases, sizeof(INPUT));
    return false;
}
static void Evaluate(bool expiry) {
    ++mouseEvaluations;
    POINT point; if (!GetCursorPos(&point)) { Reset(); return; }
    nhc::Point p = { point.x, point.y };
    nhc::Target hit = nhc::Hit(monitors, monitorCount, p, config);
    // Stay inexpensive outside corners: no foreground/desktop queries there.
    bool blocked = hit.monitor >= 0 && Blocked(hit);
    if (!expiry) {
        bool newBlock = blocked && !nhc::Equal(engine.latched, hit);
        engine.Observe(monitors, monitorCount, p, config, GetTickCount64(), blocked);
        if (newBlock) ++blockedEntries;
    }
    // A zero delay executes in this input-event dispatch. Do not route it
    // through SetTimer, whose minimum interval would add an artificial delay.
    if (expiry || config.delay == 0) {
        int corner = engine.waiting.corner;
        nhc::Action action = engine.Expire(monitors, monitorCount, p, config, GetTickCount64(), blocked);
        if (action != nhc::None) {
            if (LaunchAction(action)) { ++activations[corner]; Log(ActionName(action)); }
            else { ++failures; Log("action-failed", lastErrorCode); }
        }
    }
    SyncTimer();
}
static bool WriteStatus(bool running = true) {
    char data[2048];
    int n = snprintf(data, sizeof(data),
        "{\n  \"version\":\"1.0.2\",\"pid\":%lu,\"running\":%s,\"paused\":%s,\"locked\":%s,\n"
        "  \"inputMode\":\"raw-input-events\",\"rawInputRegistered\":%s,\"inputDesktopAvailable\":%s,\"periodicPolling\":false,\"timerArmed\":%s,\n"
        "  \"uptimeMs\":%llu,\"rawEvents\":%llu,\"evaluations\":%llu,\"monitors\":%d,\n"
        "  \"delayMs\":%u,\"cooldownMs\":%u,\"cornerPixels\":%d,\"rearmPixels\":%d,\"fullscreenSuppression\":%s,\n"
        "  \"actions\":[\"%s\",\"%s\",\"%s\",\"%s\"],\"activations\":[%llu,%llu,%llu,%llu],\n"
        "  \"blockedEntries\":%llu,\"failures\":%llu,\"lastError\":%lu\n}\n",
        (unsigned long)GetCurrentProcessId(), running ? "true" : "false", paused ? "true" : "false", locked ? "true" : "false",
        rawRegistered ? "true" : "false", DefaultInputDesktop() ? "true" : "false", timerArmed ? "true" : "false",
        (unsigned long long)(GetTickCount64() - started), (unsigned long long)rawEvents, (unsigned long long)mouseEvaluations, monitorCount,
        config.delay, config.cooldown, config.pixels, config.rearm, config.fullscreen ? "true" : "false",
        ActionName(config.actions[0]), ActionName(config.actions[1]), ActionName(config.actions[2]), ActionName(config.actions[3]),
        (unsigned long long)activations[0], (unsigned long long)activations[1], (unsigned long long)activations[2], (unsigned long long)activations[3],
        (unsigned long long)blockedEntries, (unsigned long long)failures, (unsigned long)lastErrorCode);
    wchar_t temp[MAX_PATH]; MakePath(temp, L"latest-status.tmp");
    HANDLE file = CreateFileW(temp, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written; BOOL ok = WriteFile(file, data, n, &written, nullptr); CloseHandle(file);
    return ok && written == (DWORD)n && MoveFileExW(temp, statusPath, MOVEFILE_REPLACE_EXISTING);
}
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_INPUT: {
        RAWINPUT input; UINT bytes = sizeof(input);
        if (GetRawInputData((HRAWINPUT)l, RID_INPUT, &input, &bytes, sizeof(RAWINPUTHEADER)) != (UINT)-1 && input.header.dwType == RIM_TYPEMOUSE) {
            ++rawEvents;
            if (!paused && !locked && !evalPending) { evalPending = PostMessageW(hwnd, kEvaluate, 0, 0) != FALSE; }
        }
        return DefWindowProcW(hwnd, message, w, l);
    }
    case kEvaluate: evalPending = false; if (!paused && !locked) Evaluate(false); return 0;
    case WM_TIMER:
        if (w == kDwell) { KillTimer(hwnd, kDwell); timerArmed = false; Evaluate(true); }
        return 0;
    case WM_DISPLAYCHANGE: RefreshMonitors(); return 0;
    case WM_SETTINGCHANGE: RefreshMonitors(); return 0;
    case WM_WTSSESSION_CHANGE:
        if (w == WTS_SESSION_LOCK || w == WTS_REMOTE_DISCONNECT || w == WTS_CONSOLE_DISCONNECT) { locked = true; Reset(); }
        if (w == WTS_SESSION_UNLOCK || w == WTS_REMOTE_CONNECT || w == WTS_CONSOLE_CONNECT) { locked = false; RefreshMonitors(); }
        return 0;
    case WM_POWERBROADCAST: Reset(); if (w == PBT_APMRESUMEAUTOMATIC || w == PBT_APMRESUMESUSPEND) RefreshMonitors(); return TRUE;
    case kControl:
        if (w == Pause) { paused = true; Reset(); Log("paused"); }
        if (w == Resume) { paused = false; Reset(); Log("resumed"); }
        if (w == Reload) {
            if (!LoadConfig()) { Log("config-rejected"); return 2; }
            RefreshMonitors(); Log("config-reloaded");
        }
        if (w == Quit) { DestroyWindow(hwnd); return 1; }
        return WriteStatus() ? 1 : 0;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_QUERYENDSESSION: return TRUE;
    case WM_ENDSESSION: if (w) DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        Reset(); WTSUnRegisterSessionNotification(hwnd); rawRegistered = false;
        WriteStatus(false); Log("stopped"); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, message, w, l);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    int argc; wchar_t** args = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!args) return 10;
    if (argc > 1) {
        const wchar_t* option = args[1];
        int command = !wcscmp(option, L"--status") ? Status : !wcscmp(option, L"--pause") ? Pause :
            !wcscmp(option, L"--resume") ? Resume : !wcscmp(option, L"--reload") ? Reload :
            !wcscmp(option, L"--quit") ? Quit : 0;
        LocalFree(args);
        if (!command) return 11;
        HWND existing = FindWindowW(kClass, L"NativeHotCorners");
        if (!existing) return 3;
        DWORD_PTR result = 0;
        if (!SendMessageTimeoutW(existing, kControl, command, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 3000, &result)) return 4;
        return result == 1 ? 0 : result == 2 ? 5 : 6;
    }
    LocalFree(args);
    HANDLE mutex = CreateMutexW(nullptr, FALSE, kMutex);
    if (!mutex) return 12;
    if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(mutex); return 0; }
    DWORD pathLength = GetModuleFileNameW(nullptr, folder, MAX_PATH);
    if (!pathLength || pathLength >= MAX_PATH - 32) { CloseHandle(mutex); return 13; }
    wchar_t* slash = wcsrchr(folder, L'\\'); if (!slash) { CloseHandle(mutex); return 13; } *slash = 0;
    MakePath(iniPath, L"latest.ini"); MakePath(logPath, L"latest.log"); MakePath(statusPath, L"latest-status.json");
    if (!LoadConfig()) { Log("invalid-config-startup"); CloseHandle(mutex); return 14; }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW wc = {}; wc.lpfnWndProc = WindowProc; wc.hInstance = instance; wc.lpszClassName = kClass;
    if (!RegisterClassW(&wc)) { CloseHandle(mutex); return 16; }
    window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClass, L"NativeHotCorners", WS_POPUP,
                             0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!window) { CloseHandle(mutex); return 17; }
    started = GetTickCount64(); RefreshMonitors();
    RAWINPUTDEVICE device = { 1, 2, RIDEV_INPUTSINK, window };
    rawRegistered = RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
    if (!rawRegistered) { Log("raw-input-failed", GetLastError()); DestroyWindow(window); CloseHandle(mutex); return 18; }
    WTSRegisterSessionNotification(window, NOTIFY_FOR_THIS_SESSION);
    Log("started"); WriteStatus();
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    CloseHandle(mutex); return 0;
}
