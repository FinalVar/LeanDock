#pragma once
#include <windows.h>
#include <bcrypt.h>
#include "symbols-generated.h"

inline bool NativeHashFile(const wchar_t* path, wchar_t result[65]) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                            nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    DWORD objectSize=0, read=0;
    unsigned char digest[32], *object=nullptr, *buffer=nullptr;
    bool ok=false;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)goto done;
    if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectSize),
                          sizeof(objectSize),&read,0)<0)goto done;
    object=static_cast<unsigned char*>(HeapAlloc(GetProcessHeap(),0,objectSize));
    buffer=static_cast<unsigned char*>(HeapAlloc(GetProcessHeap(),0,65536));
    if(!object || !buffer)goto done;
    if(BCryptCreateHash(algorithm,&hash,object,objectSize,nullptr,0,0)<0)goto done;
    for(;;) {
        if(!ReadFile(file,buffer,65536,&read,nullptr))goto done;
        if(!read)break;
        if(BCryptHashData(hash,buffer,read,0)<0)goto done;
    }
    if(BCryptFinishHash(hash,digest,sizeof(digest),0)<0)goto done;
    for(unsigned i=0;i<32;i++) {
        constexpr wchar_t hex[]=L"0123456789ABCDEF";
        result[i*2]=hex[digest[i]>>4];result[i*2+1]=hex[digest[i]&15];
    }
    result[64]=0;ok=true;
done:
    if(hash)BCryptDestroyHash(hash);
    if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    if(object)HeapFree(GetProcessHeap(),0,object);
    if(buffer)HeapFree(GetProcessHeap(),0,buffer);
    CloseHandle(file);return ok;
}

inline bool NativeVerifySystem(wchar_t* failed, size_t capacity) {
    for(auto const& module:NativeModules) {
        wchar_t actual[65];
        if(!NativeHashFile(module.path,actual) || _wcsicmp(actual,module.sha256)) {
            wcsncpy_s(failed,capacity,module.name,_TRUNCATE);return false;
        }
    }
    if(capacity)failed[0]=0;
    return true;
}

inline bool NativeValidModule(HMODULE module, const NativeModulePin& expected) {
    if(!module)return false;
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>0x100000)return false;
    auto pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<const BYTE*>(module)+dos->e_lfanew);
    return pe->Signature==IMAGE_NT_SIGNATURE &&
        pe->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64 &&
        pe->FileHeader.TimeDateStamp==expected.timestamp &&
        pe->OptionalHeader.SizeOfImage==expected.imageSize;
}
