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
// ShellIconLoaderV2::s_ForceImagePresent gets the icon from a shared image list
// after resizing it for the request. Past the native size that is the 256 px
// list, which for some icons holds only the native picture in a 256 px cell.
// Those icons take the native list's copy; the rest keep their 256 px source.
using ForceImage=long(WINAPI*)(int,int,HICON*);
ForceImage forceOriginal{};
std::atomic<unsigned long> nativeFallbacks{};
enum class Artwork{Unknown,Undersized,Fine};
Artwork Inspect(HICON icon) noexcept {
    Artwork result=Artwork::Unknown;
    ICONINFO info{};
    if(!icon||!GetIconInfo(icon,&info))return result;
    BITMAP bitmap{};
    if(info.hbmColor&&GetObjectW(info.hbmColor,sizeof bitmap,&bitmap)&&bitmap.bmBitsPixel==32&&
       bitmap.bmWidth>0&&bitmap.bmWidth<=1024&&bitmap.bmHeight>0&&bitmap.bmHeight<=1024) {
        try {
            std::string pixels(std::size_t(bitmap.bmWidth)*bitmap.bmHeight*4,'\0');
            BITMAPINFO format{};
            format.bmiHeader.biSize=sizeof format.bmiHeader;format.bmiHeader.biWidth=bitmap.bmWidth;
            format.bmiHeader.biHeight=-bitmap.bmHeight;format.bmiHeader.biPlanes=1;
            format.bmiHeader.biBitCount=32;format.bmiHeader.biCompression=BI_RGB;
            if(HDC dc=CreateCompatibleDC(nullptr)) {
                if(GetDIBits(dc,info.hbmColor,0,bitmap.bmHeight,pixels.data(),&format,DIB_RGB_COLORS)==bitmap.bmHeight) {
                    std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(pixels.data()),pixels.size());
                    const bool any=std::any_of(pixels.begin(),pixels.end(),[](char c){return c!=0;});
                    result=!any?Artwork::Unknown:LeanDockIconResolution::UndersizedArtwork(bytes,bitmap.bmWidth,bitmap.bmHeight)?Artwork::Undersized:Artwork::Fine;
                }
                DeleteDC(dc);
            }
        }catch(...){}
    }
    if(info.hbmColor)DeleteObject(info.hbmColor);
    if(info.hbmMask)DeleteObject(info.hbmMask);
    return result;
}
HICON NativeListIcon(int index) noexcept {
    // Read only: resizing the shared list here could race another load.
    struct ImageList{void* unused0[2];ULONG(WINAPI* Release)(void*);void* unused1[7];HRESULT(WINAPI* GetIcon)(void*,int,UINT,HICON*);};
    using GetList=HRESULT(WINAPI*)(int,REFIID,void**);
    static const auto get=reinterpret_cast<GetList>(GetProcAddress(GetModuleHandleW(L"shell32.dll"),MAKEINTRESOURCEA(727)));
    static constexpr GUID iid{0x46EB5926,0x582E,0x4017,{0x9F,0xDF,0xE8,0x99,0x8D,0xAA,0x09,0x50}}; // IImageList
    void* list{};HICON icon{};
    if(get&&SUCCEEDED(get(2,iid,&list))&&list) { // SHIL_EXTRALARGE
        auto const& calls=**reinterpret_cast<ImageList**>(list);
        if(FAILED(calls.GetIcon(list,index,ILD_TRANSPARENT,&icon)))icon=nullptr;
        calls.Release(list);
    }
    return icon;
}
long WINAPI ForceHook(int index,int size,HICON* icon) {
    struct Scope { Scope(){++g_hookCallCounter;} ~Scope(){--g_hookCallCounter;} } scope;
    const long result=forceOriginal(index,size,icon);
    if(result<0||!icon||!*icon||g_unloading||Inspect(*icon)!=Artwork::Undersized)return result;
    if(HICON native=NativeListIcon(index)) {
        if(Inspect(native)!=Artwork::Fine){DestroyIcon(native);return result;}
        DestroyIcon(*icon);*icon=native;
        const auto count=++nativeFallbacks;
        if(HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr))
            SetPropW(taskbar,L"LeanDock.IconLoader.NativeFallback",reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(count)+1));
    }
    return result;
}
void Clear() noexcept {
    if(HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr)){
        for(auto key:countKeys)RemovePropW(taskbar,key);
        RemovePropW(taskbar,L"LeanDock.IconLoader.DisplayDips");
        RemovePropW(taskbar,L"LeanDock.IconLoader.SourceDips");
        RemovePropW(taskbar,L"LeanDock.IconLoader.NativeFallback");
    }
    for(auto& count:counts)count=0;
    nativeFallbacks=0;
}
}
