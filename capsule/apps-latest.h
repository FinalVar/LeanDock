#pragma once
#include "platform-latest.h"
#include <winrt/base.h>
#include <propkey.h>
#include <propvarutil.h>
#include <tlhelp32.h>
#include <appmodel.h>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace Capsule {
inline std::wstring Lower(std::wstring s) {
    std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return towlower(c);});return s;
}
inline std::wstring ProcessPath(DWORD pid) {
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return {};
    wchar_t s[32768]{};DWORD n=32768;bool ok=QueryFullProcessImageNameW(p,0,s,&n);CloseHandle(p);
    return ok?std::wstring(s,n):L"";
}
inline std::wstring ProcessAppId(DWORD pid) {
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return {};
    UINT32 n=0;GetApplicationUserModelId(p,&n,nullptr);std::wstring id(n,L'\0');
    if(!n||GetApplicationUserModelId(p,&n,id.data())!=ERROR_SUCCESS)id.clear();
    CloseHandle(p);while(!id.empty()&&!id.back())id.pop_back();return id;
}
inline std::wstring Prop(IPropertyStore* store,REFPROPERTYKEY key) {
    if(!store)return {};PROPVARIANT v{};std::wstring result;
    if(SUCCEEDED(store->GetValue(key,&v))&&v.vt==VT_LPWSTR&&v.pwszVal)result=v.pwszVal;
    PropVariantClear(&v);return result;
}
inline std::wstring ItemProp(IShellItem2* item,REFPROPERTYKEY key) {
    PWSTR p=nullptr;std::wstring s;if(item&&SUCCEEDED(item->GetString(key,&p))&&p)s=p;
    CoTaskMemFree(p);return s;
}
inline std::wstring ItemName(IShellItem* item,SIGDN kind) {
    PWSTR p=nullptr;std::wstring s;if(item&&SUCCEEDED(item->GetDisplayName(kind,&p))&&p)s=p;
    CoTaskMemFree(p);return s;
}
// The Taskband COM contract is version-bound. EnumObjects and Modify are
// isolated here; no registry binary parsing, PEB spoofing or Explorer injection.
struct PinnedList : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE EnumObjects(IEnumIDList**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetPinnableInfo(IDataObject*,int,IShellItem**,IShellItem**,PWSTR*,int)=0;
    virtual HRESULT STDMETHODCALLTYPE IsPinnable(IDataObject*,int)=0;
    virtual HRESULT STDMETHODCALLTYPE Resolve(HWND,ULONG,PCIDLIST_ABSOLUTE,PIDLIST_ABSOLUTE*)=0;
    virtual HRESULT STDMETHODCALLTYPE LegacyModify(PCIDLIST_ABSOLUTE,PCIDLIST_ABSOLUTE)=0;
    virtual HRESULT STDMETHODCALLTYPE GetChangeCount(ULONG*)=0;
    virtual HRESULT STDMETHODCALLTYPE IsPinned(PCIDLIST_ABSOLUTE)=0;
    virtual HRESULT STDMETHODCALLTYPE GetPinnedItem(PCIDLIST_ABSOLUTE,PIDLIST_ABSOLUTE*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetAppIDForPinnedItem(PCIDLIST_ABSOLUTE,PWSTR*)=0;
    virtual HRESULT STDMETHODCALLTYPE ItemChangeNotify(PCIDLIST_ABSOLUTE,PCIDLIST_ABSOLUTE)=0;
    virtual HRESULT STDMETHODCALLTYPE UpdateForRemovedItemsAsNecessary()=0;
    virtual HRESULT STDMETHODCALLTYPE PinShellLink(IShellLinkW*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetPinnedItemForAppID(PCWSTR,PIDLIST_ABSOLUTE*)=0;
    virtual HRESULT STDMETHODCALLTYPE Modify(PCIDLIST_ABSOLUTE,PCIDLIST_ABSOLUTE,int)=0;
};
inline constexpr GUID PinnedClsid{0x90AA3A4E,0x1CBA,0x4233,{0xB8,0xBB,0x53,0x57,0x73,0xD4,0x84,0x49}};
inline constexpr GUID PinnedIid{0x0DD79AE2,0xD156,0x45D4,{0x9E,0xEB,0x3B,0x54,0x97,0x69,0xE9,0x40}};
using Pidl=std::shared_ptr<ITEMIDLIST>;
inline Pidl OwnPidl(ITEMIDLIST* p){return Pidl(p,[](ITEMIDLIST* value){CoTaskMemFree(value);});}
struct App {
    std::wstring key,name,exe,aumid,parse,args,working;
    Pidl pidl;
    std::vector<HWND> windows;
    HWND recent{};
    bool pinned=false;
    uint64_t order=0;
};
inline std::wstring Key(std::wstring const& aumid,std::wstring const& exe) {
    return Lower(aumid.empty()?L"exe:"+exe:L"id:"+aumid);
}
inline App FromPidl(ITEMIDLIST* p) {
    App app;app.pidl=OwnPidl(p);app.pinned=true;
    winrt::com_ptr<IShellItem2> item;
    if(FAILED(SHCreateItemFromIDList(p,__uuidof(IShellItem2),item.put_void())))return app;
    app.name=ItemName(item.get(),SIGDN_NORMALDISPLAY);app.parse=ItemName(item.get(),SIGDN_DESKTOPABSOLUTEPARSING);
    app.aumid=ItemProp(item.get(),PKEY_AppUserModel_ID);
    auto path=ItemName(item.get(),SIGDN_FILESYSPATH);
    if(Lower(std::filesystem::path(path).extension().wstring())==L".lnk"){
        winrt::com_ptr<IShellLinkW> link;
        if(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IShellLinkW),link.put_void()))&&
           SUCCEEDED(link.as<IPersistFile>()->Load(path.c_str(),STGM_READ))){
            wchar_t s[32768]{};link->GetPath(s,32768,nullptr,SLGP_RAWPATH);app.exe=s;
            link->GetArguments(s,32768);app.args=s;link->GetWorkingDirectory(s,32768);app.working=s;
            if(app.aumid.empty())app.aumid=Prop(link.as<IPropertyStore>().get(),PKEY_AppUserModel_ID);
        }
    }else if(!path.empty())app.exe=path;
    app.key=Key(app.aumid,app.exe.empty()?app.parse:app.exe);return app;
}
inline bool TaskWindow(HWND w) {
    if(!IsWindowVisible(w))return false;
    DWORD pid{};GetWindowThreadProcessId(w,&pid);if(pid==GetCurrentProcessId())return false;
    LONG_PTR ex=GetWindowLongPtrW(w,GWL_EXSTYLE);
    if(ex&WS_EX_TOOLWINDOW)return false;
    if(GetWindow(w,GW_OWNER)&&!(ex&WS_EX_APPWINDOW))return false;
    DWORD cloaked=0;if(SUCCEEDED(DwmGetWindowAttribute(w,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))&&cloaked)return false;
    wchar_t cls[128]{};GetClassNameW(w,cls,128);
    return wcscmp(cls,L"Progman")&&wcscmp(cls,L"WorkerW")&&wcscmp(cls,L"Shell_TrayWnd")&&
        wcscmp(cls,L"Shell_SecondaryTrayWnd")&&wcscmp(cls,L"Windows.UI.Core.CoreWindow");
}
struct Apps {
    // Windows remains the only owner of pin/order persistence. On the tested
    // OS Modify returned S_OK without changing IsPinned/EnumObjects; disable
    // writes rather than present an unsaved local ordering as a successful pin.
    static constexpr bool CanEditPins=false;
    HKEY pinKey{};
    HANDLE pinChanged{};
    winrt::com_ptr<PinnedList> pins;
    std::vector<App> list;
    std::unordered_map<std::wstring,uint64_t> order;
    std::unordered_map<std::wstring,HWND> recent;
    uint64_t sequence=0;
    HRESULT pinResult=E_PENDING;
    std::wstring pinModule;
    void WatchPins() {
        if(pinKey)return;
        if(RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Taskband",
            0,KEY_NOTIFY,&pinKey)!=ERROR_SUCCESS)return;
        pinChanged=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(!pinChanged||RegNotifyChangeKeyValue(pinKey,TRUE,REG_NOTIFY_CHANGE_LAST_SET,
            pinChanged,TRUE)!=ERROR_SUCCESS)StopWatch();
    }
    void StopWatch() {
        if(pinKey){RegCloseKey(pinKey);pinKey=nullptr;}
        if(pinChanged){CloseHandle(pinChanged);pinChanged=nullptr;}
    }
    bool PinsChanged() {
        if(!pinChanged||WaitForSingleObject(pinChanged,0)!=WAIT_OBJECT_0)return false;
        if(RegNotifyChangeKeyValue(pinKey,TRUE,REG_NOTIFY_CHANGE_LAST_SET,
            pinChanged,TRUE)!=ERROR_SUCCESS)StopWatch();
        return true;
    }
    ~Apps(){StopWatch();}
    bool Init() {
        wchar_t system[MAX_PATH]{};GetSystemDirectoryW(system,MAX_PATH);
        const auto expected=std::wstring(system)+L"\\shell32.dll";
        // Verified on this user's build 26300.9711 / shell32 26100.9549.
        // Future servicing fails closed to the native taskbar, not an unknown ABI.
        if(Sha256(expected)!=L"C55A5F3B6EF059871FE2A0C8D82F086E0998D1154023E3ECF49698B97F9341F6"){
            pinResult=HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH);return false;
        }
        pinResult=CoCreateInstance(PinnedClsid,nullptr,CLSCTX_INPROC_SERVER,PinnedIid,pins.put_void());
        if(FAILED(pinResult))return false;
        HMODULE module{};wchar_t path[32768]{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>((*reinterpret_cast<void***>(pins.get()))[3]),&module);
        GetModuleFileNameW(module,path,32768);pinModule=path;
        if(Lower(pinModule)!=Lower(expected)){pins=nullptr;pinResult=E_NOINTERFACE;return false;}
        return true;
    }
    bool Refresh(HWND foreground=GetForegroundWindow()) {
        std::vector<App> next;
        if(!pins&&!Init())return false;
        winrt::com_ptr<IEnumIDList> enumeration;pinResult=pins->EnumObjects(enumeration.put());
        if(FAILED(pinResult)||!enumeration)return false;
        for(unsigned n=0;n<256;n++){
            ITEMIDLIST* p=nullptr;ULONG fetched=0;
            if(enumeration->Next(1,&p,&fetched)!=S_OK||!p)break;
            auto app=FromPidl(p);
            if(!app.key.empty())next.push_back(std::move(app));
        }
        struct Context{Apps* self;std::vector<App>* result;HWND fg;};
        Context ctx{this,&next,foreground};
        EnumWindows([](HWND w,LPARAM raw)->BOOL{
            if(!TaskWindow(w))return TRUE;
            auto& ctx=*reinterpret_cast<Context*>(raw);
            DWORD pid{};GetWindowThreadProcessId(w,&pid);
            auto exe=ProcessPath(pid);if(exe.empty())return TRUE;
            winrt::com_ptr<IPropertyStore> store;SHGetPropertyStoreForWindow(w,__uuidof(IPropertyStore),store.put_void());
            auto aumid=Prop(store.get(),PKEY_AppUserModel_ID);
            if(aumid.empty())aumid=ProcessAppId(pid);
            // ApplicationFrameHost owns the outer window; the visible child
            // owns the packaged application's identity and executable.
            if(Lower(std::filesystem::path(exe).filename().wstring())==L"applicationframehost.exe"){
                struct Child{DWORD host,app;};Child c{pid,0};
                EnumChildWindows(w,[](HWND child,LPARAM data)->BOOL{
                    auto& c=*reinterpret_cast<Child*>(data);DWORD id{};GetWindowThreadProcessId(child,&id);
                    if(id!=c.host&&IsWindowVisible(child))c.app=id;return TRUE;
                },reinterpret_cast<LPARAM>(&c));
                if(c.app){exe=ProcessPath(c.app);if(aumid.empty())aumid=ProcessAppId(c.app);}
            }
            auto key=Key(aumid,exe);
            auto& out=*ctx.result;
            auto found=std::find_if(out.begin(),out.end(),[&](App const& a){
                return a.key==key||(!exe.empty()&&!a.exe.empty()&&Lower(a.exe)==Lower(exe)&&
                    (a.aumid.empty()||aumid.empty()||Lower(a.aumid)==Lower(aumid)));
            });
            if(found==out.end()){
                App a;a.key=key;a.exe=exe;a.aumid=aumid;
                a.parse=aumid.empty()?exe:L"shell:AppsFolder\\"+aumid;
                winrt::com_ptr<IShellItem2> item;
                if(SUCCEEDED(SHCreateItemFromParsingName(a.parse.c_str(),nullptr,__uuidof(IShellItem2),item.put_void())))
                    a.name=ItemName(item.get(),SIGDN_NORMALDISPLAY);
                if(!item||a.name.empty()||Lower(std::filesystem::path(a.name).extension().wstring())==L".exe"){
                    auto description=ItemProp(item.get(),PKEY_FileDescription);
                    a.name=description.empty()?std::filesystem::path(exe).stem().wstring():description;
                }
                if(!ctx.self->order.contains(key)){
                    FILETIME created{},exit{},kernel{},user{};uint64_t age=0;
                    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
                    if(process){if(GetProcessTimes(process,&created,&exit,&kernel,&user))
                        age=(uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;CloseHandle(process);}
                    ctx.self->order[key]=age?age:UINT64_MAX-(1000000-++ctx.self->sequence);
                }
                a.order=ctx.self->order[key];out.push_back(std::move(a));found=out.end()-1;
            }
            found->windows.push_back(w);
            if(w==ctx.fg)ctx.self->recent[found->key]=w;
            return TRUE;
        },reinterpret_cast<LPARAM>(&ctx));
        auto first=std::find_if(next.begin(),next.end(),[](App const& a){return !a.pinned;});
        std::stable_sort(first,next.end(),[](App const& a,App const& b){return a.order<b.order;});
        for(auto& a:next){
            auto r=recent.find(a.key);
            a.recent=r!=recent.end()&&std::find(a.windows.begin(),a.windows.end(),r->second)!=a.windows.end()?
                r->second:(a.windows.empty()?nullptr:a.windows.front());
        }
        list=std::move(next);return true;
    }
    bool Launch(App const& a,HWND owner) {
        SHELLEXECUTEINFOW s{sizeof(s)};s.hwnd=owner;s.nShow=SW_SHOWNORMAL;
        s.fMask=SEE_MASK_FLAG_NO_UI;
        if(a.pidl){s.fMask|=SEE_MASK_IDLIST;s.lpIDList=a.pidl.get();}
        else{s.lpFile=a.parse.c_str();s.lpParameters=a.args.empty()?nullptr:a.args.c_str();
            s.lpDirectory=a.working.empty()?nullptr:a.working.c_str();}
        return ShellExecuteExW(&s);
    }
    bool Activate(App const& a,HWND owner) {
        if(!a.recent||!IsWindow(a.recent))return Launch(a,owner);
        if(GetForegroundWindow()==a.recent){ShowWindowAsync(a.recent,SW_MINIMIZE);return true;}
        if(IsIconic(a.recent))ShowWindowAsync(a.recent,SW_RESTORE);
        if(!SetForegroundWindow(a.recent))SwitchToThisWindow(a.recent,TRUE);
        return true;
    }
    void CloseWindows(App const& a) {for(auto w:a.windows)if(IsWindow(w))PostMessageW(w,WM_CLOSE,0,0);}
    bool End(App const& a) {
        if(a.exe.empty())return false;
        auto name=Lower(std::filesystem::path(a.exe).filename().wstring());
        if(name==L"explorer.exe"||name==L"applicationframehost.exe"||name==L"svchost.exe"||
           Lower(a.exe)==Lower(Executable()))return false;
        HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snap==INVALID_HANDLE_VALUE)return false;
        PROCESSENTRY32W entry{sizeof(entry)};bool done=false;
        if(Process32FirstW(snap,&entry))do{
            if(entry.th32ProcessID<=4||entry.th32ProcessID==GetCurrentProcessId())continue;
            if(Lower(ProcessPath(entry.th32ProcessID))!=Lower(a.exe))continue;
            HANDLE p=OpenProcess(PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
            if(!p)continue;
            wchar_t actual[32768]{};DWORD size=32768;
            if(QueryFullProcessImageNameW(p,0,actual,&size)&&Lower(actual)==Lower(a.exe))done|=TerminateProcess(p,1)!=0;
            CloseHandle(p);
        }while(Process32NextW(snap,&entry));CloseHandle(snap);return done;
    }
};
}
