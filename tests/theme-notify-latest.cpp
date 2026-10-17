// Real WM_SETTINGCHANGE delivery to test-owned HIDDEN primary/secondary
// taskbar stand-ins and a non-taskbar hidden listener on a private, never
// displayed desktop. Exercises the real HWND_BROADCAST without notifying the
// user's shell/apps, changing settings, switching desktops or sending input.
#include "../include/theme-notify-latest.h"
#include <iostream>
#include <stdexcept>
static unsigned checks,primaryCalls,secondaryCalls,otherCalls;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM w,LPARAM l){
    if(message==WM_SETTINGCHANGE){
        if(w||!l||wcscmp(reinterpret_cast<const wchar_t*>(l),L"ImmersiveColorSet"))return 0;
        wchar_t name[64]{};GetClassNameW(window,name,64);
        if(!wcscmp(name,L"Shell_TrayWnd"))++primaryCalls;
        else if(!wcscmp(name,L"Shell_SecondaryTrayWnd"))++secondaryCalls;
        else ++otherCalls;
    }
    return DefWindowProcW(window,message,w,l);
}
int main(){
    HWND windows[3]{};const wchar_t* names[]={L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd",L"LeanDock.ThemeOther"};
    auto foreground=GetForegroundWindow();
    const auto originalDesktop=GetThreadDesktop(GetCurrentThreadId());
    HDESK fixtureDesktop=nullptr;
    int code=0;
    try{
        wchar_t desktopName[80]{};
        swprintf_s(desktopName,L"LeanDockThemeTest-%lu",GetCurrentProcessId());
        fixtureDesktop=CreateDesktopW(desktopName,nullptr,nullptr,0,
            DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS,nullptr);
        Check(fixtureDesktop&&SetThreadDesktop(fixtureDesktop),"private desktop, never switched to input");
        for(unsigned i=0;i<3;++i){
            WNDCLASSW c{};c.hInstance=GetModuleHandleW(nullptr);c.lpfnWndProc=WindowProc;c.lpszClassName=names[i];
            Check(RegisterClassW(&c)!=0,"register owned class");
            windows[i]=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,names[i],L"",WS_OVERLAPPED,-32000,-32000,10,10,nullptr,nullptr,c.hInstance,nullptr);
            Check(windows[i]&&!IsWindowVisible(windows[i]),"created invisible fixture");
        }
        auto empty=LeanDockTheme::NotifyTaskbarsForProcess(0);
        Check(empty.matched==0&&primaryCalls==0&&secondaryCalls==0,"unknown owner sends nothing");
        auto wrong=LeanDockTheme::NotifyTaskbarsForProcess(MAXDWORD);
        Check(wrong.matched==0&&primaryCalls==0&&secondaryCalls==0,"wrong process excluded");
        for(unsigned i=1;i<=3;++i){
            auto result=LeanDockTheme::NotifyTaskbarsForProcess(GetCurrentProcessId());
            Check(result.matched==2&&result.delivered==2&&result.failed==0,"both hidden shell classes notified");
            Check(primaryCalls==i&&secondaryCalls==i&&otherCalls==0,"only owned taskbars received mode notification");
        }
        Check(LeanDockTheme::BroadcastAppearance(),"native completion broadcast succeeds");
        Check(primaryCalls==4&&secondaryCalls==4&&otherCalls==1,"completion includes hidden non-taskbar listener");
        Check(LeanDockTheme::BroadcastAppearance(),"repeated native completion broadcast succeeds");
        Check(primaryCalls==5&&secondaryCalls==5&&otherCalls==2,"every completion reaches all listeners");
        DestroyWindow(windows[1]);windows[1]=nullptr;
        auto gone=LeanDockTheme::NotifyTaskbarsForProcess(GetCurrentProcessId());
        Check(gone.matched==1&&gone.delivered==1&&primaryCalls==6&&secondaryCalls==5,"destroyed secondary ignored");
        Check(LeanDockTheme::BroadcastAppearance()&&primaryCalls==7&&secondaryCalls==5&&otherCalls==3,"broadcast after listener destruction");
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';code=1;}
    for(unsigned i=0;i<3;++i){if(windows[i])DestroyWindow(windows[i]);UnregisterClassW(names[i],GetModuleHandleW(nullptr));}
    if(fixtureDesktop){
        if(!SetThreadDesktop(originalDesktop)||!CloseDesktop(fixtureDesktop)){std::cerr<<"private desktop cleanup failed\n";code=1;}
    }
    if(foreground!=GetForegroundWindow()){std::cerr<<"foreground changed during fixture\n";code=1;}
    else ++checks;
    if(!code)std::cout<<checks<<" checks passed: native hidden-window delivery and full completion broadcast on an undisplayed test desktop\n";
    return code;
}
