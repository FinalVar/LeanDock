#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <wbemcli.h>
#include <oleauto.h>
#include <winrt/base.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

// Wheel and two-finger input on the time key change the volume, or with Ctrl
// the brightness, in place. The endpoint-volume and WMI calls are the hover
// panel's (aggregate/controls-latest.h: Audio, Connect, Active, BrightnessRead,
// BrightnessSet), run here on a short-lived worker thread so that a slow WMI
// call never blocks the taskbar. The thread starts on demand, exits after a
// few idle seconds and holds a reference on this module until it has left.
namespace LeanDockKeyLevels {
enum class Kind : unsigned { Volume, Brightness };
// Volume moves like the Windows volume keys; brightness in coarser steps.
inline constexpr int VolumeUnit=2,BrightnessUnit=5;
inline constexpr DWORD IdleMs=4000;
// Pure: one WHEEL_DELTA is one step. High-resolution wheels keep their
// remainder; a pause longer than half a second drops it.
struct Notches {
    int remainder=0;uint64_t last=0;
    void Reset(){remainder=0;last=0;}
    int Step(int delta,uint64_t now){
        if(last&&(now<last||now-last>500))remainder=0;
        last=now;remainder+=std::clamp(delta,-10*WHEEL_DELTA,10*WHEEL_DELTA);
        const int steps=remainder/WHEEL_DELTA;remainder-=steps*WHEEL_DELTA;return steps;
    }
};
// Pure: the next percentage.
inline int Next(int current,int steps,Kind kind) {
    const int unit=kind==Kind::Volume?VolumeUnit:BrightnessUnit;
    return std::clamp(current+steps*unit,0,100);
}
struct Result {Kind kind{};int value=-1;bool muted=false,ok=false;};
// Pure: the key's two lines while a level is shown, value over label.
struct Readout {std::wstring value,label;};
inline Readout Show(Result const& r) {
    if(r.kind==Kind::Brightness)return {r.ok?std::to_wstring(r.value):L"—",L"亮度"};
    if(!r.ok)return {L"—",L"音量"};
    return {std::to_wstring(r.value),r.muted?L"静音":L"音量"};
}

class Devices {
    struct Bstr {BSTR p;explicit Bstr(const wchar_t* s):p(SysAllocString(s)){}~Bstr(){SysFreeString(p);}
        operator BSTR()const{return p;}};
    struct Value {VARIANT v;Value(){VariantInit(&v);}~Value(){VariantClear(&v);}};
    winrt::com_ptr<IMMDeviceEnumerator> audioDevices;
    winrt::com_ptr<IAudioEndpointVolume> audioVolume;
    std::wstring audioId;
    winrt::com_ptr<IWbemServices> wmi;
    std::wstring brightnessInstance;int brightness=-1;
    // Reuse the endpoint while Windows' default output stays the same device.
    winrt::com_ptr<IAudioEndpointVolume> Audio(){
        if(!audioDevices)winrt::check_hresult(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,
            __uuidof(IMMDeviceEnumerator),audioDevices.put_void()));
        winrt::com_ptr<IMMDevice> device;
        winrt::check_hresult(audioDevices->GetDefaultAudioEndpoint(eRender,eMultimedia,device.put()));
        LPWSTR raw=nullptr;winrt::check_hresult(device->GetId(&raw));
        std::wstring id(raw?raw:L"");CoTaskMemFree(raw);
        if(!audioVolume||id!=audioId){
            winrt::com_ptr<IAudioEndpointVolume> volume;
            winrt::check_hresult(device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_INPROC_SERVER,nullptr,volume.put_void()));
            audioVolume=std::move(volume);audioId=std::move(id);
        }
        return audioVolume;
    }
    void Connect(){
        if(wmi)return;
        winrt::com_ptr<IWbemLocator> locator;
        winrt::check_hresult(CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,IID_IWbemLocator,locator.put_void()));
        winrt::check_hresult(locator->ConnectServer(Bstr(L"ROOT\\WMI"),nullptr,nullptr,nullptr,
            WBEM_FLAG_CONNECT_USE_MAX_WAIT,nullptr,nullptr,wmi.put()));
        winrt::check_hresult(CoSetProxyBlanket(wmi.get(),RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,nullptr,
            RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE));
    }
    winrt::com_ptr<IWbemClassObject> Active(const wchar_t* query,std::wstring const& instance=L""){
        Connect();winrt::com_ptr<IEnumWbemClassObject> items;
        winrt::check_hresult(wmi->ExecQuery(Bstr(L"WQL"),Bstr(query),
            WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,items.put()));
        for(unsigned n=0;n<16;++n){
            winrt::com_ptr<IWbemClassObject> item;ULONG count=0;
            auto hr=items->Next(1000,1,item.put(),&count);
            winrt::check_hresult(hr);if(!count){if(hr==WBEM_S_TIMEDOUT)winrt::throw_hresult(HRESULT_FROM_WIN32(WAIT_TIMEOUT));break;}
            Value active;winrt::check_hresult(item->Get(L"Active",0,&active.v,nullptr,nullptr));
            if(active.v.vt!=VT_BOOL||!active.v.boolVal)continue;
            if(!instance.empty()){Value name;winrt::check_hresult(item->Get(L"InstanceName",0,&name.v,nullptr,nullptr));
                if(name.v.vt!=VT_BSTR||instance!=name.v.bstrVal)continue;}
            return item;
        }
        return nullptr;
    }
    void BrightnessRead(){
        brightness=-1;brightnessInstance.clear();
        auto item=Active(L"SELECT Active,CurrentBrightness,InstanceName FROM WmiMonitorBrightness");
        if(!item)winrt::throw_hresult(E_NOTIMPL);
        Value level,name;winrt::check_hresult(item->Get(L"CurrentBrightness",0,&level.v,nullptr,nullptr));
        winrt::check_hresult(item->Get(L"InstanceName",0,&name.v,nullptr,nullptr));
        winrt::check_hresult(VariantChangeType(&level.v,&level.v,0,VT_I4));
        if(level.v.lVal<0||level.v.lVal>100||name.v.vt!=VT_BSTR)winrt::throw_hresult(E_UNEXPECTED);
        brightness=level.v.lVal;brightnessInstance=name.v.bstrVal;
    }
    void BrightnessSet(int value){
        auto item=Active(L"SELECT Active,InstanceName,__PATH FROM WmiMonitorBrightnessMethods",brightnessInstance);
        if(!item)winrt::throw_hresult(E_NOTIMPL);
        Value path;winrt::check_hresult(item->Get(L"__PATH",0,&path.v,nullptr,nullptr));
        if(path.v.vt!=VT_BSTR)winrt::throw_hresult(E_UNEXPECTED);
        winrt::com_ptr<IWbemClassObject> klass,definition,arguments;
        winrt::check_hresult(wmi->GetObject(Bstr(L"WmiMonitorBrightnessMethods"),0,nullptr,klass.put(),nullptr));
        winrt::check_hresult(klass->GetMethod(L"WmiSetBrightness",0,definition.put(),nullptr));
        winrt::check_hresult(definition->SpawnInstance(0,arguments.put()));
        Value timeout,level;timeout.v.vt=VT_I4;timeout.v.lVal=0;level.v.vt=VT_UI1;level.v.bVal=BYTE(std::clamp(value,0,100));
        winrt::check_hresult(arguments->Put(L"Timeout",0,&timeout.v,0));
        winrt::check_hresult(arguments->Put(L"Brightness",0,&level.v,0));
        winrt::com_ptr<IWbemCallResult> call;
        winrt::check_hresult(wmi->ExecMethod(path.v.bstrVal,Bstr(L"WmiSetBrightness"),WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,arguments.get(),nullptr,call.put()));
        winrt::com_ptr<IWbemClassObject> result;auto hr=call->GetResultObject(1500,result.put());
        if(hr==WBEM_S_TIMEDOUT)winrt::throw_hresult(HRESULT_FROM_WIN32(WAIT_TIMEOUT));
        winrt::check_hresult(hr);
        // Declared void: a completed call has no out-parameter object.
        if(!result)return;
        Value returned;
        if(FAILED(result->Get(L"ReturnValue",0,&returned.v,nullptr,nullptr))||returned.v.vt==VT_NULL||returned.v.vt==VT_EMPTY)return;
        winrt::check_hresult(VariantChangeType(&returned.v,&returned.v,0,VT_I4));
        if(returned.v.lVal)winrt::throw_hresult(E_FAIL);
    }
