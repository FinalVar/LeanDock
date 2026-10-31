#pragma once
#include "platform-latest.h"
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <winver.h>
#include <algorithm>
#include <cwctype>

namespace Capsule {
struct AudioOutput {std::wstring id,name,detail;int form=-1;bool selected=false;};
// One row per app: sessions of the same program share a slider, as in the
// Windows volume mixer.
struct AudioApp {std::wstring key,name,path;int volume=-1;bool mute=false,system=false;};
// What the sound page shows. Read only while that page is open.
struct SoundList {std::vector<AudioOutput> outputs;std::vector<AudioApp> apps;bool ready=false;};
inline const wchar_t* SystemSoundsKey=L"system";

// Choosing the default output has no documented API. Windows' own sound
// settings use the PolicyConfig client; its long-stable vtable is declared
// here as EarTrumpet and AudioDeviceCmdlets declare it (reference only).
struct IPolicyConfig : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR,WAVEFORMATEX**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR,INT,WAVEFORMATEX**)=0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR)=0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR,WAVEFORMATEX*,WAVEFORMATEX*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR,INT,PINT64,PINT64)=0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR,PINT64)=0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR,void*)=0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR,void*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR,const PROPERTYKEY&,PROPVARIANT*)=0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR,const PROPERTYKEY&,PROPVARIANT*)=0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR,ERole)=0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR,INT)=0;
};

