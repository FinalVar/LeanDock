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
#include "../hotcorners/core/latest.h"
#include "../hotcorners/actions/latest.h"
#include "hotcorners-latest.h"
#include "../hotcorners/fullscreen/latest.h"

namespace NativeCorners {
static const wchar_t* kClass = L"NativeHotCorners.1.Window";
static const wchar_t* kMutex = L"Local\\NativeHotCorners.1.Instance";
enum { kEvaluate = WM_APP + 220, kDwell = 2 };
static HANDLE instanceMutex;
static bool initialized;
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
enum class BlockReason { None, Paused, Locked, InputHeld, Desktop, Fullscreen };
static uint64_t blockReasons[6]{},maximizedClientEntries,lastBlockAt;
static BlockReason lastBlockReason=BlockReason::None;
static DWORD lastBlockPid;
static int lastBlockCorner=-1;
static bool maximizedClientAllowed,lastBlockMaximized;
static unsigned long lastBlockStyle;
static const char* BlockName(BlockReason reason) {
    switch(reason) {
    case BlockReason::Paused:return "paused";
    case BlockReason::Locked:return "locked";
    case BlockReason::InputHeld:return "input-held";
    case BlockReason::Desktop:return "input-desktop";
    case BlockReason::Fullscreen:return "fullscreen";
    default:return "none";
    }
}
static void RecordBlock(BlockReason reason,int corner) {
    ++blockedEntries;++blockReasons[static_cast<unsigned>(reason)];
    lastBlockReason=reason;lastBlockCorner=corner;lastBlockAt=GetTickCount64();
    HWND foreground=GetForegroundWindow();lastBlockPid=0;
    GetWindowThreadProcessId(foreground,&lastBlockPid);
    lastBlockMaximized=IsZoomed(foreground)!=FALSE;
    lastBlockStyle=static_cast<unsigned long>(GetWindowLongPtrW(foreground,GWL_STYLE));
}

static void MakePath(wchar_t* result, const wchar_t* name) { swprintf(result, MAX_PATH, L"%ls\\%ls", folder, name); }
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
    return action == nhc::LockScreen ? "lock-screen" : action == nhc::TaskView ? "task-view" : action == nhc::ShowDesktop ? "show-desktop" : action == nhc::StartMenu ? "start-menu" : "none";
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
    if(GetFileAttributesW(iniPath)==INVALID_FILE_ATTRIBUTES)return false;
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
        else if (!_wcsicmp(value, L"lock-screen")) next.actions[c] = nhc::LockScreen;
        else if (!_wcsicmp(value, L"none")) next.actions[c] = nhc::None;
        else return false;
    }
    const wchar_t* delayKeys[] = { L"TopLeftDelayMs", L"TopRightDelayMs", L"BottomLeftDelayMs", L"BottomRightDelayMs" };
    for (int c = 0; c < 4; ++c)
        if (!ReadNumber(delayKeys[c], delay, 0, 1500, next.cornerDelay[c])) return false;
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
    const LONG_PTR style=GetWindowLongPtrW(foreground,GWL_STYLE);
    if ((style & WS_CAPTION) == WS_CAPTION) return false;
    MONITORINFO mi = { sizeof(mi) }; RECT rect;
    if (!GetMonitorInfoW(target, &mi)) return false;
    if (FAILED(DwmGetWindowAttribute(foreground, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect))) && !GetWindowRect(foreground, &rect)) return false;
    // Resolve identity only for an actual screen-covering candidate. Do not
    // depend on asynchronous NonRudeHWND acquisition or clear fullscreen
    // suppression for every maximized game/browser.
    if(!nhc::SuppressFullscreen(style,rect,mi.rcMonitor,false,false))return false;
    const bool maximized=IsZoomed(foreground)!=FALSE;
    const bool client=maximized && (style&WS_THICKFRAME) &&
        LeanDockClientEdgePolicy::ClientWindow(foreground);
    const bool suppress=nhc::SuppressFullscreen(style,rect,mi.rcMonitor,maximized,client);
    maximizedClientAllowed=!suppress;return suppress;
}
static BlockReason Blocked(nhc::Target target) {
    if(paused)return BlockReason::Paused;
    if(locked)return BlockReason::Locked;
    if(ModifierOrButton())return BlockReason::InputHeld;
    if(!DefaultInputDesktop())return BlockReason::Desktop;
    return target.monitor >= 0 && target.monitor < monitorCount && Fullscreen(monitorIds[target.monitor])
        ? BlockReason::Fullscreen : BlockReason::None;
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
    if(action == nhc::LockScreen)return LockWorkStation()!=FALSE;
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
    maximizedClientAllowed=false;
    const BlockReason reason=hit.monitor>=0 ? Blocked(hit) : BlockReason::None;
    const bool blocked=reason!=BlockReason::None;
    if(maximizedClientAllowed && !nhc::Equal(engine.latched,hit) && !nhc::Equal(engine.waiting,hit))
        ++maximizedClientEntries;
    if (!expiry) {
        bool newBlock = blocked && !nhc::Equal(engine.latched, hit);
        engine.Observe(monitors, monitorCount, p, config, GetTickCount64(), blocked);
        if (newBlock) RecordBlock(reason,hit.corner);
    }
    // A zero delay executes in this input-event dispatch. Do not route it
    // through SetTimer, whose minimum interval would add an artificial delay.
    if (expiry || config.Delay(engine.waiting.corner) == 0) {
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
    char data[4096];
    int n = snprintf(data, sizeof(data),
        "{\n  \"version\":\"1.0.4-integrated\",\"hosted\":true,\"pid\":%lu,\"running\":%s,\"paused\":%s,\"locked\":%s,\n"
        "  \"inputMode\":\"raw-input-events\",\"rawInputRegistered\":%s,\"inputDesktopAvailable\":%s,\"periodicPolling\":false,\"timerArmed\":%s,\n"
        "  \"uptimeMs\":%llu,\"rawEvents\":%llu,\"evaluations\":%llu,\"monitors\":%d,\n"
        "  \"delayMs\":%u,\"cornerDelayMs\":{\"topLeft\":%u,\"topRight\":%u,\"bottomLeft\":%u,\"bottomRight\":%u},\"cooldownMs\":%u,\"cornerPixels\":%d,\"rearmPixels\":%d,\"fullscreenSuppression\":%s,\n"
        "  \"actions\":[\"%s\",\"%s\",\"%s\",\"%s\"],\"activations\":[%llu,%llu,%llu,%llu],\n"
        "  \"blockedEntries\":%llu,\"failures\":%llu,\"lastError\":%lu,\n  \"maximizedClientEntries\":%llu,\"blockReasons\":{\"paused\":%llu,\"locked\":%llu,\"inputHeld\":%llu,\"inputDesktop\":%llu,\"fullscreen\":%llu},\n  \"lastBlock\":{\"reason\":\"%s\",\"pid\":%lu,\"corner\":%d,\"tick\":%llu,\"maximized\":%s,\"style\":%lu}\n}\n",
        (unsigned long)GetCurrentProcessId(), running ? "true" : "false", paused ? "true" : "false", locked ? "true" : "false",
        rawRegistered ? "true" : "false", DefaultInputDesktop() ? "true" : "false", timerArmed ? "true" : "false",
        (unsigned long long)(GetTickCount64() - started), (unsigned long long)rawEvents, (unsigned long long)mouseEvaluations, monitorCount,
        config.delay, config.Delay(0), config.Delay(1), config.Delay(2), config.Delay(3), config.cooldown, config.pixels, config.rearm, config.fullscreen ? "true" : "false",
        ActionName(config.actions[0]), ActionName(config.actions[1]), ActionName(config.actions[2]), ActionName(config.actions[3]),
        (unsigned long long)activations[0], (unsigned long long)activations[1], (unsigned long long)activations[2], (unsigned long long)activations[3],
        (unsigned long long)blockedEntries, (unsigned long long)failures, (unsigned long)lastErrorCode,
        (unsigned long long)maximizedClientEntries,
        (unsigned long long)blockReasons[1],(unsigned long long)blockReasons[2],
        (unsigned long long)blockReasons[3],(unsigned long long)blockReasons[4],(unsigned long long)blockReasons[5],
        BlockName(lastBlockReason),(unsigned long)lastBlockPid,lastBlockCorner,(unsigned long long)lastBlockAt,
        lastBlockMaximized ? "true" : "false",lastBlockStyle);
    if(n<0 || static_cast<size_t>(n)>=sizeof(data))return false;
    wchar_t temp[MAX_PATH]; MakePath(temp, L"latest-status.tmp");
    HANDLE file = CreateFileW(temp, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written; BOOL ok = WriteFile(file, data, n, &written, nullptr); CloseHandle(file);
    return ok && written == (DWORD)n && MoveFileExW(temp, statusPath, MOVEFILE_REPLACE_EXISTING);
}

static bool Paths(const wchar_t* root) {
    if(wcslen(root)+48>=MAX_PATH)return false;
    swprintf(folder,MAX_PATH,L"%ls\\hotcorners",root);
    MakePath(iniPath,L"latest.ini");MakePath(logPath,L"latest.log");MakePath(statusPath,L"latest-status.json");
    return true;
}
bool Validate(const wchar_t* root) {return Paths(root) && LoadConfig();}
bool Ready() {return initialized && rawRegistered;}
bool Snapshot() {return initialized && WriteStatus();}
bool Initialize(HWND owner,const wchar_t* root) {
    if(initialized)return true;
    if(!Paths(root) || !LoadConfig())return false;
    instanceMutex=CreateMutexW(nullptr,FALSE,kMutex);
    if(!instanceMutex)return false;
    if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(instanceMutex);instanceMutex=nullptr;return false;}
    window=owner;paused=false;locked=false;evalPending=false;engine={};started=GetTickCount64();RefreshMonitors();
    RAWINPUTDEVICE device={1,2,RIDEV_INPUTSINK,window};
    rawRegistered=RegisterRawInputDevices(&device,1,sizeof(device))!=FALSE;
    if(!rawRegistered){CloseHandle(instanceMutex);instanceMutex=nullptr;Log("raw-input-failed",GetLastError());return false;}
    if(!WTSRegisterSessionNotification(window,NOTIFY_FOR_THIS_SESSION)){
        RAWINPUTDEVICE remove={1,2,RIDEV_REMOVE,nullptr};RegisterRawInputDevices(&remove,1,sizeof(remove));
        rawRegistered=false;CloseHandle(instanceMutex);instanceMutex=nullptr;return false;
    }
    initialized=true;Log("integrated-started");WriteStatus();return true;
}
void Shutdown() {
    if(!initialized)return;
    paused=true;Reset();
    RAWINPUTDEVICE remove={1,2,RIDEV_REMOVE,nullptr};RegisterRawInputDevices(&remove,1,sizeof(remove));
    WTSUnRegisterSessionNotification(window);rawRegistered=false;initialized=false;
    WriteStatus(false);Log("integrated-stopped");
    if(instanceMutex){CloseHandle(instanceMutex);instanceMutex=nullptr;}
}
bool Dispatch(HWND hwnd,UINT message,WPARAM w,LPARAM l,LRESULT& result) {
    if(!initialized)return false;
    result=0;
    switch(message){
    case WM_INPUT: {
        RAWINPUT input;UINT bytes=sizeof(input);
        if(GetRawInputData((HRAWINPUT)l,RID_INPUT,&input,&bytes,sizeof(RAWINPUTHEADER))!=(UINT)-1 && input.header.dwType==RIM_TYPEMOUSE){
            ++rawEvents;
            if(!paused && !locked && !evalPending)evalPending=PostMessageW(hwnd,kEvaluate,0,0)!=FALSE;
        }
        result=DefWindowProcW(hwnd,message,w,l);return true;
    }
    case kEvaluate:evalPending=false;if(!paused && !locked)Evaluate(false);return true;
    case WM_TIMER:
        if(w!=kDwell)return false;
        KillTimer(hwnd,kDwell);timerArmed=false;Evaluate(true);return true;
    case WM_DISPLAYCHANGE:case WM_SETTINGCHANGE:RefreshMonitors();return false;
    case WM_WTSSESSION_CHANGE:
        if(w==WTS_SESSION_LOCK || w==WTS_REMOTE_DISCONNECT || w==WTS_CONSOLE_DISCONNECT){locked=true;Reset();}
        if(w==WTS_SESSION_UNLOCK || w==WTS_REMOTE_CONNECT || w==WTS_CONSOLE_CONNECT){locked=false;RefreshMonitors();}
        return false;
    case WM_POWERBROADCAST:Reset();if(w==PBT_APMRESUMEAUTOMATIC || w==PBT_APMRESUMESUSPEND)RefreshMonitors();return false;
    case ControlMessage:
        if(w==Pause){paused=true;Reset();Log("paused");}
        else if(w==Resume){paused=false;Reset();Log("resumed");}
        else if(w==Reload){if(!LoadConfig()){Log("config-rejected");result=2;return true;}RefreshMonitors();Log("config-reloaded");}
        else if(w!=Status){result=0;return true;}
        result=WriteStatus()?1:0;return true;
    }
    return false;
}
} // namespace NativeCorners