public:
    Result Volume(int steps){
        Result r{Kind::Volume};
        try{
            auto audio=Audio();float level=0;BOOL mute=FALSE;
            winrt::check_hresult(audio->GetMasterVolumeLevelScalar(&level));winrt::check_hresult(audio->GetMute(&mute));
            const int current=int(std::lround(std::clamp(level,0.f,1.f)*100)),next=Next(current,steps,Kind::Volume);
            if(next!=current)winrt::check_hresult(audio->SetMasterVolumeLevelScalar(next/100.f,nullptr));
            // Raising the volume unmutes, as the volume keys do.
            if(steps>0&&mute){winrt::check_hresult(audio->SetMute(FALSE,nullptr));mute=FALSE;}
            r.value=next;r.muted=mute!=FALSE;r.ok=true;
        }catch(...){audioVolume=nullptr;audioDevices=nullptr;audioId.clear();}
        return r;
    }
    // The level is read once per worker lifetime and then tracked, since the
    // reported level can lag right after a change.
    Result Brightness(int steps){
        Result r{Kind::Brightness};
        try{
            if(brightness<0||brightnessInstance.empty())BrightnessRead();
            const int next=Next(brightness,steps,Kind::Brightness);
            if(next!=brightness)BrightnessSet(next);
            brightness=next;r.value=next;r.ok=true;
        }catch(...){wmi=nullptr;brightnessInstance.clear();brightness=-1;}
        return r;
    }
};

