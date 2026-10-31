#pragma once
#include <windows.h>
#include <shellapi.h>
#include <cwchar>
#include <iterator>

// Which fullscreen windows keep the auto-hide Dock and the hot corners away.
// Windows hides an auto-hide taskbar behind any fullscreen window, and the
// upstream hot corners paused for all of them. LeanDock does so only for
// games, so players, browsers, terminals and other applications keep both.
// A game is exclusive Direct3D fullscreen, a program installed in a game
// store's library, or a file name listed in hotcorners/latest.ini Games=.
namespace LeanDockFullscreen {
inline constexpr const wchar_t* Libraries[]={
    L"\\steamapps\\common\\",L"\\Epic Games\\",L"\\XboxGames\\",L"\\Riot Games\\",L"\\WeGameApps\\",
    L"\\HoYoPlay\\games\\",L"\\GOG Galaxy\\Games\\",L"\\GOG Games\\",L"\\Ubisoft Game Launcher\\games\\",L"\\EA Games\\"};
// Games= from the hot-corner settings: file names separated by ';'.
inline wchar_t listed[1024];
inline bool Contains(const wchar_t* text,const wchar_t* part) {
    const size_t n=wcslen(text),m=wcslen(part);
    for(size_t i=0;m && i+m<=n;++i)if(!_wcsnicmp(text+i,part,m))return true;
    return false;
}
inline bool Listed(const wchar_t* image,const wchar_t* list) {
    if(!image || !list)return false;
    const wchar_t* slash=wcsrchr(image,L'\\');const wchar_t* name=slash?slash+1:image;
    const size_t length=wcslen(name);
    for(const wchar_t* p=list;*p;) {
        while(*p==L';' || *p==L' ')++p;
        const wchar_t* end=p;while(*end && *end!=L';')++end;
        const wchar_t* last=end;while(last>p && last[-1]==L' ')--last;
        if(length && size_t(last-p)==length && !_wcsnicmp(p,name,length))return true;
        p=end;
    }
    return false;
}
// image is null when the process cannot be identified (protected or gone);
// that counts as a game, so an anti-cheat-protected title stays undisturbed.
inline bool Game(const wchar_t* image,const wchar_t* list,bool exclusive) {
    if(exclusive || !image)return true;
    for(auto library:Libraries)if(Contains(image,library))return true;
    return Listed(image,list);
}
inline bool Game(HWND w) {
    QUERY_USER_NOTIFICATION_STATE state{};
    const bool exclusive=SUCCEEDED(SHQueryUserNotificationState(&state)) && state==QUNS_RUNNING_D3D_FULL_SCREEN;
    DWORD pid=0;GetWindowThreadProcessId(w,&pid);
    wchar_t image[1024]{};DWORD size=std::size(image);bool known=false;
    if(HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid)) {
        known=QueryFullProcessImageNameW(p,0,image,&size)!=FALSE;CloseHandle(p);
    }
    return Game(known?image:nullptr,listed,exclusive);
}
// No caption and the frame covers the whole monitor: the shape Windows'
// fullscreen detection and the hot corners' fullscreen rule both look for.
inline bool Covers(LONG_PTR style,const RECT& frame,const RECT& monitor) {
    return (style&WS_CAPTION)!=WS_CAPTION && monitor.right>monitor.left && monitor.bottom>monitor.top &&
        frame.left<=monitor.left && frame.top<=monitor.top && frame.right>=monitor.right && frame.bottom>=monitor.bottom;
}
}
