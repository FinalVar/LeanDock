#pragma once
#include <windows.h>
#include <shellapi.h>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace LeanDockAppBar {
inline constexpr wchar_t ControllerReady[]=L"LeanDock.ClientEdge.v2.Ready";
inline constexpr wchar_t HookReady[]=L"LeanDock.AppBar.v1.Ready";
inline constexpr wchar_t Queries[]=L"LeanDock.AppBar.v1.Queries";
inline constexpr wchar_t CacheWrites[]=L"LeanDock.AppBar.v1.CacheWrites";
inline UINT RefreshMessage(){static UINT message=RegisterWindowMessageW(L"LeanDock.AppBar.v1.Refresh");return message;}
// Explorer EEE089FE...8EE11: matched Microsoft PDB and SHAppBarMessage's
// actual serializer on an unshown private desktop. Fixed wire layout, NOT
// native APPBARDATA (whose HWND and alignment differ on x64).
struct WireRequest {
    std::uint32_t size,window,callback,edge;
    RECT rectangle;
    std::int64_t parameter;
    std::uint32_t message,padding;
    std::uint64_t shared;
    // Receiver PID for shared memory, NOT the caller identity.
    std::uint32_t sharedProcess,padding2;
};
static_assert(sizeof(WireRequest)==64 && offsetof(WireRequest,message)==40 &&
              offsetof(WireRequest,sharedProcess)==56);
inline bool ReadRequest(const COPYDATASTRUCT* data,WireRequest& request) {
    if(!data || data->dwData!=0 || data->cbData!=sizeof(request) || !data->lpData)return false;
    std::memcpy(&request,data->lpData,sizeof(request));
    return request.size==40 && request.sharedProcess!=0;
}
inline ULONG_PTR Filter(const WireRequest& request,ULONG_PTR nativeResult,
                        bool ready,bool nativeCaller,
                        bool primaryOwned,bool returnedBarOwned) {
    if(!ready || nativeCaller)return nativeResult;
    if(request.message==ABM_GETSTATE && primaryOwned)return nativeResult&~ULONG_PTR(ABS_AUTOHIDE);
    if((request.message==ABM_GETAUTOHIDEBAR || request.message==ABM_GETAUTOHIDEBAREX) &&
       request.edge==ABE_BOTTOM && returnedBarOwned)return 0;
    return nativeResult;
}
inline bool BottomCacheKey(const wchar_t* key) {
    if(!key || reinterpret_cast<uintptr_t>(key)<=0xffff)return false;
    // Verified shell32 C55A5F...341F6: WindowOnEdge:%08x:%1u.
    if(wcsncmp(key,L"WindowOnEdge:",13))return false;
    for(unsigned i=13;i<21;++i)if(!((key[i]>=L'0'&&key[i]<=L'9') ||
        (key[i]>=L'a'&&key[i]<=L'f') || (key[i]>=L'A'&&key[i]<=L'F')))return false;
    return key[21]==L':' && key[22]==L'3' && key[23]==0;
}
inline bool NativeWireQuery(HWND tray,HWND caller,UINT query,UINT edge,
                            const RECT& monitor,ULONG_PTR& result) {
    if(query!=ABM_GETSTATE && query!=ABM_GETAUTOHIDEBAR && query!=ABM_GETAUTOHIDEBAREX)return false;
    DWORD receiver=0;GetWindowThreadProcessId(tray,&receiver);if(!receiver || !caller)return false;
    WireRequest data{40};data.window=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(caller));
    data.edge=edge;data.rectangle=monitor;data.message=query;data.sharedProcess=receiver;
    COPYDATASTRUCT copy{0,sizeof(data),&data};DWORD_PTR value=0;
    if(!SendMessageTimeoutW(tray,WM_COPYDATA,reinterpret_cast<WPARAM>(caller),
        reinterpret_cast<LPARAM>(&copy),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&value))return false;
    result=value;return true;
}
inline bool FullMaximizedClient(LONG_PTR style,const RECT& client,const RECT& monitor) {
    constexpr LONG_PTR required=WS_MAXIMIZE|WS_THICKFRAME|WS_CAPTION|WS_MAXIMIZEBOX;
    // Caption and sizing styles disappear for the true-fullscreen path in
    // the inspected Tao and Chromium implementations. Never resize the app.
    return (style&required)==required && !(style&(WS_CHILD|WS_POPUP)) &&
        monitor.right>monitor.left && monitor.bottom>monitor.top &&
        EqualRect(&client,&monitor);
}
}