// Belongs to the Metrics MTA worker. Everything here is synchronous COM on
// the audio service: outputs and app sessions are re-read once a second while
// the page is open, and nothing is held between reads but the enumerator.
class SoundBackend {
    static constexpr GUID PolicyConfigClient{0x870af99c,0x171d,0x4f9e,{0xaf,0x0d,0xe6,0x3d,0xf4,0x0c,0x2b,0xc9}};
    static constexpr GUID PolicyConfigIid{0xf8679f50,0x850a,0x41cf,{0x9c,0x72,0x43,0x0f,0x29,0x02,0x90,0xc8}};
    static constexpr PROPERTYKEY FormFactor{{0x1da5d803,0xd492,0x4edd,{0x8c,0x23,0xe0,0xc0,0xff,0xee,0x7f,0x0e}},0};
    static constexpr PROPERTYKEY InterfaceName{{0x026e516e,0xb814,0x414b,{0x83,0xcd,0x85,0x6d,0x6f,0xef,0x48,0x22}},2};
    winrt::com_ptr<IMMDeviceEnumerator> devices;
    std::vector<std::pair<std::wstring,std::wstring>> names; // program path -> display name
    uint64_t readAt=0;bool active=false;
    IMMDeviceEnumerator* Devices(){
        if(!devices)winrt::check_hresult(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,
            __uuidof(IMMDeviceEnumerator),devices.put_void()));
        return devices.get();
    }
    static std::wstring Text(IPropertyStore* store,PROPERTYKEY const& key){
        PROPVARIANT v;PropVariantInit(&v);std::wstring r;
        if(SUCCEEDED(store->GetValue(key,&v))&&v.vt==VT_LPWSTR&&v.pwszVal)r=v.pwszVal;
        PropVariantClear(&v);return r;
    }
    static std::wstring Lower(std::wstring s){for(auto& c:s)c=wchar_t(std::towlower(c));return s;}
    static std::wstring ProcessPath(DWORD pid){
        std::wstring path;
        if(HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid)){
            wchar_t b[MAX_PATH*2]{};DWORD size=MAX_PATH*2;if(QueryFullProcessImageNameW(p,0,b,&size))path.assign(b,size);
            CloseHandle(p);
        }
        return path;
    }
    // The program's own description ("Microsoft Edge"), else its file name.
    std::wstring ProgramName(std::wstring const& path){
        for(auto const& [p,n]:names)if(p==path)return n;
        std::wstring name;
        DWORD ignored=0;const DWORD size=GetFileVersionInfoSizeW(path.c_str(),&ignored);
        if(size){std::vector<BYTE> data(size);
            struct Translation{WORD language,page;}* t=nullptr;UINT bytes=0;
            if(GetFileVersionInfoW(path.c_str(),0,size,data.data())&&
               VerQueryValueW(data.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&t),&bytes)&&bytes>=sizeof(*t)){
                wchar_t query[64];swprintf_s(query,L"\\StringFileInfo\\%04x%04x\\FileDescription",t->language,t->page);
                wchar_t* text=nullptr;UINT length=0;
                if(VerQueryValueW(data.data(),query,reinterpret_cast<void**>(&text),&length)&&length>1)name.assign(text,wcsnlen(text,length));
            }
        }
        while(!name.empty()&&name.back()==L' ')name.pop_back();
        if(name.empty())name=std::filesystem::path(path).stem().wstring();
        names.emplace_back(path,name);return name;
    }
    std::wstring SessionName(IAudioSessionControl* c,std::wstring const& path,DWORD pid){
        LPWSTR raw=nullptr;std::wstring name;
        if(SUCCEEDED(c->GetDisplayName(&raw))&&raw){
            if(raw[0]==L'@'){wchar_t text[256]{};if(SUCCEEDED(SHLoadIndirectString(raw,text,256,nullptr)))name=text;}
            else name=raw;
        }
        CoTaskMemFree(raw);
        if(!name.empty())return name;
        return path.empty()?L"进程 "+std::to_wstring(pid):ProgramName(path);
    }
    winrt::com_ptr<IMMDevice> Default(){
        winrt::com_ptr<IMMDevice> d;
        if(FAILED(Devices()->GetDefaultAudioEndpoint(eRender,eMultimedia,d.put())))return nullptr;
        return d;
    }
    // Every live session on the default output with the row key it belongs to.
    template<class F> void EachSession(F&& f){
        auto device=Default();if(!device)return;
        winrt::com_ptr<IAudioSessionManager2> manager;
        winrt::check_hresult(device->Activate(__uuidof(IAudioSessionManager2),CLSCTX_INPROC_SERVER,nullptr,manager.put_void()));
        winrt::com_ptr<IAudioSessionEnumerator> sessions;winrt::check_hresult(manager->GetSessionEnumerator(sessions.put()));
        int count=0;winrt::check_hresult(sessions->GetCount(&count));
        for(int i=0;i<count;++i){
            winrt::com_ptr<IAudioSessionControl> c;if(FAILED(sessions->GetSession(i,c.put())))continue;
            AudioSessionState state{};if(FAILED(c->GetState(&state))||state==AudioSessionStateExpired)continue;
            auto c2=c.try_as<IAudioSessionControl2>();auto volume=c.try_as<ISimpleAudioVolume>();if(!c2||!volume)continue;
            DWORD pid=0;c2->GetProcessId(&pid);const bool system=c2->IsSystemSoundsSession()==S_OK;
            const auto path=system?std::wstring():ProcessPath(pid);
            const auto key=system?std::wstring(SystemSoundsKey):path.empty()?L"pid:"+std::to_wstring(pid):Lower(path);
            f(key,c.get(),volume.get(),path,pid,system);
        }
    }
    void Read(SoundList& s,uint64_t now){
        readAt=now;
        std::vector<AudioOutput> outputs;
        auto device=Default();LPWSTR raw=nullptr;std::wstring selected;
        if(device&&SUCCEEDED(device->GetId(&raw))&&raw)selected=raw;CoTaskMemFree(raw);
        winrt::com_ptr<IMMDeviceCollection> all;
        winrt::check_hresult(Devices()->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,all.put()));
        UINT n=0;winrt::check_hresult(all->GetCount(&n));
        for(UINT i=0;i<n;++i){
            winrt::com_ptr<IMMDevice> d;if(FAILED(all->Item(i,d.put())))continue;
            LPWSTR id=nullptr;if(FAILED(d->GetId(&id))||!id)continue;
            AudioOutput o;o.id=id;CoTaskMemFree(id);o.selected=o.id==selected;
            winrt::com_ptr<IPropertyStore> store;
            if(SUCCEEDED(d->OpenPropertyStore(STGM_READ,store.put()))){
                // "扬声器" over "Realtek(R) Audio", as the two lines Windows shows.
                o.name=Text(store.get(),PKEY_Device_DeviceDesc);o.detail=Text(store.get(),InterfaceName);
                if(o.name.empty()){o.name=Text(store.get(),PKEY_Device_FriendlyName);o.detail.clear();}
                PROPVARIANT v;PropVariantInit(&v);
                if(SUCCEEDED(store->GetValue(FormFactor,&v))&&v.vt==VT_UI4)o.form=int(v.ulVal);
                PropVariantClear(&v);
            }
            if(o.name.empty())o.name=L"音频输出";
            outputs.push_back(std::move(o));
        }
        std::vector<AudioApp> apps;
        EachSession([&](std::wstring const& key,IAudioSessionControl* c,ISimpleAudioVolume* volume,std::wstring const& path,DWORD pid,bool system){
            if(std::any_of(apps.begin(),apps.end(),[&](AudioApp const& a){return a.key==key;}))return;
            AudioApp a;a.key=key;a.path=path;a.system=system;a.name=system?L"系统声音":SessionName(c,path,pid);
            float level=0;BOOL mute=FALSE;
            if(SUCCEEDED(volume->GetMasterVolume(&level)))a.volume=int(std::lround(std::clamp(level,0.f,1.f)*100));
            if(SUCCEEDED(volume->GetMute(&mute)))a.mute=mute!=FALSE;
            apps.push_back(std::move(a));
        });
        // System sounds first, then by name; rows keep their places while the
        // page is open so the one under the pointer never moves.
        std::sort(apps.begin(),apps.end(),[](AudioApp const& a,AudioApp const& b){
            return a.system!=b.system?a.system:a.name<b.name;});
        auto rank=[&](AudioApp const& a){
            const auto at=std::find_if(s.apps.begin(),s.apps.end(),[&](AudioApp const& o){return o.key==a.key;});
            return size_t(at-s.apps.begin());
        };
        std::stable_sort(apps.begin(),apps.end(),[&](AudioApp const& a,AudioApp const& b){return rank(a)<rank(b);});
        s.outputs=std::move(outputs);s.apps=std::move(apps);s.ready=true;
    }
