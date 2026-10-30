#include "platform-latest.h"
#include "composition-latest.h"
#include "drawing-latest.h"
#include "apps-latest.h"
#include "metrics-latest.h"
#include "popup-latest.h"
#include "tray-latest.h"
#include "dashboard-latest.h"
#include "visual-probe-latest.h"
#include "protocol-tests-latest.h"
#include "time-key-tests-latest.h"
#include "control-tests-latest.h"
#include "../include/dock-geometry-latest.h"
#include "companion-host-latest.h"
#include <shellscalingapi.h>
#include <memory>
#include <atomic>

using namespace Capsule;
static HWND uiWindow{},guardWindow{};
static HHOOK keyboardHook{};
static bool keyboardPinned=false,swallowedT=false,swallowedEscape=false;
static HWINEVENTHOOK taskbarHook{};
static bool stopping=false,probe=false;
static bool visualProbe=false;
static bool iconProbe=false;
static bool timeKeyProbe=false;
static bool controlPanelProbe=false;
static unsigned visualStage=0;
static bool idleProbe=false;
static uint64_t idleStarted{},idleCpu{};
static HWND checkerWindow{},probeForeground{};
static UINT originalState{},createdMessage{};
static HANDLE guardProcess{};
static std::unique_ptr<Scene> scene;
static std::unique_ptr<Drawing> drawing;
static std::unique_ptr<Dashboard> dashboard;
static Reveal reveal;
static Motion motion;
static Position horizontal;
static MONITORINFO monitor{sizeof(monitor)};
static float dpiScale=1;
static bool lastMouse=false,transparent=true;
static RECT hostRect{};
static DWORD failure=0;

