// Compile the real production Panel, Popup and Metrics into an isolated fixture.
// Never call Show/Run with the real taskbar, send input, or issue device writes.
#define wWinMain AggregateProductionEntry
#include "../aggregate/latest.cpp"
#undef wWinMain
#include <stdexcept>

namespace {
using namespace Capsule;
unsigned checks=0,closeCycles=0,workerCycles=0;
uint64_t maximumStop=0;
std::string failure;
std::vector<std::pair<std::string,DWORD>> handleSamples;
void Handles(std::string name){DWORD count=0;if(GetProcessHandleCount(GetCurrentProcess(),&count))handleSamples.emplace_back(std::move(name),count);}
void Check(bool ok,const char* reason){++checks;if(!ok){failure=reason;throw std::runtime_error(reason);}}
HWND Hidden(const wchar_t* cls=L"STATIC",void* context=nullptr){
    auto window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,cls,L"LeanDock isolated lifecycle fixture",
        WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),context);
    Check(window&&!IsWindowVisible(window),"fixture window must remain hidden");return window;
}
void Publish(HWND bar,uintptr_t cookie,HWND popup,HWND control){
    Check(SetPropW(bar,Bridge::CookieProp,HANDLE(cookie)),"publish fixture cookie");
    Check(SetPropW(bar,Bridge::PidProp,HANDLE(uintptr_t(GetCurrentProcessId()))),"publish fixture pid");
    Check(SetPropW(bar,Bridge::WindowProp,popup),"publish fixture popup");
    Check(SetPropW(bar,Bridge::ControlProp,control),"publish fixture control");
    Check(SetPropW(bar,Bridge::PendingProp,HANDLE(GetTickCount64()+5000)),"publish fixture deadline");
}
void Attach(Panel& panel,HWND bar,uintptr_t cookie){
    panel.bar=bar;panel.cookie=cookie;
    panel.owner=OpenProcess(SYNCHRONIZE,FALSE,GetCurrentProcessId());
    Check(panel.owner!=nullptr,"fixture process handle");
}
unsigned OwnPanelWindows(){
    unsigned count=0;EnumWindows([](HWND w,LPARAM p)->BOOL{
        DWORD pid=0;GetWindowThreadProcessId(w,&pid);if(pid!=GetCurrentProcessId())return TRUE;
        wchar_t cls[96]{};GetClassNameW(w,cls,96);
        if(!wcscmp(cls,Bridge::ControlClass)||!wcscmp(cls,Bridge::PopupClass))++*reinterpret_cast<unsigned*>(p);
        return TRUE;
    },LPARAM(&count));return count;
}
void PanelCases(){
    {Panel panel;panel.Close();panel.Close();Check(!panel.control&&!panel.popup.window,"close before initialization");}
    WNDCLASSW cls{};cls.lpfnWndProc=Panel::Proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=Bridge::ControlClass;
    Check(RegisterClassW(&cls)||GetLastError()==ERROR_CLASS_ALREADY_EXISTS,"fixture control registration");
    auto bar=Hidden();auto replacement=Hidden();
    {Panel panel;Check(panel.Run(bar,0)==20,"reject untrusted owner before initialization");}
    // Full/partial initialization, stale cookie, stale PID, destroyed owner HWND.
    for(unsigned mode=0;mode<5;++mode){
        const uintptr_t token=100+mode;
        Panel panel;Attach(panel,bar,token);
        panel.control=Hidden(Bridge::ControlClass,&panel);
        if(mode!=1)panel.popup.Init(panel.draw,1,PanelWidth,408,14,false);
        auto popup=panel.popup.window;auto control=panel.control;
        Publish(bar,token,popup?popup:control,control);
        Check(panel.Valid(),"matching owner tuple accepted");
        if(mode==2)Publish(bar,token+1000,replacement,replacement);
        if(mode==3){Publish(bar,token,replacement,replacement);SetPropW(bar,Bridge::PidProp,HANDLE(uintptr_t(GetCurrentProcessId()+1)));}
        if(mode==4){DestroyWindow(bar);bar=nullptr;}
        if(mode>=2)Check(!panel.Valid(),"late instance rejects changed owner");
        Check(!popup||!IsWindowVisible(popup),"initialized popup is hidden");
        panel.Close();panel.Close();++closeCycles;
        Check(!IsWindow(control)&&(!popup||!IsWindow(popup)),"owned windows destroyed");
        Check(!panel.control&&!panel.popup.window,"close clears owned HWNDs");
        if(mode<2){
            Check(!GetPropW(bar,Bridge::WindowProp)&&!GetPropW(bar,Bridge::ControlProp)&&!GetPropW(bar,Bridge::PendingProp),"close removes own published windows");
            Check(GetPropW(bar,Bridge::CookieProp)==HANDLE(token)&&GetPropW(bar,Bridge::PidProp)==HANDLE(uintptr_t(GetCurrentProcessId())),"parent-owned identity retained");
        }else if(bar){
            Check(GetPropW(bar,Bridge::WindowProp)==replacement&&GetPropW(bar,Bridge::ControlProp)==replacement&&GetPropW(bar,Bridge::PendingProp),"late cleanup preserves successor properties");
        }
    }
    if(bar)DestroyWindow(bar);DestroyWindow(replacement);
    Check(OwnPanelWindows()==0,"no owned panel/control HWND remains");
}
void QuitCases(){
    // Deliver notifications only to our own hidden fixture HWNDs. This does not
    // lock/suspend Windows, restart Explorer, or prove resume behavior.
    const std::pair<UINT,WPARAM> events[]={{WM_CLOSE,0},{WM_WTSSESSION_CHANGE,WTS_SESSION_LOCK},
        {WM_POWERBROADCAST,PBT_APMSUSPEND},{WM_TIMER,1}};
    for(auto [message,value]:events){
        Panel panel;panel.control=Hidden(Bridge::ControlClass,&panel);
        panel.popup.Init(panel.draw,1,PanelWidth,408,14,false);
        auto control=panel.control;auto popup=panel.popup.window;
        Log(L"fixture-before-notification",message);Check(!panel.closing,"fixture closed before notification");
        SendMessageW(control,message,value,0);Check(panel.closing,"notification requests shutdown");
        MSG quit{};bool sawQuit=false;const auto deadline=GetTickCount64()+2000;
        while(!sawQuit&&GetTickCount64()<deadline){
            while(PeekMessageW(&quit,nullptr,0,0,PM_REMOVE)){
                if(quit.message==WM_QUIT){sawQuit=true;break;}
                TranslateMessage(&quit);DispatchMessageW(&quit);
            }
            if(!sawQuit)Sleep(1);
        }
        Check(sawQuit,"notification terminates own message loop");
        panel.Close();Check(!IsWindow(control)&&!IsWindow(popup),"notification cleanup destroys own HWNDs");
        Check(OwnPanelWindows()==0,"notification cleanup has no panel HWND leftovers");++closeCycles;
    }
}
void Stop(Metrics& metrics){
    auto start=GetTickCount64();metrics.Stop();maximumStop=std::max(maximumStop,GetTickCount64()-start);
    metrics.Stop();++workerCycles;
    Check(metrics.Read().writes==0,"worker never wrote a device");
    Check(maximumStop<3000,"bounded normal-machine worker shutdown");
}
void WorkerCases(){
    // Every cycle uses the same one-shot contract as a production panel process.
    for(unsigned i=0;i<16;++i){{Metrics metrics;metrics.Start(nullptr);if(i%2)metrics.Visible(true);Stop(metrics);}Handles("immediate-"+std::to_string(i));}
    for(unsigned i=0;i<4;++i){{Metrics metrics;metrics.Start(nullptr);metrics.Visible(true);Sleep(5);metrics.Visible(false);Stop(metrics);}Handles("racing-stop-"+std::to_string(i));}
    {Metrics metrics;metrics.Start(nullptr);Sleep(150);metrics.RefreshControls();Sleep(150);
        auto s=metrics.Read();Check(!s.reads&&!s.detailReads&&!s.diskReads&&!s.at,"hidden worker does not sample devices");
        Check(metrics.Set(Control::Volume,1)==0&&metrics.Set(Control::Brightness,1)==0,"hidden controls reject writes before enqueue");Stop(metrics);}
    {Metrics metrics;metrics.Start(nullptr);metrics.Visible(true);const auto deadline=GetTickCount64()+8000;
        Sample s;do{Sleep(100);s=metrics.Read();}while(s.reads<3&&GetTickCount64()<deadline);
        Check(s.reads>=3&&s.history.size()>=2,"live read-only worker publishes real samples");
        metrics.Visible(false);Sleep(400);auto first=metrics.Read();metrics.RefreshControls();Sleep(1300);auto second=metrics.Read();
        Check(first.at==second.at&&first.reads==second.reads&&first.detailReads==second.detailReads&&first.diskReads==second.diskReads&&first.history.size()==second.history.size(),"hidden refresh does not restart sampling");
        Stop(metrics);}
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);
    const bool allowed=count==2&&!wcscmp(args[1],L"--hidden-probe");LocalFree(args);if(!allowed)return 2;
    DWORD beforeHandles=0,afterHandles=0;int result=0;bool apartment=false;
    try{
        Check(!Elevated(),"fixture must use ordinary Explorer token");
        winrt::init_apartment(winrt::apartment_type::single_threaded);apartment=true;
        GetProcessHandleCount(GetCurrentProcess(),&beforeHandles);
        Handles("cold");
        for(unsigned cycle=0;cycle<3;++cycle){PanelCases();Handles("panel-batch-"+std::to_string(cycle));}
        QuitCases();Handles("notifications");WorkerCases();Handles("workers-stopped");
        Check(OwnPanelWindows()==0,"all test panel windows released");
        GetProcessHandleCount(GetCurrentProcess(),&afterHandles);
    }catch(...){result=1;if(failure.empty())failure="unexpected fixture exception";}
    if(apartment)winrt::uninit_apartment();
    FILE* f=_wfopen((Directory()+L"\\lifecycle-probe-latest.json").c_str(),L"w");
    if(!f)return 3;
    fprintf(f,"{\"passed\":%s,\"normalUser\":%s,\"checks\":%u,\"closeCases\":%u,\"workerCycles\":%u,\"maximumStopMs\":%llu,\"ownedPanelWindows\":%u,\"processHandlesBefore\":%lu,\"processHandlesAfter\":%lu,\"failure\":\"%s\",\"writesRequested\":false,\"desktopInput\":false,\"realHostRestartTested\":false,\"handleSamples\":[",
        result?"false":"true",Elevated()?"false":"true",checks,closeCycles,workerCycles,maximumStop,OwnPanelWindows(),beforeHandles,afterHandles,failure.c_str());
    for(size_t i=0;i<handleSamples.size();++i)fprintf(f,"%s{\"stage\":\"%s\",\"handles\":%lu}",i?",":"",handleSamples[i].first.c_str(),handleSamples[i].second);
    fprintf(f,"]}\n");
    fclose(f);return result;
}
