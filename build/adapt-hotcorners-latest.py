"""Embed the locally maintained NativeHotCorners 1.0.2 without a second host."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
text=(root/'upstream/NativeHotCorners/latest.cpp').read_text(encoding='utf-8-sig')
text=text.replace('#include "core/latest.h"','#include "../hotcorners/core/latest.h"')
text=text.replace('#include "actions/latest.h"','#include "../hotcorners/actions/latest.h"\n#include "hotcorners-latest.h"')
text=text.replace('#define UNICODE\n','').replace('#define _UNICODE\n','')
# ISO wide printf uses %ls for wide strings.  The original Zig/MSVCRT build
# accepted %s; this build uses MinGW's standards-conforming formatter.
text=text.replace('L"%s\\\\%s"', 'L"%ls\\\\%ls"')
start=text.index('static const wchar_t* kClass')
text=text[:start]+'namespace NativeCorners {\n'+text[start:]
text=text.replace('enum { kEvaluate = WM_APP + 1, kControl = WM_APP + 2, kDwell = 1 };','enum { kEvaluate = WM_APP + 220, kDwell = 2 };')
text=text.replace('enum { Status = 1, Pause, Resume, Reload, Quit };','static HANDLE instanceMutex;\nstatic bool initialized;')
text=text.replace('static bool LoadConfig() {','static bool LoadConfig() {\n    if(GetFileAttributesW(iniPath)==INVALID_FILE_ATTRIBUTES)return false;')
text=text.replace('static bool WriteStatus(bool running = true)', 'static bool WriteStatus(bool running = true)')
text=text.replace('\\"version\\":\\"1.0.2\\",','\\"version\\":\\"1.0.2-integrated\\",\\"hosted\\":true,')
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