static LRESULT CALLBACK Keyboard(int code,WPARAM message,LPARAM data) {
    if(code==HC_ACTION){
        auto const& key=*reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
        if(!(key.flags&LLKHF_INJECTED)){
            const bool down=message==WM_KEYDOWN||message==WM_SYSKEYDOWN;
            const bool win=GetAsyncKeyState(VK_LWIN)<0||GetAsyncKeyState(VK_RWIN)<0;
            if(key.vkCode=='T'){
                if(swallowedT){if(!down)swallowedT=false;return 1;}
                if(down&&win&&GetAsyncKeyState(VK_CONTROL)>=0&&GetAsyncKeyState(VK_MENU)>=0){
                    if(!swallowedT){
                        swallowedT=true;
                        // Mask the Win-only release before it can reach Explorer.
                        // Exactly two bounded input records; all UI work is posted.
                        INPUT mask[2]{};for(auto& i:mask){i.type=INPUT_KEYBOARD;i.ki.wVk=0xE8;}
                        mask[1].ki.dwFlags=KEYEVENTF_KEYUP;SendInput(2,mask,sizeof(INPUT));
                        PostMessageW(uiWindow,ToggleMessage,0,0);
                    }return 1;
                }
                if(!down&&swallowedT){swallowedT=false;return 1;}
            }
            if(key.vkCode==VK_ESCAPE){
                if(swallowedEscape){if(!down)swallowedEscape=false;return 1;}
                if(down&&keyboardPinned&&!(dashboard&&(dashboard->MenuOpen()||dashboard->StartHolding()))){
                    swallowedEscape=true;PostMessageW(uiWindow,EscapeMessage,0,0);return 1;}
                if(!down&&swallowedEscape){swallowedEscape=false;return 1;}
            }
        }
    }return CallNextHookEx(keyboardHook,code,message,data);
}
static void CALLBACK TaskbarEvent(HWINEVENTHOOK,DWORD,HWND w,LONG object,LONG child,DWORD,DWORD) {
    if(stopping||probe||!w||object!=OBJID_WINDOW||child!=CHILDID_SELF)return;
    wchar_t cls[96]{};GetClassNameW(w,cls,96);
    if(!wcscmp(cls,L"Shell_TrayWnd")||!wcscmp(cls,L"Shell_SecondaryTrayWnd"))
        PostMessageW(uiWindow,TaskbarMessage,WPARAM(w),0);
}
static Rect CapsuleRect() {
    const auto f=motion.At(GetTickCount64());
    const double right=hostRect.left+(horizontal.At(GetTickCount64())+motion.full)*dpiScale;
    const double top=hostRect.top+(ShadowPad+f.y+Height/2*(1-f.scale))*dpiScale;
    return {right-f.width*f.scale*dpiScale,top,f.width*f.scale*dpiScale,Height*f.scale*dpiScale};
}
static void Placement(bool reflow=false) {
    POINT zero{};auto m=MonitorFromPoint(zero,MONITOR_DEFAULTTOPRIMARY);
    monitor={sizeof(monitor)};GetMonitorInfoW(m,&monitor);
    UINT x=96,y=96;GetDpiForMonitor(m,MDT_EFFECTIVE_DPI,&x,&y);dpiScale=float(x)/96;
    const int width=monitor.rcMonitor.right-monitor.rcMonitor.left;
    const int height=int(std::ceil((ShadowPad+Height+BottomGap)*dpiScale));
    const int left=monitor.rcMonitor.left;
    const int top=monitor.rcMonitor.bottom-height;
    hostRect={left,top,left+width,top+height};
    SetWindowPos(uiWindow,HWND_TOPMOST,left,top,width,height,SWP_NOACTIVATE);
    const double center=std::round((double(width)-motion.full*dpiScale)/2)/dpiScale;
    horizontal.Move(center,GetTickCount64(),reflow&&reveal.visible&&Animations());
    if(scene){
        scene->Scale(dpiScale);
        if(horizontal.enabled)scene->Scalar(L"X",float(center),300,0,Ease);
        else{scene->props.StopAnimation(L"X");scene->props.InsertScalar(L"X",float(center));}
    }
    // Native Start/Quick Settings companions use the fully shown capsule
    // geometry. No native taskbar window or application geometry is modified.
    if(!probe)LeanDockGeometry::Publish(uiWindow,int(std::lround(center*dpiScale)),
        int(std::lround((center+motion.full)*dpiScale)),int(std::lround((Height+BottomGap)*dpiScale)));
    if(!probe)for(auto bar:ExplorerBars()){
        if(MonitorFromWindow(bar,MONITOR_DEFAULTTONEAREST)!=m)continue;
        RECT r{};if(!GetWindowRect(bar,&r))continue;
        if(!SetPropW(bar,L"Capsule.GeometryOwner",reinterpret_cast<HANDLE>(uintptr_t(GetCurrentProcessId()))))continue;
        LeanDockGeometry::Publish(bar,hostRect.left-r.left+int(std::lround(center*dpiScale)),
            hostRect.left-r.left+int(std::lround((center+motion.full)*dpiScale)),int(std::lround((Height+BottomGap)*dpiScale)));
    }
}
static void ChangeState(Change change) {
    keyboardPinned=reveal.pinned;
    if(change==Change::None)return;
    const bool show=change==Change::Show;
    motion.Begin(show,GetTickCount64(),Animations());
    if(show)ShowWithoutActivation(uiWindow);
    auto start=motion.At(GetTickCount64());
    if(scene)scene->Animate(show,motion.enabled,&start);
    if(dashboard)dashboard->Shown(show);
    if(!show&&!motion.enabled)ShowWindow(uiWindow,SW_HIDE);
    SetPropW(uiWindow,L"Capsule.Visible",reinterpret_cast<HANDLE>(uintptr_t(show)));
    Log(show?L"reveal":L"hide");
}
static void Poll() {
    if(guardProcess&&WaitForSingleObject(guardProcess,0)==WAIT_OBJECT_0){
        Log(L"guard-exited");PostMessageW(uiWindow,WM_CLOSE,0,0);return;
    }
    POINT p{};if(!GetCursorPos(&p))return;
    const bool button=GetAsyncKeyState(VK_LBUTTON)<0||GetAsyncKeyState(VK_RBUTTON)<0;
    const bool hit=CapsuleHit(CapsuleRect(),p.x,p.y)&&reveal.visible;
    if(dashboard){
        auto r=CapsuleRect();auto f=motion.At(GetTickCount64());
        dashboard->Geometry({r.x-(motion.full-f.width)*dpiScale*f.scale,r.y,
            motion.full*dpiScale*f.scale,Height*dpiScale*f.scale},monitor);
        dashboard->Poll(p,button&&!lastMouse);
    }
    Input i{GetTickCount64()};
    i.edge=!probe&&p.x>=monitor.rcMonitor.left&&p.x<monitor.rcMonitor.right&&
           p.y>=monitor.rcMonitor.bottom-2&&p.y<monitor.rcMonitor.bottom;
    i.band=p.x>=monitor.rcMonitor.left&&p.x<monitor.rcMonitor.right&&
           p.y>=monitor.rcMonitor.bottom-int(std::ceil(HoldBand*dpiScale))&&p.y<monitor.rcMonitor.bottom;
    i.buttons=button;i.fullscreen=!reveal.visible&&i.edge&&!button&&Fullscreen();
    i.detail=dashboard&&dashboard->DetailHold(p);
    i.menu=dashboard&&(dashboard->MenuOpen()||dashboard->TrayHold()||dashboard->StartHolding());
    i.outsidePress=button&&!lastMouse&&!hit&&!(dashboard&&(dashboard->AnyHit(p)||dashboard->StartHolding()));lastMouse=button;
    ChangeState(reveal.Tick(i));
    if(motion.Hidden(i.now)&&IsWindowVisible(uiWindow))ShowWindow(uiWindow,SW_HIDE);
    const bool nextTransparent=!hit;
    if(nextTransparent!=transparent){
        transparent=nextTransparent;
        LONG_PTR ex=GetWindowLongPtrW(uiWindow,GWL_EXSTYLE);
        SetWindowLongPtrW(uiWindow,GWL_EXSTYLE,transparent?ex|WS_EX_TRANSPARENT:ex&~WS_EX_TRANSPARENT);
    }
}
static LRESULT CALLBACK WindowProc(HWND w,UINT m,WPARAM a,LPARAM b) {
    try{
        if(dashboard&&dashboard->Shell(m,a))return 0;
        if(m==createdMessage&&!probe){HideTaskbar(originalState);Placement();if(dashboard)dashboard->Recreated();Log(L"explorer-recreated");return 0;}
        switch(m){
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT p;BeginPaint(w,&p);EndPaint(w,&p);return 0;}
        case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
        case WM_CAPTURECHANGED:case WM_CANCELMODE:if(dashboard)dashboard->CancelInput();return 0;
        case WM_NCHITTEST:{
            POINT p{GET_X_LPARAM(b),GET_Y_LPARAM(b)};
            return CapsuleHit(CapsuleRect(),p.x,p.y)?HTCLIENT:HTTRANSPARENT;
        }
        case WM_TIMER:
            if(a==1)Poll();
            if(a==2&&dashboard){
                if(drawing&&FAILED(drawing->d3d->GetDeviceRemovedReason()))
                    winrt::throw_hresult(drawing->d3d->GetDeviceRemovedReason());
                dashboard->Tick();
            }
            if(a==10&&idleProbe){
                KillTimer(w,10);
                FILETIME created{},ended{},kernel{},user{};
                GetProcessTimes(GetCurrentProcess(),&created,&ended,&kernel,&user);
                const auto elapsed=GetTickCount64()-idleStarted;
                const double cpu=double(Ticks(kernel)+Ticks(user)-idleCpu)/10000/elapsed*100;
                PROCESS_MEMORY_COUNTERS_EX mem{sizeof(mem)};
                GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem),sizeof(mem));
                FILE* f=_wfopen((Directory()+L"\\idle-probe-latest.json").c_str(),L"w");
                if(f){fprintf(f,"{\"durationMs\":%llu,\"cpuOneCorePercent\":%.4f,\"cpuMachinePercent\":%.4f,"
                    "\"privateBytes\":%llu,\"workingSetBytes\":%llu,\"gdiHandles\":%lu,\"userHandles\":%lu,"
                    "\"scope\":\"hidden composition, 20ms pointer policy and 1s CPU sampling; native tray interception omitted\"}\n",
                    elapsed,cpu,cpu/std::max<DWORD>(1,GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)),
                    (unsigned long long)mem.PrivateUsage,(unsigned long long)mem.WorkingSetSize,
                    GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS),GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS));fclose(f);}
                PostMessageW(w,WM_CLOSE,0,0);
            }
            if(a==3&&dashboard){KillTimer(w,3);dashboard->RefreshApps();}
            if(a==9&&visualProbe){
                KillTimer(w,9);
                if(!IsWindowVisible(uiWindow)||!IsWindowVisible(checkerWindow))
                    winrt::throw_hresult(E_FAIL);
                auto r=CapsuleRect();r.x-=20;r.y-=24;r.w+=40;r.h+=40;
                if(visualStage==0){
                    SaveOwnFrame(*drawing,r,Directory()+L"\\visual-probe-latest.png");
                    SetPropW(checkerWindow,L"Capsule.FixtureWarm",HANDLE(1));
                    InvalidateRect(checkerWindow,nullptr,FALSE);
                    ++visualStage;SetTimer(w,9,700,nullptr);
                }else if(visualStage==1){
                    SaveOwnFrame(*drawing,r,Directory()+L"\\visual-probe-warm-latest.png");
                    auto fixtureMonitor=monitor;fixtureMonitor.rcMonitor.bottom=hostRect.bottom;
                    dashboard->ProbeDetails(CapsuleRect(),fixtureMonitor);
                    ++visualStage;SetTimer(w,9,2300,nullptr);
                }else{
                    dashboard->PaintDetail();
                    auto d=dashboard->DetailRect();
                    const auto bottom=r.y+r.h;
                    r.y=d.y-24;r.h=bottom-r.y;
                    SaveOwnFrame(*drawing,r,Directory()+L"\\detail-probe-latest.png");
                    bool foreground=GetForegroundWindow()==probeForeground;
                    Log(L"visual-probe-foreground-preserved",foreground);
                    if(!foreground)failure=ERROR_INVALID_STATE;
                    PostMessageW(w,WM_CLOSE,0,0);
                }
            }
            return 0;
        case WM_APP+28:case WM_APP+29:if(dashboard)dashboard->TrayChanged();return 0;
        case WM_LBUTTONDOWN:case WM_LBUTTONUP:case WM_RBUTTONUP:{
            if(dashboard){
                POINT p{GET_X_LPARAM(b),GET_Y_LPARAM(b)};ClientToScreen(w,&p);
                auto r=CapsuleRect();auto f=motion.At(GetTickCount64());
                float x=float((p.x-r.x)/(dpiScale*f.scale)+motion.full-f.width);
                float y=float((p.y-r.y)/(dpiScale*f.scale));
                dashboard->Mouse(m,x,y);
            }return 0;
        }
        case ToggleMessage:ChangeState(reveal.Toggle());return 0;
        case EscapeMessage:ChangeState(reveal.Hide());return 0;
        case TaskbarMessage:
            if(!stopping&&!probe){
                auto bars=ExplorerBars();auto target=reinterpret_cast<HWND>(a);
                if(std::find(bars.begin(),bars.end(),target)!=bars.end())ShowWindowAsync(target,SW_HIDE);
            }return 0;
        case WM_DISPLAYCHANGE:case WM_DPICHANGED:Placement();if(dashboard)dashboard->DPI(dpiScale);return 0;
        case WM_SETTINGCHANGE:
            if(b&&!wcscmp(reinterpret_cast<LPCWSTR>(b),L"ImmersiveColorSet")){
                if(scene)scene->Theme();if(dashboard)dashboard->Theme();
            }return 0;
        case WM_WTSSESSION_CHANGE:
            if(a==WTS_SESSION_LOCK)ChangeState(reveal.Hide());
            if(a==WTS_SESSION_UNLOCK){Placement();if(!probe)HideTaskbar(originalState);}
            return 0;
        case WM_DEVICECHANGE:if(dashboard)dashboard->DevicesChanged();return 0;
        case WM_POWERBROADCAST:
            if(a==PBT_POWERSETTINGCHANGE&&dashboard)dashboard->PowerChanged();
            if(a==PBT_APMRESUMEAUTOMATIC){Placement();if(scene)scene->Theme();if(dashboard)dashboard->DevicesChanged();if(!probe)HideTaskbar(originalState);}
            return TRUE;
        case WM_QUERYENDSESSION:return TRUE;
        case WM_ENDSESSION:if(!a)return 0;[[fallthrough]];
        case WM_CLOSE:
            stopping=true;for(UINT_PTR t:{1,2,3,9,10})KillTimer(w,t);
            RemovePropW(w,L"Capsule.Ready");
            if(keyboardHook){UnhookWindowsHookEx(keyboardHook);keyboardHook=nullptr;}
            if(taskbarHook){UnhookWinEvent(taskbarHook);taskbarHook=nullptr;}
            WTSUnRegisterSessionNotification(w);
            dashboard.reset();
            if(!probe)RestoreTaskbar(originalState);
            ShowWindow(w,SW_HIDE);drawing.reset();if(scene)scene->Close();
            if(checkerWindow){DestroyWindow(checkerWindow);checkerWindow=nullptr;}
            DestroyWindow(w);return 0;
        case WM_DESTROY:PostQuitMessage(failure);return 0;
        }
    }catch(winrt::hresult_error const& e){failure=DWORD(e.code().value);Log(L"ui-error",failure);PostMessageW(w,WM_CLOSE,0,0);}
    catch(...){failure=E_FAIL;Log(L"ui-error",failure);PostMessageW(w,WM_CLOSE,0,0);}
    return DefWindowProcW(w,m,a,b);
}
static int UserInterface(DWORD owner,bool test) {
    probe=test;SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    WNDCLASSW c{};c.hInstance=GetModuleHandleW(nullptr);c.lpfnWndProc=WindowProc;
    c.lpszClassName=WindowClass;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    RegisterClassW(&c);createdMessage=RegisterWindowMessageW(L"TaskbarCreated");
    uiWindow=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE|
        WS_EX_NOREDIRECTIONBITMAP|WS_EX_LAYERED|WS_EX_TRANSPARENT,WindowClass,L"LeanDock",
        WS_POPUP,0,0,464,84,nullptr,nullptr,c.hInstance,nullptr);
    if(!uiWindow){Log(L"window-failed",GetLastError());return 10;}
    SetLayeredWindowAttributes(uiWindow,0,255,LWA_ALPHA);
    try{
        scene=std::make_unique<Scene>();scene->Init(uiWindow,1);Placement();
        drawing=std::make_unique<Drawing>();drawing->Init(scene->compositor);
        dashboard=std::make_unique<Dashboard>(*scene,*drawing);
        dashboard->Init(uiWindow,dpiScale,monitor,[&](float width,float base){
            const bool changed=motion.full!=width;
            motion.Resize(width,base,GetTickCount64());
            scene->Dimensions(width,base);if(changed)Placement(true);
            if(changed&&reveal.visible&&Animations())scene->Scalar(L"Width",width,300,0,Ease);
            else scene->props.InsertScalar(L"Width",reveal.visible?width:base);
        },[]{ChangeState(reveal.Hide());},test);
        SetPropW(uiWindow,L"Capsule.CompositionReady",HANDLE(1));
        Log(L"host-backdrop-created");
        if(test){
            if(visualProbe){
                probeForeground=GetForegroundWindow();
                // Move our fixed host, once, to the middle of our private fixture.
                const int middle=(monitor.rcMonitor.top+monitor.rcMonitor.bottom)/2;
                int delta=middle-hostRect.top;
                hostRect.top+=delta;hostRect.bottom+=delta;
                RECT backdrop=hostRect;backdrop.top-=int(440*dpiScale);backdrop.bottom+=40;
                checkerWindow=Checker(backdrop);if(!checkerWindow)winrt::throw_last_error();
                SetWindowPos(uiWindow,HWND_TOPMOST,hostRect.left,hostRect.top,
                    hostRect.right-hostRect.left,hostRect.bottom-hostRect.top,SWP_NOACTIVATE);
                motion.Begin(true,GetTickCount64(),true);scene->Animate(true,true);
                dashboard->Shown(true);ShowWithoutActivation(uiWindow);
                SetTimer(uiWindow,9,1800,nullptr);
            }else if(idleProbe){
                dashboard->ProbeIdle();
                FILETIME created{},ended{},kernel{},user{};
                GetProcessTimes(GetCurrentProcess(),&created,&ended,&kernel,&user);
                idleCpu=Ticks(kernel)+Ticks(user);idleStarted=GetTickCount64();
                SetTimer(uiWindow,1,20,nullptr);SetTimer(uiWindow,2,1000,nullptr);SetTimer(uiWindow,10,30000,nullptr);
            }else{
                if(iconProbe)dashboard->ProbeIcons();
                if(timeKeyProbe)VerifyTimeKey(*drawing);
                if(controlPanelProbe)RenderControlPanel(*drawing);
                scene->Animate(true,true);scene->Theme();scene->Animate(false,false);
                // API smoke test: this HWND is NEVER made visible.
                PostMessageW(uiWindow,WM_CLOSE,0,0);
            }
        }else{
            guardProcess=OpenProcess(SYNCHRONIZE,FALSE,owner);
            if(!guardProcess)winrt::throw_last_error();
            keyboardHook=SetWindowsHookExW(WH_KEYBOARD_LL,Keyboard,c.hInstance,0);
            if(!keyboardHook)winrt::throw_last_error();
            taskbarHook=SetWinEventHook(EVENT_OBJECT_SHOW,EVENT_OBJECT_SHOW,nullptr,TaskbarEvent,0,0,
                                      WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
            if(!taskbarHook)winrt::throw_last_error();
            WTSRegisterSessionNotification(uiWindow,NOTIFY_FOR_THIS_SESSION);
            if(!SetTimer(uiWindow,1,20,nullptr))winrt::throw_last_error();
            if(!SetTimer(uiWindow,2,1000,nullptr))winrt::throw_last_error();
            HideTaskbar(originalState);
            if(!dashboard->StartTray()){Log(L"tray-unavailable");winrt::throw_hresult(E_FAIL);}
            SetPropW(uiWindow,L"Capsule.NormalUser",HANDLE(uintptr_t(!Elevated())));
            SetPropW(uiWindow,L"Capsule.Ready",HANDLE(1));Log(L"ready");
        }
    }catch(winrt::hresult_error const& e){
        failure=DWORD(e.code().value);Log(L"initialization-failed",failure);
        PostMessageW(uiWindow,WM_CLOSE,0,0);
    }
    MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    scene.reset();if(guardProcess)CloseHandle(guardProcess);
    winrt::uninit_apartment();return failure?11:0;
}
static bool guardStopping=false;
static DWORD childPid{};
static LRESULT CALLBACK GuardProc(HWND w,UINT m,WPARAM a,LPARAM b) {
    if(m==WM_QUERYENDSESSION)return TRUE;
    if(m==WM_CLOSE||(m==WM_ENDSESSION&&a)){
        guardStopping=true;auto child=FindOwnedWindow(WindowClass,childPid);
        if(child)PostMessageW(child,WM_CLOSE,0,0);
        return 0;
    }return DefWindowProcW(w,m,a,b);
}
static int Guard(bool fixture=false,DWORD companion=0) {
    HANDLE mutex=CreateMutexW(nullptr,FALSE,fixture?L"Local\\NativeTaskbarDock.Capsule.Fixture":L"Local\\NativeTaskbarDock.Capsule.v1");
    if(!mutex||GetLastError()==ERROR_ALREADY_EXISTS){if(mutex)CloseHandle(mutex);return 0;}
    HANDLE companionProcess=nullptr;
    if(!fixture){
        HWND controller=FindWindowW(L"NativeTaskbarDock.Controller.v1",nullptr);
        if(controller){
            DWORD pid=0;GetWindowThreadProcessId(controller,&pid);
            const auto expected=(std::filesystem::path(Directory()).parent_path()/L"latest.exe").wstring();
            if(!companion||pid!=companion||!ExactProcess(controller,expected.c_str())||
               GetPropW(controller,L"LeanDock.Companion.v1")!=reinterpret_cast<HANDLE>(uintptr_t(pid))){
                Log(L"incompatible-native-controller");CloseHandle(mutex);return 20;
            }
            companionProcess=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
            if(!companionProcess){CloseHandle(mutex);return 20;}
        }else if(companion){CloseHandle(mutex);return 20;}
    }
    auto journal=Directory()+L"\\recovery-latest.ini";
    if(!fixture&&GetPrivateProfileIntW(L"restore",L"active",0,journal.c_str())){
        RestoreTaskbar(GetPrivateProfileIntW(L"restore",L"state",ABS_ALWAYSONTOP,journal.c_str()));
        Journal(false,0);
    }
    if(!fixture){originalState=AppBarState();Journal(true,originalState);}
    unsigned restores=0,starts=0;
    auto restore=[&]{++restores;if(!fixture)RestoreTaskbar(originalState);};
    WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=GuardProc;
    wc.lpszClassName=fixture?L"Capsule.GuardFixture":GuardClass;
    RegisterClassW(&wc);guardWindow=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,
        wc.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
    if(!guardWindow){restore();if(!fixture)Journal(false,originalState);if(companionProcess)CloseHandle(companionProcess);CloseHandle(mutex);return 23;}
    RestartBudget budget;int result=0;
    for(;;){
        auto p=Spawn(fixture?L"--guard-fixture-child":
            L"--ui "+std::to_wstring(GetCurrentProcessId())+L" "+std::to_wstring(originalState));
        if(!p.hProcess){result=21;break;}childPid=p.dwProcessId;
        ++starts;
        Log(L"child-started",childPid);
        uint64_t stopAt=0,started=GetTickCount64();bool ready=false;
        for(;;){
            DWORD r=MsgWaitForMultipleObjects(1,&p.hProcess,FALSE,1000,QS_ALLINPUT);
            if(r==WAIT_OBJECT_0)break;
            MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
            if(companionProcess&&WaitForSingleObject(companionProcess,0)!=WAIT_TIMEOUT)guardStopping=true;
            if(!ready){
                auto w=FindOwnedWindow(WindowClass,childPid);ready=w&&GetPropW(w,L"Capsule.Ready");
                if(!ready&&GetTickCount64()-started>15000){
                    Log(L"child-ready-timeout");TerminateProcess(p.hProcess,24);WaitForSingleObject(p.hProcess,1000);break;
                }
            }
            if(guardStopping){
                if(!stopAt)stopAt=GetTickCount64();
                if(auto w=FindOwnedWindow(WindowClass,childPid))PostMessageW(w,WM_CLOSE,0,0);
                if(GetTickCount64()-stopAt>=5000){TerminateProcess(p.hProcess,2);WaitForSingleObject(p.hProcess,1000);break;}
            }
        }
        DWORD code=1;GetExitCodeProcess(p.hProcess,&code);CloseHandle(p.hProcess);
        restore();Log(L"child-exited",code);
        if(guardStopping||code==0)break;
        if(!budget.Take(GetTickCount64())){result=22;Log(L"restart-budget-exhausted");break;}
    }
    restore();if(!fixture)Journal(false,originalState);
    DestroyWindow(guardWindow);if(companionProcess)CloseHandle(companionProcess);CloseHandle(mutex);
    if(fixture){
        FILE* f=_wfopen((Directory()+L"\\guard-probe-latest.json").c_str(),L"w");
        if(f){fprintf(f,"{\"childStarts\":%u,\"restoreBranches\":%u,\"exitCode\":%d,"
            "\"actualTaskbarMutations\":false}\n",starts,restores,result);fclose(f);}
        return starts==4&&restores==5&&result==22?0:1;
    }
    return result;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    std::wstring mode=argc>1?argv[1]:L"";
    DWORD owner=argc>2?wcstoul(argv[2],nullptr,10):0;
    originalState=argc>3?wcstoul(argv[3],nullptr,10):0;
    LocalFree(argv);
    if(mode==L"--token-probe"){
        HANDLE t=nullptr;bool normal=OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&t)&&LeanDockCompanion::NormalToken(t);
        if(t)CloseHandle(t);
        FILE* f=_wfopen((Directory()+L"\\token-probe-latest.json").c_str(),L"w");
        if(f){fprintf(f,"{\"normalMediumUser\":%s,\"pid\":%lu,\"visibleWindows\":false}\n",normal?"true":"false",GetCurrentProcessId());fclose(f);}
        return normal?0:5;
    }
    if(mode==L"--protocol-tests")return ProtocolTests();
    if(mode==L"--controls-probe")return ControlsProbe();
    if(mode==L"--native-tray-probe")return NativeTrayProbe();
    if(mode==L"--guard-fixture-child"){Sleep(100);return 86;}
    if(mode==L"--guard-probe")return Guard(true);
    if(mode==L"--apps-probe"){
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        Apps apps;bool ok=apps.Refresh();
        FILE* f=_wfopen((Directory()+L"\\apps-probe-latest.txt").c_str(),L"w, ccs=UTF-8");
        if(f){
            fwprintf(f,L"ok=%d hr=%08lx module=%ls count=%zu\n",ok,apps.pinResult,apps.pinModule.c_str(),apps.list.size());
            for(auto const& a:apps.list)fwprintf(f,L"pinned=%d windows=%zu name=%ls key=%ls parse=%ls exe=%ls\n",
                a.pinned,a.windows.size(),a.name.c_str(),a.key.c_str(),a.parse.c_str(),a.exe.c_str());
            fclose(f);
        }return ok?0:1;
    }
    if(mode==L"--data-probe"){
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        SYSTEMTIME date{2026,10,0,20,12,0,0,0};
        auto lunar=Lunar(date);
        Metrics metrics;metrics.Start(nullptr);metrics.Visible(true);Sleep(2300);
        auto data=metrics.Read();metrics.Visible(false);Sleep(1100);auto hidden=metrics.Read();metrics.Stop();
        Power power;power.Read();
        FILE* f=_wfopen((Directory()+L"\\data-probe-latest.txt").c_str(),L"w, ccs=UTF-8");
        if(f){fwprintf(f,L"lunar=%ls cpu=%d memory=%d capacity=%ls gpu=%d name=%ls network=%ls disks=%ls volume=%d battery=%ls percent=%ls reads=%llu hiddenReads=%llu\n",
            lunar.c_str(),data.cpu,data.memory,data.capacity.c_str(),data.gpu,data.gpuName.c_str(),
            data.network.c_str(),data.disks.c_str(),data.volume,data.battery.c_str(),power.Percent().c_str(),data.detailReads,hidden.detailReads);fclose(f);}
        return lunar==L"农历九月十一"&&data.cpu>=0&&data.memory>=0&&data.detailReads==hidden.detailReads?0:1;
    }
    if(mode==L"--pin-roundtrip"){
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        Apps apps;if(!apps.Refresh())return 30;
        const auto path=Directory()+L"\\capsule-pin-fixture.lnk";
        winrt::com_ptr<IShellLinkW> link;
        winrt::check_hresult(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IShellLinkW),link.put_void()));
        link->SetPath(Executable().c_str());link->SetArguments(L"--probe");link->SetDescription(L"LeanDock isolated pin test");
        // Use a plain Win32 link. An invented, unregistered AUMID would not
        // be a representative pin target for the native shell.
        winrt::check_hresult(link.as<IPersistFile>()->Save(path.c_str(),TRUE));
        ITEMIDLIST* raw=nullptr;winrt::check_hresult(SHParseDisplayName(path.c_str(),nullptr,&raw,0,nullptr));
        auto item=OwnPidl(raw);
        HRESULT add=apps.pins->Modify(nullptr,item.get(),4);
        HRESULT present=apps.pins->IsPinned(item.get());
        const auto deadline=GetTickCount64()+3000;
        while(present==S_FALSE&&GetTickCount64()<deadline){
            MsgWaitForMultipleObjectsEx(0,nullptr,50,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
            present=apps.pins->IsPinned(item.get());
        }
        HRESULT remove=present==S_OK?apps.pins->Modify(item.get(),nullptr,4):S_FALSE;
        HRESULT absent=apps.pins->IsPinned(item.get());
        FILE* f=_wfopen((Directory()+L"\\pin-roundtrip-latest.txt").c_str(),L"w");
        if(f){fwprintf(f,L"add=%08lx present=%08lx remove=%08lx absent=%08lx\n",add,present,remove,absent);fclose(f);}
        return SUCCEEDED(add)&&present==S_OK&&SUCCEEDED(remove)&&absent==S_FALSE?0:31;
    }
    if(mode==L"--probe")return UserInterface(0,true);
    if(mode==L"--icons-probe"){iconProbe=true;return UserInterface(0,true);}
    if(mode==L"--time-key-probe"){timeKeyProbe=true;return UserInterface(0,true);}
    if(mode==L"--control-panel-probe"){controlPanelProbe=true;return UserInterface(0,true);}
    if(mode==L"--visual-probe"){
        if(!CanInspectOwnVisibleSurface()){Log(L"visual-probe-skipped-inactive-desktop");return ERROR_REQUIRES_INTERACTIVE_WINDOWSTATION;}
        visualProbe=true;return UserInterface(0,true);
    }
    if(mode==L"--idle-probe"){idleProbe=true;return UserInterface(0,true);}
    if(mode==L"--status"){
        HWND guard=FindWindowW(GuardClass,nullptr),window=FindWindowW(WindowClass,nullptr);
        const auto exe=Executable();
        const bool ownsGuard=guard&&ExactProcess(guard,exe.c_str()),ownsUI=window&&ExactProcess(window,exe.c_str());
        DWORD gp=0,up=0;if(ownsGuard)GetWindowThreadProcessId(guard,&gp);if(ownsUI)GetWindowThreadProcessId(window,&up);
        bool ready=ownsUI&&GetPropW(window,L"Capsule.Ready"),trayReady=ownsUI&&GetPropW(window,L"Capsule.TrayReady");
        FILE* f=_wfopen((Directory()+L"\\latest-status.json").c_str(),L"w");
        if(f){fprintf(f,"{\"schema\":1,\"architecture\":\"capsule\",\"guardPid\":%lu,\"uiPid\":%lu,"
            "\"ready\":%s,\"trayReady\":%s,\"normalUser\":%s,\"pinWrites\":false}\n",gp,up,ready?"true":"false",
            trayReady?"true":"false",ownsUI&&GetPropW(window,L"Capsule.NormalUser")?"true":"false");fclose(f);}
        return ownsGuard&&ready&&trayReady?0:1;
    }
    if(mode==L"--stop"){
        auto w=FindWindowW(GuardClass,nullptr);
        if(w&&ExactProcess(w,Executable().c_str()))PostMessageW(w,WM_CLOSE,0,0);return 0;
    }
    if(Elevated()){Log(L"refused-elevated-ui");return 5;}
    if(mode==L"--ui"&&!owner)return ERROR_INVALID_PARAMETER;
    if(mode==L"--companion-guard")return owner?Guard(false,owner):ERROR_INVALID_PARAMETER;
    if(!mode.empty()&&mode!=L"--ui")return ERROR_INVALID_PARAMETER;
    return mode==L"--ui"?UserInterface(owner,false):Guard();
}
