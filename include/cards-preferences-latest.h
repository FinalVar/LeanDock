#pragma once
#include <windows.h>
#include "cards-core-latest.h"

namespace LeanDockCards {
// User data is created on demand outside the packaged runtime manifest. An
// update/rollback must not replace it. Windows owns INI per-key serialization;
// saving order never overwrites a concurrently remembered selection.
inline std::wstring PreferencesPath(std::wstring const& root){return root+L"\\cards\\latest.ini";}
inline bool PreferencesDirectory(std::wstring const& root){
    const auto path=root+L"\\cards";
    return CreateDirectoryW(path.c_str(),nullptr)||GetLastError()==ERROR_ALREADY_EXISTS;
}
inline bool ReadPreferences(std::wstring const& root,PageOrder& order,Page& selected) {
    const auto path=PreferencesPath(root);wchar_t buffer[256]{};
    const auto n=GetPrivateProfileStringW(L"cards",L"Order",L"clock,power,media,metrics,network",buffer,256,path.c_str());
    if(n>=255)return false;
    auto parsed=ParseOrder(buffer);if(!parsed)return false;
    GetPrivateProfileStringW(L"cards",L"LastPage",L"clock",buffer,256,path.c_str());
    selected=parsed->Select(ParsePage(buffer).value_or(parsed->pages.front()));order=std::move(*parsed);return true;
}
inline bool SaveOrder(std::wstring const& root,PageOrder const& order) {
    auto valid=ParseOrder(order.Text());if(!valid||!PreferencesDirectory(root))return false;
    return WritePrivateProfileStringW(L"cards",L"Order",valid->Text().c_str(),PreferencesPath(root).c_str())!=FALSE;
}
inline bool SaveSelection(std::wstring const& root,Page page) {
    if(!*PageKey(page)||!PreferencesDirectory(root))return false;
    return WritePrivateProfileStringW(L"cards",L"LastPage",PageKey(page),PreferencesPath(root).c_str())!=FALSE;
}
inline UINT PreferencesMessage(){static UINT value=RegisterWindowMessageW(L"LeanDock.Cards.Changed.v1");return value;}
inline void NotifyPreferences() {
    EnumWindows([](HWND window,LPARAM)->BOOL{
        wchar_t cls[64]{};GetClassNameW(window,cls,64);
        if(wcscmp(cls,L"Shell_TrayWnd")&&wcscmp(cls,L"Shell_SecondaryTrayWnd"))return TRUE;
        DWORD pid=0,session=0,ownSession=0;GetWindowThreadProcessId(window,&pid);
        if(ProcessIdToSessionId(pid,&session)&&ProcessIdToSessionId(GetCurrentProcessId(),&ownSession)&&session==ownSession)
            PostMessageW(window,PreferencesMessage(),0,0);
        return TRUE;
    },0);
}
}
