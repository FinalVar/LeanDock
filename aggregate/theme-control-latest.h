#pragma once
#include "platform-latest.h"

// The old QuickTheme tile delegates to the retained controller's --theme
// worker. Keep that isolation, implementation pin, full-theme application,
// mutex, rollback and completion broadcast instead of reimplementing them in
// this UI. No CloudStore/Night Light write belongs to this control.
namespace Capsule::ThemeControl {
inline constexpr wchar_t Personalize[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
// Controller 0.3.34 (0.8.90): the accepted 0.8.78 controller plus fullscreen apps; the theme worker is unchanged.
inline constexpr wchar_t ControllerHash[]=L"CD380B5E13FCA59F249F9B2720885394352D129B947796ABDEDC7913A06641E4";
inline constexpr wchar_t ImplementationHash[]=L"2AA99876194E8EE59D806A92DD740348279B6F1B704B4AE5A4E6BDB89552CD51";
inline constexpr wchar_t MutexName[]=L"Local\\LeanDock.ThemeSwitch.v1";
inline int Classify(DWORD apps,DWORD system){
    if(apps>1||system>1)return -1;
    if(apps!=system)return 2;
    return apps==0?1:0;
}
inline int Read(){
    DWORD apps=2,system=2,n=sizeof(DWORD);
    if(RegGetValueW(HKEY_CURRENT_USER,Personalize,L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&apps,&n)!=ERROR_SUCCESS)return -1;
    n=sizeof(DWORD);
    if(RegGetValueW(HKEY_CURRENT_USER,Personalize,L"SystemUsesLightTheme",RRF_RT_REG_DWORD,nullptr,&system,&n)!=ERROR_SUCCESS)return -1;
    return Classify(apps,system);
}
inline std::wstring Label(int state){return state==2?L"混合配色":state==1?L"已开启":state==0?L"已关闭":L"不可用";}
inline std::wstring ControllerPath(){return (std::filesystem::path(Directory()).parent_path()/L"latest.exe").wstring();}
inline bool Available(std::wstring const& controller){
    wchar_t system[MAX_PATH]{},windows[MAX_PATH]{};
    if(!GetSystemDirectoryW(system,MAX_PATH)||!GetWindowsDirectoryW(windows,MAX_PATH))return false;
    if(Sha256(controller)!=ControllerHash||Sha256(std::wstring(system)+L"\\themeui.dll")!=ImplementationHash)return false;
    for(auto relative:{L"\\Resources\\Themes\\dark.theme",L"\\Resources\\Themes\\aero.theme",
                       L"\\Web\\Wallpaper\\Windows\\img19.jpg",L"\\Web\\Wallpaper\\Windows\\img0.jpg"})
        if(GetFileAttributesW((std::wstring(windows)+relative).c_str())==INVALID_FILE_ATTRIBUTES)return false;
    return true;
}
inline bool OtherWorker(){
    HANDLE mutex=OpenMutexW(SYNCHRONIZE,FALSE,MutexName);
    if(mutex){CloseHandle(mutex);return true;}
    return GetLastError()==ERROR_ACCESS_DENIED;
}
inline bool Verified(DWORD exitCode,int state,int target){return exitCode==0&&state==target;}
struct Completion {DWORD code{};int actual=-1;bool success=false;};
class Worker {
    HANDLE process{};uint64_t at=0;int target=-1;
public:
    bool available=false;
    Worker()=default;Worker(Worker const&)=delete;Worker& operator=(Worker const&)=delete;
    void Refresh(){available=Available(ControllerPath());}
    bool Pending()const{return process!=nullptr;}
    DWORD Start(bool dark){
        if(Pending()||OtherWorker())return ERROR_BUSY;
        Refresh();if(!available||Elevated())return ERROR_NOT_SUPPORTED;
        auto path=ControllerPath();std::wstring command=L"\""+path+L"\" --theme "+(dark?L"dark":L"light");
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION created{};
        startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
        if(!CreateProcessW(path.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,
                           nullptr,std::filesystem::path(path).parent_path().c_str(),&startup,&created))return GetLastError();
        CloseHandle(created.hThread);process=created.hProcess;target=dark?1:0;at=GetTickCount64();return ERROR_SUCCESS;
    }
    bool Poll(Completion& result){
        if(!process)return false;
        auto wait=WaitForSingleObject(process,0);
        if(wait==WAIT_TIMEOUT&&GetTickCount64()-at<30000)return false;
        result.code=ERROR_GEN_FAILURE;
        if(wait==WAIT_OBJECT_0){if(!GetExitCodeProcess(process,&result.code))result.code=GetLastError();}
        else if(wait==WAIT_TIMEOUT)result.code=WAIT_TIMEOUT;
        else result.code=GetLastError();
        result.actual=Read();result.success=Verified(result.code,result.actual,target);
        CloseHandle(process);process=nullptr;target=-1;return true;
    }
    // Hiding the popup does not cancel a requested system theme transition.
    // Unload/timeout drops only our wait; do not kill the old worker mid-apply.
    // Its named mutex continues to prevent overlapping theme requests.
    ~Worker(){if(process)CloseHandle(process);}
};
}
