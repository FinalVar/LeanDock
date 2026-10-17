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
text=text.replace('\\"version\\":\\"1.0.2\\",','\\"version\\":\\"1.0.4-integrated\\",\\"hosted\\":true,')
# Keep the upstream core, actions and fullscreen exclusions. Share the exact
# installed-client identity/maximization policy with the existing edge repair.
def replace_once(old, new):
    global text
    assert text.count(old) == 1, f'Hot-corner adaptation anchor drifted: {old[:80]}'
    text=text.replace(old,new,1)

replace_once('#include "hotcorners-latest.h"', '#include "hotcorners-latest.h"\n#include "../hotcorners/fullscreen/latest.h"')
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
(root/'src/hotcorners-latest.cpp').write_text(text,encoding='utf-8',newline='\n')
print('NativeHotCorners adapted as an event handler in the existing controller.')