public:
    bool Active()const{return active;}
    void Open(){active=true;readAt=0;}
    bool Pump(SoundList& s){
        const auto now=GetTickCount64();
        if(readAt&&now-readAt<1000)return false;
        try{Read(s,now);}catch(...){Log(L"sound-page-error",winrt::to_hresult());devices=nullptr;s.ready=true;}
        return true;
    }
    // Each returns the error to show, or nullptr, and reads the list back.
    // Windows' sound settings make one output the default for every role.
    const wchar_t* Select(std::wstring const& id,SoundList& s,uint64_t& writes){
        try{
            winrt::com_ptr<IPolicyConfig> policy;
            winrt::check_hresult(CoCreateInstance(PolicyConfigClient,nullptr,CLSCTX_ALL,PolicyConfigIid,policy.put_void()));
            for(auto role:{eConsole,eMultimedia,eCommunications})winrt::check_hresult(policy->SetDefaultEndpoint(id.c_str(),role));
            ++writes;Read(s,GetTickCount64());return nullptr;
        }catch(...){Log(L"sound-output-failed",winrt::to_hresult());devices=nullptr;return L"未能切换输出设备";}
    }
    // value: 0-100 for the volume, or -1 with mute set.
    const wchar_t* App(std::wstring const& key,int value,int mute,SoundList& s,uint64_t& writes){
        try{
            int found=0;
            EachSession([&](std::wstring const& k,IAudioSessionControl*,ISimpleAudioVolume* volume,std::wstring const&,DWORD,bool){
                if(k!=key)return;++found;
                if(value>=0){winrt::check_hresult(volume->SetMasterVolume(std::clamp(value,0,100)/100.f,nullptr));
                    if(value>0)winrt::check_hresult(volume->SetMute(FALSE,nullptr));}
                else winrt::check_hresult(volume->SetMute(mute?TRUE:FALSE,nullptr));
            });
            if(!found)return L"这个应用已不再播放声音";
            ++writes;Read(s,GetTickCount64());return nullptr;
        }catch(...){Log(L"sound-app-failed",winrt::to_hresult());devices=nullptr;return L"未能调整应用音量";}
    }
    void Close(SoundList& s){devices=nullptr;names.clear();active=false;readAt=0;s=SoundList{};}
    ~SoundBackend(){SoundList ignored;Close(ignored);}
};
}
