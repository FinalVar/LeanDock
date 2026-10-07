#pragma once
#include <windows.h>
#include <cwchar>
#include <iterator>
// Appearance and behavior are the single validated compiled profile. Only
// diagnostics can be configured at runtime; unknown keys fail closed.
static bool NativeReadConfiguration(const wchar_t* path,bool& logging,bool& diagnostics) {
    logging=false;diagnostics=false;
    if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)return false;
    wchar_t sections[256]{};
    const DWORD size=GetPrivateProfileSectionNamesW(sections,std::size(sections),path);
    if(size>=std::size(sections)-2)return false;
    for(const wchar_t* section=sections;*section;section+=wcslen(section)+1)
        if(_wcsicmp(section,L"runtime"))return false;
    wchar_t entries[512]{};
    const DWORD length=GetPrivateProfileSectionW(L"runtime",entries,std::size(entries),path);
    if(length>=std::size(entries)-2)return false;
    bool seenLog=false,seenDiagnostics=false;
    for(wchar_t* entry=entries;*entry;entry+=wcslen(entry)+1) {
        const wchar_t* separator=wcschr(entry,L'=');
        if(!separator || (wcscmp(separator+1,L"0") && wcscmp(separator+1,L"1")))return false;
        const size_t keyLength=separator-entry;
        if(keyLength==7 && !_wcsnicmp(entry,L"Logging",7) && !seenLog) {
            logging=separator[1]==L'1';seenLog=true;
        } else if(keyLength==11 && !_wcsnicmp(entry,L"Diagnostics",11) && !seenDiagnostics) {
            diagnostics=separator[1]==L'1';seenDiagnostics=true;
        } else return false;
    }
    return seenLog && seenDiagnostics;
}
