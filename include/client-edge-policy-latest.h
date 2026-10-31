#pragma once
#include <windows.h>
#include <appmodel.h>
#include <cwchar>
#include <iterator>

// Public shell contract, not an Explorer ABI hook. This deliberately excludes
// non-maximized and non-resizable/fullscreen windows. The shared package
// identity remains narrow; generic edge repair requires separate live evidence.
namespace LeanDockClientEdgePolicy {
inline bool Eligible(bool visible,bool minimized,bool maximized,LONG_PTR style,
                     const RECT& window,const RECT& monitor) {
    return visible && !minimized && maximized && (style&WS_THICKFRAME) &&
        monitor.right>monitor.left && monitor.bottom>monitor.top &&
        window.left<=monitor.left && window.top<=monitor.top &&
        window.right>=monitor.right && window.bottom>=monitor.bottom;
}
inline bool Identity(const wchar_t* family,const wchar_t* image) {
    if(!family || !image || _wcsicmp(family,L"OpenAI.Codex_2p2nqsd0c76g0"))return false;
    constexpr wchar_t suffix[]=L"\\app\\ChatGPT.exe";
    const size_t n=wcslen(image),m=std::size(suffix)-1;
    return n>m && !_wcsicmp(image+n-m,suffix);
}
inline bool BottomResize(LRESULT hit) {
    return hit==HTBOTTOM || hit==HTBOTTOMLEFT || hit==HTBOTTOMRIGHT;
}
inline bool BorderInset(LONG_PTR exStyle,const RECT& frame,const RECT& monitor,int dpi) {
    // A visible inset plus a native resize hit distinguishes an invisible
    // sizing border from genuine full-screen content. Unknown data is a no-op.
    const auto gap=monitor.bottom-frame.bottom;
    return !(exStyle&(WS_EX_TOPMOST|WS_EX_TOOLWINDOW)) &&
        frame.right>frame.left && frame.bottom>frame.top && dpi>=48 && dpi<=768 &&
        gap>0 && gap<=MulDiv(16,dpi,96);
}
inline bool ResizeObstruction(bool ownsEdge,LRESULT hit,LONG_PTR exStyle,
                              const RECT& frame,const RECT& monitor,int dpi) {
    return ownsEdge && BottomResize(hit) && BorderInset(exStyle,frame,monitor,dpi);
}
// Shared read-only identity check for the shell edge and hot-corner policies.
// Do not infer identity from a caption, a file name alone, or NonRudeHWND.
inline bool ClientWindow(HWND w) {
    wchar_t cls[96]{};GetClassNameW(w,cls,std::size(cls));
    if(wcscmp(cls,L"Chrome_WidgetWin_1"))return false;
    DWORD pid=0;GetWindowThreadProcessId(w,&pid);
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return false;
    wchar_t family[128]{},image[32768]{};UINT32 familySize=std::size(family);DWORD imageSize=std::size(image);
    const bool ok=GetPackageFamilyName(p,&familySize,family)==ERROR_SUCCESS &&
        QueryFullProcessImageNameW(p,0,image,&imageSize) && Identity(family,image);
    CloseHandle(p);return ok;
}
inline constexpr wchar_t NonRude[]=L"NonRudeHWND";
inline constexpr wchar_t Owner[]=L"LeanDock.ClientEdge.v1.Owner";
// Set while LeanDock holds MarkFullscreenWindow(TRUE) on a fullscreen
// application, so a later controller can take over or clear a mark left by
// one that ended without cleanup.
inline constexpr wchar_t Held[]=L"LeanDock.ClientEdge.v1.Fullscreen";
// The owner token makes a stale HWND or a replacement property a no-op on
// cleanup. An existing NonRudeHWND property belongs to its previous writer.
inline bool Release(HWND window,HANDLE token) {
    if(!window || GetPropW(window,Owner)!=token)return false;
    const bool ours=GetPropW(window,NonRude)==reinterpret_cast<HANDLE>(1);
    if(ours)RemovePropW(window,NonRude);
    RemovePropW(window,Owner);
    return ours;
}
inline bool Acquire(HWND window,HANDLE token) {
    if(!window || !token || GetPropW(window,NonRude) || GetPropW(window,Owner))return false;
    if(!SetPropW(window,Owner,token))return false;
    if(SetPropW(window,NonRude,reinterpret_cast<HANDLE>(1)))return true;
    RemovePropW(window,Owner);return false;
}
}