class Worker {
    struct Shared {
        std::mutex mutex;std::condition_variable wake;
        int steps[2]{};bool running=false,stopped=false;
        std::function<void(Result)> report;
    };
    std::shared_ptr<Shared> shared=std::make_shared<Shared>();
    static void Body(Shared& s){
        Devices devices;
        for(;;){
            int take[2]{};std::function<void(Result)> report;
            {
                std::unique_lock lock(s.mutex);
                s.wake.wait_for(lock,std::chrono::milliseconds(IdleMs),[&]{return s.stopped||s.steps[0]||s.steps[1];});
                if(s.stopped||(!s.steps[0]&&!s.steps[1])){s.running=false;return;}
                take[0]=std::exchange(s.steps[0],0);take[1]=std::exchange(s.steps[1],0);report=s.report;
            }
            if(take[0]){auto r=devices.Volume(take[0]);if(report)report(r);}
            if(take[1]){auto r=devices.Brightness(take[1]);if(report)report(r);}
        }
    }
    static DWORD WINAPI Run(void* parameter){
        HMODULE module{};
        {
            std::unique_ptr<std::shared_ptr<Shared>> handoff(static_cast<std::shared_ptr<Shared>*>(parameter));
            auto s=*handoff;handoff.reset();
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&Run),&module);
            if(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED))){
                try{Body(*s);}catch(...){std::lock_guard guard(s->mutex);s->running=false;}
                CoUninitialize();
            }else{std::lock_guard guard(s->mutex);s->running=false;}
        }
        // Releases the reference taken in Request after every object above is gone.
        FreeLibraryAndExitThread(module,0);
    }
public:
    Worker()=default;
    Worker(Worker const&)=delete;
    Worker& operator=(Worker const&)=delete;
    ~Worker(){Stop();}
    // report runs on the worker thread.
    void Reporter(std::function<void(Result)> report){std::lock_guard guard(shared->mutex);shared->report=std::move(report);}
    bool Request(Kind kind,int steps){
        if(!steps)return true;
        std::lock_guard guard(shared->mutex);
        if(shared->stopped)return false;
        auto& pending=shared->steps[unsigned(kind)];
        pending=std::clamp(pending+steps,-100,100);
        if(shared->running){shared->wake.notify_one();return true;}
        HMODULE module{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&Run),&module)){pending=0;return false;}
        auto handoff=new std::shared_ptr<Shared>(shared);
        HANDLE thread=CreateThread(nullptr,0,Run,handoff,0,nullptr);
        if(!thread){delete handoff;FreeLibrary(module);pending=0;return false;}
        CloseHandle(thread);shared->running=true;return true;
    }
    bool Running()const{std::lock_guard guard(shared->mutex);return shared->running;}
    // The worker finishes at most the call in progress; nothing is reported after this.
    void Stop(){
        std::lock_guard guard(shared->mutex);
        shared->stopped=true;shared->report=nullptr;shared->steps[0]=shared->steps[1]=0;shared->wake.notify_all();
    }
};
}
