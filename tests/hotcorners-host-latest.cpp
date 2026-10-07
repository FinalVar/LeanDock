// Integration tests never dispatch real mouse events or submit actions.
#include "../src/hotcorners-latest.cpp"
#include <string>
static unsigned passed,failed;
static void Check(bool ok,const char* name){if(ok)++passed;else{++failed;printf("FAIL: %s\n",name);}}
int main(){
    using namespace NativeCorners;
    wchar_t base[MAX_PATH];GetModuleFileNameW(nullptr,base,MAX_PATH);*wcsrchr(base,L'\\')=0;
    wcscat_s(base,L"\\hotcorners-host-fixture");CreateDirectoryW(base,nullptr);
    std::wstring sub=std::wstring(base)+L"\\hotcorners";CreateDirectoryW(sub.c_str(),nullptr);
    std::wstring cfg=sub+L"\\latest.ini";
    const wchar_t data[]=L"TopLeft=lock-screen\0TopRight=none\0BottomLeft=show-desktop\0BottomRight=task-view\0DelayMs=0\0CooldownMs=0\0CornerPixels=3\0RearmPixels=32\0Monitors=all\0PauseOnFullscreen=1\0\0";
    Check(WritePrivateProfileSectionW(L"HotCorners",data,cfg.c_str())!=FALSE,"test configuration created");
    Check(Validate(base),"current config valid");
    Check(!wcscmp(iniPath,cfg.c_str()),"wide-character absolute path preserved");
    Check(config.actions[2]==nhc::ShowDesktop && config.actions[3]==nhc::TaskView,"confirmed mapping preserved");
    Check(config.actions[0]==nhc::LockScreen && !strcmp(ActionName(config.actions[0]),"lock-screen"),"native lock action parses and reports correctly");
    Check(config.delay==0 && config.cooldown==0 && config.pixels==3 && config.rearm==32,"instant activation and spatial rearm preserved");
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"NativeDock.CornerFixture";RegisterClassW(&wc);
    HWND test=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
    Check(Initialize(test,base),"attach to host window");
    Check(Ready() && window==test,"same host window and active registration");
    RAWINPUTDEVICE devices[8];UINT count=8;
    UINT result=GetRegisteredRawInputDevices(devices,&count,sizeof(RAWINPUTDEVICE));
    Check(result==1 && devices[0].usUsagePage==1 && devices[0].usUsage==2 && devices[0].hwndTarget==test,"only mouse raw-input device registered on host");
    Check(!timerArmed && !evalPending,"no idle polling or dwell timer");
    HANDLE duplicate=CreateMutexW(nullptr,FALSE,kMutex);
    Check(duplicate && GetLastError()==ERROR_ALREADY_EXISTS,"legacy singleton conflict protected");if(duplicate)CloseHandle(duplicate);
    LRESULT reply=0;
    Check(Dispatch(test,ControlMessage,Pause,0,reply) && reply==1 && paused,"pause readback");
    Check(Dispatch(test,ControlMessage,Resume,0,reply) && reply==1 && !paused,"resume readback");
    WritePrivateProfileStringW(L"HotCorners",L"DelayMs",L"-1",cfg.c_str());
    Check(Dispatch(test,ControlMessage,Reload,0,reply) && reply==2 && config.delay==0,"invalid reload retains current config");
    WritePrivateProfileStringW(L"HotCorners",L"DelayMs",L"300",cfg.c_str());
    Check(Dispatch(test,ControlMessage,Reload,0,reply) && reply==1 && config.delay==300,"valid reload accepted");
    WritePrivateProfileStringW(L"HotCorners",L"BottomLeft",L"typo",cfg.c_str());
    Check(Dispatch(test,ControlMessage,Reload,0,reply) && reply==2 && config.actions[2]==nhc::ShowDesktop,"invalid action rejected atomically");
    WritePrivateProfileSectionW(L"HotCorners",data,cfg.c_str());
    Check(Dispatch(test,ControlMessage,Reload,0,reply) && reply==1 && config.delay==0,"original zero delay restored");
    Check(!Dispatch(test,WM_TIMER,1,0,reply),"Dock readiness timer not consumed");
    Check(!Dispatch(test,WM_DISPLAYCHANGE,0,0,reply) && monitorCount>0,"display event remains available to Dock");
    Dispatch(test,WM_WTSSESSION_CHANGE,WTS_SESSION_LOCK,0,reply);
    Check(locked && !timerArmed,"lock clears corner state");
    Dispatch(test,WM_WTSSESSION_CHANGE,WTS_SESSION_UNLOCK,0,reply);
    Check(!locked && monitorCount>0,"unlock refreshes geometry");
    Check(!Dispatch(test,WM_POWERBROADCAST,PBT_APMRESUMEAUTOMATIC,0,reply),"power event remains available to Dock");
    Check(activations[0]+activations[1]+activations[2]+activations[3]==0,"test submitted no action input");
    Check(Snapshot(),"hosted status writes successfully");
    Shutdown();Check(!Ready() && !initialized && !rawRegistered,"shutdown detaches feature");
    count=8;result=GetRegisteredRawInputDevices(devices,&count,sizeof(RAWINPUTDEVICE));
    Check(result==0,"raw-input registration actually removed");
    duplicate=CreateMutexW(nullptr,FALSE,kMutex);
    Check(duplicate && GetLastError()!=ERROR_ALREADY_EXISTS,"legacy singleton released");if(duplicate)CloseHandle(duplicate);
    Check(Initialize(test,base) && !paused,"reinitialization starts cleanly");Shutdown();DestroyWindow(test);
    printf("%u hosted checks passed, %u failed\n",passed,failed);return failed?1:0;
}
