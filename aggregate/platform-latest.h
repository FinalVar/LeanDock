#pragma once
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <psapi.h>
#include <wtsapi32.h>
#include <winrt/base.h>
#include <string>
#include <vector>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <bcrypt.h>
#include "policy-latest.h"
namespace Capsule {
inline std::wstring Sha256(std::wstring const& path) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file==INVALID_HANDLE_VALUE)return {};
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    DWORD length=0,returned=0;std::wstring result;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&
       BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&returned,0)>=0){
        std::vector<BYTE> object(length);
        if(BCryptCreateHash(algorithm,&hash,object.data(),length,nullptr,0,0)>=0){
            BYTE bytes[32768];DWORD n=0;bool ok=true;
            while(ok){
                if(!ReadFile(file,bytes,sizeof(bytes),&n,nullptr)){ok=false;break;}
                if(!n)break;
                ok=BCryptHashData(hash,bytes,n,0)>=0;
            }
            BYTE digest[32]{};
            if(ok&&BCryptFinishHash(hash,digest,sizeof(digest),0)>=0){
                for(auto c:digest){wchar_t b[3];swprintf_s(b,L"%02X",c);result+=b;}
            }
            BCryptDestroyHash(hash);
        }
    }
    if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);CloseHandle(file);return result;
}
inline std::wstring Directory() {
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);
    return std::filesystem::path(path).parent_path().wstring();
}
inline std::wstring Executable() {
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);return path;
}
inline void Log(wchar_t const* event,long code=0) {
    auto path=Directory()+L"\\latest.log";
    FILE* f=_wfopen(path.c_str(),L"a");if(!f)return;
    SYSTEMTIME t{};GetLocalTime(&t);
    fwprintf(f,L"%04u-%02u-%02u %02u:%02u:%02u pid=%lu %ls code=%ld\n",
        t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,GetCurrentProcessId(),event,code);
    fclose(f);
}
inline bool Elevated() {
    HANDLE t=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&t))return true;
    TOKEN_ELEVATION value{};DWORD bytes{};
    bool ok=GetTokenInformation(t,TokenElevation,&value,sizeof(value),&bytes);
    CloseHandle(t);return !ok||value.TokenIsElevated;
}
inline bool ExactProcess(HWND w,wchar_t const* expected) {
    DWORD pid{};GetWindowThreadProcessId(w,&pid);
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return false;
    wchar_t path[32768]{};DWORD size=32768;
    bool ok=QueryFullProcessImageNameW(p,0,path,&size);CloseHandle(p);
    return ok&&_wcsicmp(path,expected)==0;
}
inline void ShowWithoutActivation(HWND window) {
    // A background launcher can set STARTF_USESHOWWINDOW / SW_HIDE. The first
    // ShowWindow call would obey that value instead of our explicit reveal.
    // This changes only our own window visibility and never its focus.
    if(!SetWindowPos(window,HWND_TOPMOST,0,0,0,0,
        SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW))winrt::throw_last_error();
}
inline bool Animations() {
    BOOL enabled=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0);return enabled;
}
}
