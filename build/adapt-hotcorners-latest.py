"""Embed the locally maintained NativeHotCorners 1.0.2 without a second host."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
text=(root/'upstream/NativeHotCorners/latest.cpp').read_text(encoding='utf-8-sig')
text=text.replace('#include "core/latest.h"','#include "../hotcorners/core/latest.h"')
text=text.replace('#include "actions/latest.h"','#include "../hotcorners/actions/latest.h"\n#include "hotcorners-latest.h"')
text=text.replace('#define UNICODE\n','').replace('#define _UNICODE\n','')
text=text.replace('action == nhc::TaskView ? "task-view"', 'action == nhc::LockScreen ? "lock-screen" : action == nhc::TaskView ? "task-view"')
text=text.replace('else if (!_wcsicmp(value, L"start-menu")) next.actions[c] = nhc::StartMenu;',
                  'else if (!_wcsicmp(value, L"start-menu")) next.actions[c] = nhc::StartMenu;\n        else if (!_wcsicmp(value, L"lock-screen")) next.actions[c] = nhc::LockScreen;')
text=text.replace('static bool LaunchAction(nhc::Action action) {',
                  'static bool LaunchAction(nhc::Action action) {\n    if(action == nhc::LockScreen)return LockWorkStation()!=FALSE;')
# ISO wide printf uses %ls for wide strings.  The original Zig/MSVCRT build
# accepted %s; this build uses MinGW's standards-conforming formatter.
text=text.replace('L"%s\\\\%s"', 'L"%ls\\\\%ls"')
start=text.index('static const wchar_t* kClass')
text=text[:start]+'namespace NativeCorners {\n'+text[start:]
text=text.replace('enum { kEvaluate = WM_APP + 1, kControl = WM_APP + 2, kDwell = 1 };','enum { kEvaluate = WM_APP + 220, kDwell = 2 };')
text=text.replace('enum { Status = 1, Pause, Resume, Reload, Quit };','static HANDLE instanceMutex;\nstatic bool initialized;')
text=text.replace('static bool LoadConfig() {','static bool LoadConfig() {\n    if(GetFileAttributesW(iniPath)==INVALID_FILE_ATTRIBUTES)return false;')
text=text.replace('    next.delay = delay; next.cooldown = cooldown;', '''    const wchar_t* delayKeys[] = { L"TopLeftDelayMs", L"TopRightDelayMs", L"BottomLeftDelayMs", L"BottomRightDelayMs" };
    for (int c = 0; c < 4; ++c)
        if (!ReadNumber(delayKeys[c], delay, 0, 1500, next.cornerDelay[c])) return false;
    next.delay = delay; next.cooldown = cooldown;''')
text=text.replace('if (expiry || config.delay == 0)', 'if (expiry || config.Delay(engine.waiting.corner) == 0)')
text=text.replace('config.delay, config.cooldown, config.pixels',
                  'config.delay, config.Delay(0), config.Delay(1), config.Delay(2), config.Delay(3), config.cooldown, config.pixels')
text=text.replace(r'\"delayMs\":%u,\"cooldownMs\":%u',
                  r'\"delayMs\":%u,\"cornerDelayMs\":{\"topLeft\":%u,\"topRight\":%u,\"bottomLeft\":%u,\"bottomRight\":%u},\"cooldownMs\":%u')
text=text.replace('static bool WriteStatus(bool running = true)', 'static bool WriteStatus(bool running = true)')
text=text.replace('\\"version\\":\\"1.0.2\\",','\\"version\\":\\"1.0.6-integrated\\",\\"hosted\\":true,')
# Keep the upstream core, actions and fullscreen exclusions. Share the exact
# installed-client identity/maximization policy with the existing edge repair.
def replace_once(old, new):
    global text
    assert text.count(old) == 1, f'Hot-corner adaptation anchor drifted: {old[:80]}'
    text=text.replace(old,new,1)

replace_once('action == nhc::TaskView ? "task-view"',
             'action == nhc::AppSwitcher ? "app-switcher" : action == nhc::TaskView ? "task-view"')
replace_once('if (!_wcsicmp(value, L"task-view")) next.actions[c] = nhc::TaskView;',
'''if (!_wcsicmp(value, L"task-view")) next.actions[c] = nhc::TaskView;
        else if (!_wcsicmp(value, L"app-switcher")) next.actions[c] = nhc::AppSwitcher;''')
replace_once('INPUT keys[4]; UINT count = nhc::BuildActionInput(action, keys);',
             'INPUT keys[nhc::ActionInputCapacity]; UINT count = nhc::BuildActionInput(action, keys);')
replace_once('if (ModifierOrButton() || (count == 4 && (GetAsyncKeyState(keys[1].ki.wVk) & 0x8000))) {',
'''if (ModifierOrButton() || nhc::AnyActionKeyHeld(keys, count, [](WORD key) {
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    })) {''')
replace_once('''    // Release only keys whose injected key-down was accepted but key-up was
    // not, so partial submission cannot leave our Windows/Tab key held down.
    INPUT releases[2]; UINT releaseCount = 0;
    if (count == 4 && sent == 2) releases[releaseCount++] = keys[2];
    if (sent > 0 && sent < count) releases[releaseCount++] = keys[count - 1];''',
'''    // Best-effort cleanup also covers partial Ctrl+Alt+Tab submission.
    // A failed action remains a failure even if cleanup succeeds.
    INPUT releases[nhc::ActionInputCapacity];
    UINT releaseCount = nhc::BuildReleaseInput(keys, count, sent, releases);''')

replace_once('#include "hotcorners-latest.h"', '#include "hotcorners-latest.h"\n#include "../hotcorners/fullscreen/latest.h"')
replace_once('#include "hotcorners-latest.h"', '#include "hotcorners-latest.h"\n#include "protocol-latest.h"')
replace_once('static bool LaunchAction(nhc::Action action) {', '''static void PrepareSwitcher(bool arm) {
    HWND backend = FindWindowW(NativeBackendClass, nullptr);
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    DWORD backendPid=0, shellPid=0;
    if (!backend || !taskbar || !GetWindowThreadProcessId(backend, &backendPid) ||
        !GetWindowThreadProcessId(taskbar, &shellPid) || backendPid!=shellPid) return;
    DWORD_PTR result=0;
    SendMessageTimeoutW(backend, NativeSwitcherPrepare, GetCurrentProcessId(), arm ? 1 : 0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 150, &result);
}
static bool LaunchAction(nhc::Action action) {''')
replace_once('    SetLastError(ERROR_SUCCESS);\n    UINT sent = SendInput',
             '    if (action == nhc::AppSwitcher) PrepareSwitcher(true);\n    SetLastError(ERROR_SUCCESS);\n    UINT sent = SendInput')
replace_once('    if (sent == count) return true;',
             '    if (sent == count) return true;\n    if (action == nhc::AppSwitcher) PrepareSwitcher(false);')
# Native Ctrl+Alt+Tab is an opener, not a toggle. Choose cancellation only for
# an actively marked native switcher, and revalidate after the bounded IPC.
replace_once('#include "protocol-latest.h"',
             '#include "protocol-latest.h"\n#include "switcher-visibility-latest.h"')
replace_once('static DWORD lastErrorCode;',
             'static DWORD lastErrorCode;\nstatic uint64_t switcherOpenRequests{},switcherCloseRequests{},switcherStateChanged{};')
replace_once('    INPUT keys[nhc::ActionInputCapacity]; UINT count = nhc::BuildActionInput(action, keys);',
'''    const HWND sourceForeground=GetForegroundWindow();
    const HWND dismissWindow=action==nhc::AppSwitcher ? LeanDockSwitcherVisibility::ActiveWindow() : nullptr;
    INPUT keys[nhc::ActionInputCapacity]; UINT count = nhc::BuildActionInput(action, keys, dismissWindow!=nullptr);''')
replace_once('    if (action == nhc::AppSwitcher) PrepareSwitcher(true);',
'''    if (action == nhc::AppSwitcher) {
        PrepareSwitcher(!dismissWindow);
        // Focus/visibility can change while the backend processes the ticket.
        // Abort that entry; never send a stale Escape to another application.
        if ((dismissWindow && LeanDockSwitcherVisibility::ActiveWindow()!=dismissWindow) ||
            (!dismissWindow && (LeanDockSwitcherVisibility::ActiveWindow() || GetForegroundWindow()!=sourceForeground))) {
            PrepareSwitcher(false); ++switcherStateChanged;
            lastErrorCode=ERROR_RETRY; return false;
        }
    }''')
replace_once('    if (sent == count) return true;',
'''    if (sent == count) {
        if (action==nhc::AppSwitcher) {
            if (dismissWindow) { ++switcherCloseRequests; Log("switcher-close-request"); }
            else { ++switcherOpenRequests; Log("switcher-open-request"); }
        }
        return true;
    }''')
replace_once(r'  \"blockedEntries\":%llu,\"failures\":%llu,\"lastError\":%lu\n}\n',
             r'  \"switcherToggle\":{\"openRequests\":%llu,\"closeRequests\":%llu,\"stateChanged\":%llu},\n'
             r'  \"blockedEntries\":%llu,\"failures\":%llu,\"lastError\":%lu\n}\n')
replace_once('        (unsigned long long)blockedEntries, (unsigned long long)failures, (unsigned long)lastErrorCode);',
'''        (unsigned long long)switcherOpenRequests,(unsigned long long)switcherCloseRequests,(unsigned long long)switcherStateChanged,
        (unsigned long long)blockedEntries, (unsigned long long)failures, (unsigned long)lastErrorCode);''')
text=text.replace('1.0.6-integrated','1.0.14-integrated')
replace_once('static DWORD lastErrorCode;', '''static DWORD lastErrorCode;
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
}''')
replace_once('    if ((GetWindowLongPtrW(foreground, GWL_STYLE) & WS_CAPTION) == WS_CAPTION) return false;',
'''    const LONG_PTR style=GetWindowLongPtrW(foreground,GWL_STYLE);
    if ((style & WS_CAPTION) == WS_CAPTION) return false;''')
replace_once('''    return rect.left <= mi.rcMonitor.left && rect.top <= mi.rcMonitor.top &&
           rect.right >= mi.rcMonitor.right && rect.bottom >= mi.rcMonitor.bottom;''',
'''    // Resolve identity only for an actual screen-covering candidate. Do not
    // depend on asynchronous NonRudeHWND acquisition or clear fullscreen
    // suppression for every maximized game/browser.
    if(!nhc::SuppressFullscreen(style,rect,mi.rcMonitor,false,false))return false;
    const bool maximized=IsZoomed(foreground)!=FALSE;
    const bool client=maximized && (style&WS_THICKFRAME) &&
        LeanDockClientEdgePolicy::ClientWindow(foreground);
    const bool suppress=nhc::SuppressFullscreen(style,rect,mi.rcMonitor,maximized,client);
    maximizedClientAllowed=!suppress;return suppress;''')
replace_once('''static bool Blocked(nhc::Target target) {
    if (paused || locked || ModifierOrButton() || !DefaultInputDesktop()) return true;
    return target.monitor >= 0 && target.monitor < monitorCount && Fullscreen(monitorIds[target.monitor]);
}''', '''static BlockReason Blocked(nhc::Target target) {
    if(paused)return BlockReason::Paused;
    if(locked)return BlockReason::Locked;
    if(ModifierOrButton())return BlockReason::InputHeld;
    if(!DefaultInputDesktop())return BlockReason::Desktop;
    return target.monitor >= 0 && target.monitor < monitorCount && Fullscreen(monitorIds[target.monitor])
        ? BlockReason::Fullscreen : BlockReason::None;
}''')
replace_once('    bool blocked = hit.monitor >= 0 && Blocked(hit);', '''    maximizedClientAllowed=false;
    const BlockReason reason=hit.monitor>=0 ? Blocked(hit) : BlockReason::None;
    const bool blocked=reason!=BlockReason::None;
    if(maximizedClientAllowed && !nhc::Equal(engine.latched,hit) && !nhc::Equal(engine.waiting,hit))
        ++maximizedClientEntries;''')
replace_once('        if (newBlock) ++blockedEntries;', '        if (newBlock) RecordBlock(reason,hit.corner);')
replace_once('    char data[2048];','    char data[4096];')
replace_once(r'  \"blockedEntries\":%llu,\"failures\":%llu,\"lastError\":%lu\n}\n',
             r'  \"blockedEntries\":%llu,\"failures\":%llu,\"lastError\":%lu,\n'
             r'  \"maximizedClientEntries\":%llu,\"blockReasons\":{\"paused\":%llu,\"locked\":%llu,\"inputHeld\":%llu,\"inputDesktop\":%llu,\"fullscreen\":%llu},\n'
             r'  \"lastBlock\":{\"reason\":\"%s\",\"pid\":%lu,\"corner\":%d,\"tick\":%llu,\"maximized\":%s,\"style\":%lu}\n}\n')
replace_once('        (unsigned long long)blockedEntries, (unsigned long long)failures, (unsigned long)lastErrorCode);',
'''        (unsigned long long)blockedEntries, (unsigned long long)failures, (unsigned long)lastErrorCode,
        (unsigned long long)maximizedClientEntries,
        (unsigned long long)blockReasons[1],(unsigned long long)blockReasons[2],
        (unsigned long long)blockReasons[3],(unsigned long long)blockReasons[4],(unsigned long long)blockReasons[5],
        BlockName(lastBlockReason),(unsigned long)lastBlockPid,lastBlockCorner,(unsigned long long)lastBlockAt,
        lastBlockMaximized ? "true" : "false",lastBlockStyle);
    if(n<0 || static_cast<size_t>(n)>=sizeof(data))return false;''')
text=text[:text.index('static LRESULT CALLBACK WindowProc')]
text+='''
static bool Paths(const wchar_t* root) {
    if(wcslen(root)+48>=MAX_PATH)return false;
    swprintf(folder,MAX_PATH,L"%ls\\\\hotcorners",root);
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
'''
replace_once('#include "../hotcorners/fullscreen/latest.h"',
    '#include "../hotcorners/fullscreen/latest.h"\n#include "start-menu-state-latest.h"\n#include "../hotcorners/start-menu/policy-latest.h"')
replace_once('static void Reset() {',
    'static void CancelStartDismiss();\nstatic void Reset() {\n    CancelStartDismiss();')
replace_once('    if (MonitorFromWindow(foreground, MONITOR_DEFAULTTONULL) != target) return false;',
    '''    // Start is a verified shell popup, not a fullscreen application.
    if(LeanDockStartState::Host(foreground))return false;
    if (MonitorFromWindow(foreground, MONITOR_DEFAULTTONULL) != target) return false;''')
replace_once('static void Evaluate(bool expiry) {',
    '#include "../hotcorners/start-menu/latest.inc"\nstatic void Evaluate(bool expiry) {')
replace_once('''            if (LaunchAction(action)) { ++activations[corner]; Log(ActionName(action)); }
            else { ++failures; Log("action-failed", lastErrorCode); }''',
    '            DispatchCorner(action,corner);')
replace_once('    case WM_TIMER:\n        if(w!=kDwell)return false;',
    '    case WM_TIMER:\n        if(w==kStartDismiss){ContinueAfterStart();return true;}\n        if(w!=kDwell)return false;')
# Fullscreen applications keep the corners; only games and remote clients
# pause them, as for the Dock (include/fullscreen-app-policy-latest.h).
# PauseOnFullscreen=1 means "in those"; Games= lists more games by file name.
replace_once('#include "../hotcorners/fullscreen/latest.h"',
    '#include "../hotcorners/fullscreen/latest.h"\n#include "fullscreen-app-policy-latest.h"')
replace_once('    config = next; return true;',
'''    wchar_t games[std::size(LeanDockFullscreen::listed)];
    GetPrivateProfileStringW(L"HotCorners", L"Games", L"", games, std::size(games), iniPath);
    config = next; wmemcpy(LeanDockFullscreen::listed, games, std::size(games)); return true;''')
replace_once('static bool maximizedClientAllowed,lastBlockMaximized;',
    'static bool maximizedClientAllowed,fullscreenAppAllowed,lastBlockMaximized;\nstatic uint64_t fullscreenAppEntries;')
replace_once('    maximizedClientAllowed=!suppress;return suppress;',
'''    const bool quiet=suppress && LeanDockFullscreen::Quiet(foreground);
    maximizedClientAllowed=!suppress;fullscreenAppAllowed=suppress && !quiet;return quiet;''')
# The Dock over fullscreen applications follows the pointer through the same
# coalesced raw input, also while the corners are paused.
replace_once('bool Validate(const wchar_t* root) {return Paths(root) && LoadConfig();}',
    'void (*pointerObserver)()=nullptr;\nbool Validate(const wchar_t* root) {return Paths(root) && LoadConfig();}')
replace_once('            if(!paused && !locked && !evalPending)evalPending=PostMessageW(hwnd,kEvaluate,0,0)!=FALSE;',
    '            if(!locked && !evalPending && (!paused || pointerObserver))evalPending=PostMessageW(hwnd,kEvaluate,0,0)!=FALSE;')
replace_once('    case kEvaluate:evalPending=false;if(!paused && !locked)Evaluate(false);return true;',
    '    case kEvaluate:evalPending=false;if(pointerObserver && !locked)pointerObserver();if(!paused && !locked)Evaluate(false);return true;')
replace_once('    maximizedClientAllowed=false;\n','    maximizedClientAllowed=false;fullscreenAppAllowed=false;\n')
replace_once('        ++maximizedClientEntries;',
'''        ++maximizedClientEntries;
    if(fullscreenAppAllowed && !nhc::Equal(engine.latched,hit) && !nhc::Equal(engine.waiting,hit))
        ++fullscreenAppEntries;''')
replace_once(r'\"maximizedClientEntries\":%llu,', r'\"maximizedClientEntries\":%llu,\"fullscreenAppEntries\":%llu,')
replace_once('        (unsigned long long)maximizedClientEntries,\n',
    '        (unsigned long long)maximizedClientEntries,(unsigned long long)fullscreenAppEntries,\n')
# Win+D only covers the windows it cannot minimize; the desktop corner also
# minimizes those, and returns them when it leaves the desktop again
# (include/desktop-sweep-latest.h).
replace_once('#include "fullscreen-app-policy-latest.h"',
    '#include "fullscreen-app-policy-latest.h"\n#include "desktop-sweep-latest.h"')
replace_once('static bool LaunchAction(nhc::Action action) {',
'''static bool SendAction(nhc::Action action);
static bool LaunchAction(nhc::Action action) {
    const bool desktop=action==nhc::ShowDesktop;
    if(desktop)LeanDockDesktopSweep::Prepare(window);
    const bool sent=SendAction(action);
    if(desktop)LeanDockDesktopSweep::Sent(sent);
    return sent;
}
static bool SendAction(nhc::Action action) {''')
replace_once('        if(w!=kDwell)return false;',
    '        if(LeanDockDesktopSweep::Timer(hwnd,w))return true;\n        if(w!=kDwell)return false;')
replace_once('    paused=true;Reset();\n    RAWINPUTDEVICE remove',
    '    paused=true;Reset();LeanDockDesktopSweep::Stop();\n    RAWINPUTDEVICE remove')
replace_once('initialized=true;Log("integrated-started");',
    'LeanDockDesktopSweep::log=[](const char* e,unsigned c){Log(e,c);};initialized=true;Log("integrated-started");')
replace_once(r'\"fullscreenAppEntries\":%llu,',
    r'\"fullscreenAppEntries\":%llu,\"desktopSweep\":{\"sweeps\":%llu,\"minimized\":%llu,\"restored\":%llu,\"ended\":%llu,\"burials\":%llu,\"buried\":%u,\"open\":%s},')
replace_once('(unsigned long long)fullscreenAppEntries,\n',
    '(unsigned long long)fullscreenAppEntries,(unsigned long long)LeanDockDesktopSweep::sweeps,(unsigned long long)LeanDockDesktopSweep::minimized,\n'
    '        (unsigned long long)LeanDockDesktopSweep::restored,(unsigned long long)LeanDockDesktopSweep::ended,(unsigned long long)LeanDockDesktopSweep::burials,LeanDockDesktopSweep::buriedCount,LeanDockDesktopSweep::open ? "true" : "false",\n')
(root/'src/hotcorners-latest.cpp').write_text(text,encoding='utf-8',newline='\n')
print('NativeHotCorners adapted as an event handler in the existing controller.')
