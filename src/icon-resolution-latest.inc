// Local addition to the retained taskbar-icon-size module. This method has
// an MSVC member-function hidden result pointer (verified in pinned code).
namespace IconResolution {
using GetSize = SIZE*(WINAPI*)(void*,SIZE*);
GetSize original{};
std::uintptr_t moduleBase{};
std::atomic<unsigned long> counts[5]{};
constexpr const wchar_t* countKeys[]={L"LeanDock.IconLoader.Cache",L"LeanDock.IconLoader.Win32",L"LeanDock.IconLoader.Packaged",L"LeanDock.IconLoader.Placeholder",L"LeanDock.IconLoader.Website"};
SIZE* WINAPI Hook(void* self,SIZE* size) {
    struct Scope { Scope(){++g_hookCallCounter;} ~Scope(){--g_hookCallCounter;} } scope;
    const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
    auto result=original(self,size);
    if(result!=size||!size)return result;
    const auto kind=caller>=moduleBase?LeanDockIconResolution::SourceKind(caller-moduleBase):-1;
    const auto before=size->cx;
    const auto budget=LeanDockIconResolution::SourceDips(before,size->cy,g_unloading,kind);
    if(budget!=before){
        size->cx=size->cy=budget;
        const auto count=++counts[kind];
        if(HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr)){
            SetPropW(taskbar,countKeys[kind],reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(count)+1));
            SetPropW(taskbar,L"LeanDock.IconLoader.DisplayDips",reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(before)+1));
            SetPropW(taskbar,L"LeanDock.IconLoader.SourceDips",reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(budget)+1));
        }
    }
    return result;
}
void Clear() noexcept {
    if(HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr)){
        for(auto key:countKeys)RemovePropW(taskbar,key);
        RemovePropW(taskbar,L"LeanDock.IconLoader.DisplayDips");
        RemovePropW(taskbar,L"LeanDock.IconLoader.SourceDips");
    }
    for(auto& count:counts)count=0;
}
}
