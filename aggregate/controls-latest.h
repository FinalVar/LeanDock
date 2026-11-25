#pragma once
#include "platform-latest.h"
#include "theme-control-latest.h"
#include "wifi-latest.h"
#include "bluetooth-latest.h"
#include "sound-latest.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <powrprof.h>
#include <wbemidl.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Networking.Connectivity.h>
#include <winrt/Windows.Networking.NetworkOperators.h>
#include <array>
#include <optional>
#include <stop_token>

namespace Capsule {
// Panel pages. Second-level pages slide over the main page inside the panel.
enum class PanelPage : int { Main, Wifi, Bluetooth, Hotspot, Sound };
enum class Control : unsigned { Volume, Brightness, Mute, Wifi, Bluetooth, DarkTheme, Hotspot, WifiConnect, WifiDisconnect,
    BluetoothConnect, OutputDevice, AppVolume, AppMute, HotspotConfigure, MediaPrev, MediaToggle, MediaNext, Count };
using RadioAccess=winrt::Windows::Devices::Radios::RadioAccessStatus;
using AccessRequest=winrt::Windows::Foundation::IAsyncOperation<RadioAccess>;
// Page commands name their network, device, endpoint or app in key; only
// Wi-Fi and hotspot commands carry a secret.
struct ControlCommand {Control target{};int value{};uint64_t serial{};AccessRequest access{nullptr};
    std::wstring key,user,secret;};
// One latest value per control: dragging cannot build an unbounded queue or
// apply old brightness values after the pointer has already been released.
struct ControlQueue {
    std::array<std::optional<ControlCommand>,unsigned(Control::Count)> values;
    void Put(ControlCommand c){
        if(auto& old=values[unsigned(c.target)])SecureZeroMemory(old->secret.data(),old->secret.size()*sizeof(wchar_t));
        values[unsigned(c.target)]=std::move(c);
    }
    bool Empty()const{for(auto const& c:values)if(c)return false;return true;}
    std::vector<ControlCommand> Take(){std::vector<ControlCommand> out;
        for(auto& c:values)if(c){out.push_back(std::move(*c));c.reset();}
        std::sort(out.begin(),out.end(),[](auto const& a,auto const& b){return a.serial<b.serial;});return out;}
};
struct HotspotClient {std::wstring name,address;};
// What the hotspot page shows. Read only while that page is open; the key
// itself never enters this struct (see Metrics::HotspotKey).
struct HotspotInfo {
    std::wstring ssid,source,failure;int band=-1;size_t keyLength=0;unsigned maxClients=0;
    std::array<bool,3> bands{}; // TetheringWiFiBand Auto, 2.4 GHz, 5 GHz this adapter accepts
    std::vector<HotspotClient> clients;
    bool ready=false,saving=false;
};
struct Controls {
    int volume=-1,brightness=-1,wifi=-1,bluetooth=-1,darkTheme=-1,hotspot=-1,bluetoothBattery=-1;
    bool themeBusy=false,themeAvailable=false;
    DWORD themeLastExit=0;
    int hotspotCapability=-1;unsigned hotspotClients=0;
    std::wstring hotspotLabel=L"不可用";
    std::wstring wifiLabel,bluetoothLabel;
    bool hotspotBusy=false;
    bool mute=false,radioAllowed=false,wifiBusy=false,bluetoothBusy=false,mediaActive=false,mediaPlaying=false;
    std::wstring error;
    WifiList wifiList;BluetoothList bluetoothList;SoundList sound;HotspotInfo hotspotInfo;
    uint64_t errorAt=0,reads=0,writes=0;
    std::array<uint64_t,unsigned(Control::Count)> completed{};
};
inline bool RadioControllable(int state){return state==0||state==1||state==2;}
inline std::wstring RadioLabel(int state){return state==1?L"已开启":state==0?L"已关闭":
    state==2?L"部分开启":state==3?L"硬件已禁用":L"不可用";}
inline std::wstring TileRadioLabel(int state,std::wstring const& label){
    return (state==1||state==2)&&!label.empty()?label:RadioLabel(state);
}
inline std::wstring BluetoothConnectionLabel(int state,bool known,std::wstring const& summary){
    if(state!=1&&state!=2)return RadioLabel(state);
    if(!known||summary.empty())return RadioLabel(state);
    if(summary==L"未连接")return state==1?std::wstring(L"未连接"):RadioLabel(state);
    return summary;
}
inline int TileBluetoothBattery(int state,bool busy,int battery){
    return !busy&&(state==1||state==2)&&battery>=0&&battery<=100?battery:-1;
}
inline constexpr GUID VideoPowerSubgroup{0x7516b95f,0xf776,0x4464,{0x8c,0x53,0x06,0x16,0x7f,0x40,0xcc,0x99}};
inline constexpr GUID VideoBrightnessSetting{0xaded5e82,0xb909,0x4619,{0x99,0x49,0xf5,0xd7,0x1d,0xac,0x0b,0xcb}};
// On Windows 11 with DisplayEnhancementService, legacy WmiSetBrightness writes
// monitor.sys without updating the active power scheme and drops every second
// 100 -> 0 transition, while bursts faster than the ~200 ms transition ramp
// preempt in-flight changes. Pace writes to PaceMs while keeping the latest
// command serial pending so UI toggles and slider drags remain immediate.
struct BrightnessPacer {
    static constexpr uint64_t PaceMs=240;
    uint64_t lastWriteAt=0,deferredSerial=0;
    int deferredTarget=-1;
    bool Active()const{return deferredSerial!=0;}
    bool Ready(uint64_t now)const{
        return !lastWriteAt||now<lastWriteAt||now-lastWriteAt>=PaceMs;
    }
    uint64_t Remaining(uint64_t now)const{
        return Ready(now)?0:PaceMs-(now-lastWriteAt);
    }
    bool Request(uint64_t serial,int target,uint64_t now){
        target=std::clamp(target,0,100);
        if(!Ready(now)){deferredSerial=serial;deferredTarget=target;return false;}
        deferredSerial=0;deferredTarget=-1;return true;
    }
    void RecordWrite(uint64_t now,uint64_t followSerial=0,int followTarget=-1){
        lastWriteAt=now?now:1;
        if(followSerial){deferredSerial=followSerial;deferredTarget=std::clamp(followTarget,0,100);}
    }
    bool TakeDue(uint64_t now,uint64_t& serialOut,int& targetOut){
        if(!deferredSerial||!Ready(now))return false;
        serialOut=deferredSerial;targetOut=deferredTarget;
        deferredSerial=0;deferredTarget=-1;return true;
    }
    void Clear(uint64_t& completed){
        if(deferredSerial)completed=std::max(completed,deferredSerial);
        lastWriteAt=deferredSerial=0;deferredTarget=-1;
    }
};
// WMI CurrentBrightness can lag right after a brightness write while the driver
// ramps the backlight. Keep the command pending and poll briefly until the
// hardware readback reaches the requested level (or settles on timeout), so
// rapid or subsequent toggles never read a stale 0 or mid-ramp value.
struct BrightnessSettle {
    static constexpr uint64_t Limit=2500,Step=100;
    uint64_t serial=0,at=0,polledAt=0;
    int target=-1;
    bool Active()const{return serial!=0;}
    void Clear(uint64_t& completed){
        if(serial)completed=std::max(completed,serial);
        serial=at=polledAt=0;target=-1;
    }
    void Begin(uint64_t cmdSerial,int desired,int readback,bool wrote,uint64_t now,uint64_t& completed){
        if(!wrote||readback==desired){
            Clear(completed);
            completed=std::max(completed,cmdSerial);
            return;
        }
        serial=cmdSerial;target=desired;at=polledAt=now;
    }
    bool Due(uint64_t now)const{
        return serial!=0&&(now<polledAt||now-polledAt>=Step);
    }
    void Observe(int readback,uint64_t now,uint64_t& completed){
        if(!serial)return;
        polledAt=now;
        if(readback==target||now<at||now-at>=Limit)Clear(completed);
    }
};

// All devices and COM proxies below belong to the existing Metrics MTA worker.
// No callbacks capture Dashboard/Popup; asynchronous results are polled only
// while an operation is pending and cancelled before apartment teardown.
class ControlBackend {
    using TetherManager=winrt::Windows::Networking::NetworkOperators::NetworkOperatorTetheringManager;
    using TetherState=winrt::Windows::Networking::NetworkOperators::TetheringOperationalState;
    using TetherCapability=winrt::Windows::Networking::NetworkOperators::TetheringCapability;
    using TetherResult=winrt::Windows::Networking::NetworkOperators::NetworkOperatorTetheringOperationResult;
    using TetherStatus=winrt::Windows::Networking::NetworkOperators::TetheringOperationStatus;
    using NetworkInfo=winrt::Windows::Networking::Connectivity::NetworkInformation;
    using TetherOperation=winrt::Windows::Foundation::IAsyncOperation<TetherResult>;
    using Radio=winrt::Windows::Devices::Radios::Radio;
    using RadioState=winrt::Windows::Devices::Radios::RadioState;
    using RadioKind=winrt::Windows::Devices::Radios::RadioKind;
    using Radios=winrt::Windows::Foundation::Collections::IVectorView<Radio>;
    using Enumeration=winrt::Windows::Foundation::IAsyncOperation<Radios>;
    using BtCollection=winrt::Windows::Devices::Enumeration::DeviceInformationCollection;
    using BtQuery=winrt::Windows::Foundation::IAsyncOperation<BtCollection>;
    using AsyncStatus=winrt::Windows::Foundation::AsyncStatus;
    static constexpr uint64_t BtQueryLimit=5000,BtReadEvery=2000,BtBatteryEvery=30000;
    struct Bstr {BSTR p;explicit Bstr(const wchar_t* s):p(SysAllocString(s)){}~Bstr(){SysFreeString(p);}
        operator BSTR()const{return p;}};
    struct Value {VARIANT v;Value(){VariantInit(&v);}~Value(){VariantClear(&v);}};
    winrt::com_ptr<IWbemServices> wmi;
    std::wstring brightnessInstance;
    Enumeration enumeration{nullptr};
    std::vector<Radio> radios;
    uint64_t enumAt=0;
    HANDLE wlanClient=nullptr;
    WifiConnection wifiState;
    BtQuery btQuery{nullptr},btBattery{nullptr};
    uint64_t btQueryAt=0,btReadAt=0,btBatteryAt=0,btBatteryReadAt=0;
    bool btKnown=false;
    std::wstring btTarget;
    winrt::guid btBatteryFor{},btSingleContainer{};
    int btBatteryLevel=-1;
    struct RadioChange {ControlCommand command;uint64_t at=0;
        std::vector<AccessRequest> changes;bool started=false;};
    std::optional<RadioChange> change;
    bool accessAllowed=false;
    ThemeControl::Worker theme;
    uint64_t themeSerial=0;
    void ThemeRead(Controls& s){
        s.darkTheme=ThemeControl::Read();s.themeAvailable=theme.available;
        s.themeBusy=theme.Pending()||ThemeControl::OtherWorker();
    }
    void ThemePump(Controls& s){
        ThemeControl::Completion result;
        if(theme.Poll(result)){
            s.completed[unsigned(Control::DarkTheme)]=themeSerial;themeSerial=0;s.themeLastExit=result.code;
            if(result.success){++s.writes;Log(L"theme-switch-complete",result.actual);}
            else{Log(L"theme-switch-failed",result.code);Error(s,L"主题切换未完成，请检查系统主题设置");}
        }
        ThemeRead(s);
    }
    void ThemeSet(ControlCommand const& command,Controls& s){
        if(theme.Pending()){s.completed[unsigned(command.target)]=command.serial;return;}
        const auto result=ThemeControl::Read()<0?DWORD(ERROR_INVALID_DATA):theme.Start(command.value!=0);
        s.themeLastExit=result;
        if(result){s.completed[unsigned(command.target)]=command.serial;Log(L"theme-switch-start-failed",result);
            Error(s,result==ERROR_BUSY?L"系统正在切换主题":L"主题切换不可用，请检查系统设置");}
        else{themeSerial=command.serial;s.error.clear();Log(L"theme-switch-request",command.value);}
        ThemeRead(s);
    }
    TetherManager tether{nullptr};
    winrt::guid tetherAdapter{};std::wstring tetherProfile;
    TetherOperation tetherOperation{nullptr};
    std::optional<ControlCommand> tetherCommand;
    uint64_t tetherAt=0;bool tetherSuccess=false;
    void HotspotOpen(){
        if(tetherCommand)return;
        try{HotspotOpen(NetworkInfo::GetInternetConnectionProfile());}catch(...){tether=nullptr;tetherProfile.clear();}
    }
    void HotspotOpen(winrt::Windows::Networking::Connectivity::ConnectionProfile const& profile){
        if(tetherCommand)return;
        // Follow Windows' current Internet profile. Do not choose a VPN/TUN
        // or reconfigure ICS, credentials, bands, drivers, or services ourselves.
        try{
            // An existing running hotspot remains stoppable even if Internet
            // connectivity changes. Never silently migrate a sharing session.
            if(tether&&tether.TetheringOperationalState()==TetherState::On)return;
            if(!profile||!profile.NetworkAdapter()){tether=nullptr;tetherProfile.clear();return;}
            auto id=profile.NetworkAdapter().NetworkAdapterId();auto name=std::wstring(profile.ProfileName());
            if(tether&&id==tetherAdapter&&name==tetherProfile)return;
            tether=TetherManager::CreateFromConnectionProfile(profile);tetherAdapter=id;tetherProfile=std::move(name);
        }catch(...){tether=nullptr;tetherProfile.clear();}
    }
    void HotspotRead(Controls& s,bool refresh=false){
        s.hotspot=-1;s.hotspotClients=0;s.hotspotCapability=-1;s.hotspotLabel=L"不可用";
        s.hotspotBusy=bool(tetherCommand);
        try{
            // One profile query serves both the refresh and the reading.
            auto profile=NetworkInfo::GetInternetConnectionProfile();
            if(refresh&&!tetherCommand)HotspotOpen(profile);
            if(profile)s.hotspotCapability=int(TetherManager::GetTetheringCapabilityFromConnectionProfile(profile));
            if(!tether){s.hotspotLabel=profile?L"不可用":L"未连接网络";return;}
            const auto actual=tether.TetheringOperationalState();
            if(actual==TetherState::On){s.hotspot=1;s.hotspotClients=tether.ClientCount();
                s.hotspotLabel=std::to_wstring(s.hotspotClients)+L" 台已连接";}
            else if(actual==TetherState::Off){s.hotspot=s.hotspotCapability==int(TetherCapability::Enabled)?0:-1;
                s.hotspotLabel=s.hotspot==0?L"已关闭":L"系统不可用";}
            else if(actual==TetherState::InTransition){s.hotspotBusy=true;s.hotspotLabel=L"正在切换";}
            if(tetherCommand)s.hotspotLabel=L"正在切换";
        }catch(...){s.hotspot=-1;s.hotspotLabel=L"不可用";}
    }
    void EndHotspot(Controls& s,const wchar_t* error=nullptr){
        const bool pending=bool(tetherCommand)||bool(tetherOperation);
        if(tetherOperation)try{tetherOperation.Cancel();}catch(...){}tetherOperation=nullptr;
        if(tetherCommand)s.completed[unsigned(Control::Hotspot)]=tetherCommand->serial;
        tetherCommand.reset();tetherSuccess=false;
        if(error)Error(s,error);
        if(pending)HotspotRead(s);else s.hotspotBusy=false;
    }
    void HotspotPump(Controls& s){
        if(!tetherCommand)return;
        try{
            if(GetTickCount64()-tetherAt>15000){EndHotspot(s,L"热点操作超时，请核对系统状态");return;}
            if(!tetherSuccess){
                auto status=tetherOperation.Status();
                if(status==AsyncStatus::Started)return;
                if(status!=AsyncStatus::Completed){EndHotspot(s,L"热点切换失败，请检查网络");return;}
                auto result=tetherOperation.GetResults();
                if(result.Status()!=TetherStatus::Success){
                    Log(L"hotspot-operation-failed",int(result.Status()));
                    EndHotspot(s,L"系统未能切换热点，请检查热点设置");return;}
                tetherSuccess=true;
            }
            HotspotRead(s);
            if(s.hotspot==tetherCommand->value){++s.writes;EndHotspot(s);}
        }catch(...){EndHotspot(s,L"热点操作失败，请重试");}
    }
    void HotspotSet(ControlCommand command,Controls& s){
        if(tetherCommand||change||configure){s.completed[unsigned(command.target)]=command.serial;return;}
        try{
            HotspotRead(s,true);
            if(s.hotspot==command.value){s.completed[unsigned(command.target)]=command.serial;return;}
            if(!tether||s.hotspot<0||s.hotspotBusy)winrt::throw_hresult(E_NOTIMPL);
            tetherOperation=command.value?tether.StartTetheringAsync():tether.StopTetheringAsync();
            tetherCommand=std::move(command);tetherAt=GetTickCount64();tetherSuccess=false;s.error.clear();
            HotspotRead(s);
        }catch(...){s.completed[unsigned(command.target)]=command.serial;EndHotspot(s,L"热点不可控制，请检查热点设置");}
    }
    winrt::Windows::Foundation::IAsyncAction configure{nullptr};
    uint64_t configureAt=0,configureSerial=0;
    void ConfigurePump(Controls& s){
        if(!configure)return;
        auto& h=s.hotspotInfo;AsyncStatus status=AsyncStatus::Error;
        try{status=configure.Status();}catch(...){}
        if(status==AsyncStatus::Started&&GetTickCount64()-configureAt<15000)return;
        if(status==AsyncStatus::Completed){++s.writes;h.failure.clear();}
        else{
            if(status==AsyncStatus::Started)try{configure.Cancel();}catch(...){}
            Log(L"hotspot-configure-failed",int(status));
            h.failure=status==AsyncStatus::Started?L"保存热点设置超时，请核对系统设置":L"系统未能保存热点设置";
        }
        configure=nullptr;h.saving=false;s.completed[unsigned(Control::HotspotConfigure)]=configureSerial;
    }
    static void Error(Controls& s,const wchar_t* text){s.error=text;s.errorAt=GetTickCount64();}
    winrt::com_ptr<IMMDeviceEnumerator> audioDevices;
    winrt::com_ptr<IAudioEndpointVolume> audioVolume;
    std::wstring audioId;
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
    void AudioReset(){audioVolume=nullptr;audioDevices=nullptr;audioId.clear();}
    void AudioRead(Controls& s){
        s.volume=-1;s.mute=false;
        try{auto audio=Audio();float value=0;BOOL mute=FALSE;
            winrt::check_hresult(audio->GetMasterVolumeLevelScalar(&value));winrt::check_hresult(audio->GetMute(&mute));
            s.volume=int(std::lround(std::clamp(value,0.f,1.f)*100));s.mute=mute!=FALSE;
        }catch(...){AudioReset();}
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
    winrt::com_ptr<IWbemClassObject> Active(const wchar_t* query,const std::wstring& instance=L""){
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
        }return nullptr;
    }
    void BrightnessRead(Controls& s){
        const int prev=s.brightness;
        s.brightness=-1;brightnessInstance.clear();
        try{auto item=Active(L"SELECT Active,CurrentBrightness,InstanceName FROM WmiMonitorBrightness");if(!item)return;
            Value level,name;winrt::check_hresult(item->Get(L"CurrentBrightness",0,&level.v,nullptr,nullptr));
            winrt::check_hresult(item->Get(L"InstanceName",0,&name.v,nullptr,nullptr));
            winrt::check_hresult(VariantChangeType(&level.v,&level.v,0,VT_I4));
            if(level.v.lVal>=0&&level.v.lVal<=100&&name.v.vt==VT_BSTR){s.brightness=level.v.lVal;brightnessInstance=name.v.bstrVal;}
        }catch(...){wmi=nullptr;if(prev>=0)s.brightness=prev;}
    }
    uint64_t brightnessAt=0;
    BrightnessPacer brightnessPacer;
    BrightnessSettle brightnessSettle;
    // WMI brightness is a cross-process query. Every three seconds suffices
    // for a hover panel; a change made in the panel is read back at once.
    void BrightnessPoll(Controls& s,bool force){
        const auto now=GetTickCount64();
        if(!force&&brightnessAt&&now-brightnessAt<3000)return;
        brightnessAt=now;BrightnessRead(s);
    }
    void EnsureBrightnessInstance(){
        if(!brightnessInstance.empty())return;
        if(auto cur=Active(L"SELECT Active,InstanceName FROM WmiMonitorBrightness")){
            Value name;
            if(SUCCEEDED(cur->Get(L"InstanceName",0,&name.v,nullptr,nullptr))&&name.v.vt==VT_BSTR)
                brightnessInstance=name.v.bstrVal;
        }
        if(brightnessInstance.empty())winrt::throw_hresult(E_NOTIMPL);
    }
    static bool PowerBrightnessSet(int value,int wmiCurrent,bool& nudged){
        nudged=false;
        static HMODULE library=LoadLibraryExW(L"powrprof.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        using ApplyFn=DWORD(WINAPI*)(const GUID*,const GUID*);
        static auto apply=library?reinterpret_cast<ApplyFn>(GetProcAddress(library,"PowerApplySettingChanges")):nullptr;
        if(!apply)return false;
        GUID* scheme=nullptr;
        if(PowerGetActiveScheme(nullptr,&scheme)!=ERROR_SUCCESS||!scheme)return false;
        SYSTEM_POWER_STATUS sps{};
        const bool dc=GetSystemPowerStatus(&sps)&&sps.ACLineStatus==0;
        const DWORD desired=DWORD(std::clamp(value,0,100));
        DWORD current=MAXDWORD;
        const DWORD readRc=dc
            ?PowerReadDCValueIndex(nullptr,scheme,&VideoPowerSubgroup,&VideoBrightnessSetting,&current)
            :PowerReadACValueIndex(nullptr,scheme,&VideoPowerSubgroup,&VideoBrightnessSetting,&current);
        DWORD toWrite=desired;
        if(readRc==ERROR_SUCCESS&&current==desired){
            if(wmiCurrent==int(desired)){LocalFree(scheme);return true;}
            toWrite=desired>0?desired-1:1;
            nudged=true;
        }
        const DWORD writeRc=dc
            ?PowerWriteDCValueIndex(nullptr,scheme,&VideoPowerSubgroup,&VideoBrightnessSetting,toWrite)
            :PowerWriteACValueIndex(nullptr,scheme,&VideoPowerSubgroup,&VideoBrightnessSetting,toWrite);
        const bool ok=writeRc==ERROR_SUCCESS&&apply(&VideoPowerSubgroup,&VideoBrightnessSetting)==ERROR_SUCCESS;
        LocalFree(scheme);
        return ok;
    }
    void WmiBrightnessSet(int value){
        EnsureBrightnessInstance();
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
        // WmiSetBrightness is declared void: a completed call yields no
        // out-parameter object (measured on this machine, 2026-10-31). Only
        // an explicit non-zero status fails; the caller reads the level back.
        if(!result)return;
        Value returned;
        if(FAILED(result->Get(L"ReturnValue",0,&returned.v,nullptr,nullptr))||returned.v.vt==VT_NULL||returned.v.vt==VT_EMPTY)return;
        winrt::check_hresult(VariantChangeType(&returned.v,&returned.v,0,VT_I4));
        if(returned.v.lVal)winrt::throw_hresult(E_FAIL);
    }
    void BrightnessDispatch(uint64_t cmdSerial,int target,Controls& s){
        target=std::clamp(target,0,100);
        const int prev=s.brightness;
        bool wrote=false,nudged=false;
        const auto now=std::max<uint64_t>(GetTickCount64(),1);
        try{
            EnsureBrightnessInstance();
            if(!PowerBrightnessSet(target,prev,nudged))WmiBrightnessSet(target);
            ++s.writes;s.error.clear();wrote=true;
        }catch(...){Error(s,L"亮度调整失败，设备可能已断开");}
        brightnessAt=now;BrightnessRead(s);
        if(wrote){
            brightnessPacer.RecordWrite(now,nudged?cmdSerial:0,target);
            if(nudged){
                brightnessSettle.Clear(s.completed[unsigned(Control::Brightness)]);
                if(s.brightness<0&&prev>=0)s.brightness=prev;
                return;
            }
        }
        brightnessSettle.Begin(cmdSerial,target,s.brightness,wrote,now,s.completed[unsigned(Control::Brightness)]);
        if(brightnessSettle.Active()&&s.brightness<0&&prev>=0)s.brightness=prev;
    }
    void BrightnessPump(Controls& s){
        const auto now=std::max<uint64_t>(GetTickCount64(),1);
        uint64_t dueSerial=0;int dueTarget=-1;
        if(brightnessPacer.TakeDue(now,dueSerial,dueTarget)){
            BrightnessDispatch(dueSerial,dueTarget,s);
            return;
        }
        if(brightnessPacer.Active())return;
        if(!brightnessSettle.Due(now))return;
        const int prev=s.brightness;
        brightnessAt=now;BrightnessRead(s);
        brightnessSettle.Observe(s.brightness,now,s.completed[unsigned(Control::Brightness)]);
        if(brightnessSettle.Active()&&s.brightness<0&&prev>=0)s.brightness=prev;
    }
    void RadioRead(Controls& s){
        auto state=[&](RadioKind kind){int on=0,off=0,locked=0;
            for(auto const& radio:radios)try{if(radio.Kind()==kind){auto v=radio.State();
                if(v==RadioState::On)++on;else if(v==RadioState::Off)++off;else ++locked;}}catch(...){}
            if(on&&off)return 2;if(on)return locked?2:1;if(off)return 0;return locked?3:-1;};
        s.wifi=state(RadioKind::WiFi);s.bluetooth=state(RadioKind::Bluetooth);
        s.wifiLabel=WifiConnectionLabel(s.wifi,wifiState,RadioLabel(s.wifi));
        s.bluetoothLabel=BluetoothConnectionLabel(s.bluetooth,btKnown,btTarget);
        s.bluetoothBattery=(s.bluetooth==1||s.bluetooth==2)&&btKnown&&btSingleContainer!=winrt::guid{}?btBatteryLevel:-1;
        s.radioAllowed=accessAllowed;
        s.wifiBusy=change&&change->command.target==Control::Wifi;
        s.bluetoothBusy=change&&change->command.target==Control::Bluetooth;
    }
    void WifiRead(Controls& s){
        wifiState=(s.wifi==1||s.wifi==2)?ReadConnectedWifi(wlanClient):WifiConnection{};
        s.wifiLabel=WifiConnectionLabel(s.wifi,wifiState,RadioLabel(s.wifi));
    }
    void BtResetBattery(){
        if(btBattery){try{btBattery.Cancel();}catch(...){}btBattery=nullptr;}
        btBatteryAt=0;btBatteryReadAt=0;btBatteryFor={};btBatteryLevel=-1;
    }
    void BtReset(){
        if(btQuery){try{btQuery.Cancel();}catch(...){}btQuery=nullptr;}
        btQueryAt=0;btReadAt=0;btKnown=false;btTarget.clear();btSingleContainer={};
        BtResetBattery();
    }
    void StartBtQuery(uint64_t now){
        if(btQuery)return;
        try{
            btQuery=winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
                ConnectedBluetoothAqs,winrt::single_threaded_vector<winrt::hstring>({AepConnected,AepContainer}),
                winrt::Windows::Devices::Enumeration::DeviceInformationKind::AssociationEndpoint);
            btQueryAt=now;
        }catch(...){btQuery=nullptr;}
    }
    void StartBtBatteryQuery(uint64_t now){
        if(btBattery||btSingleContainer==winrt::guid{})return;
        try{
            btBattery=winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
                BluetoothBatteryAqs(btSingleContainer),
                winrt::single_threaded_vector<winrt::hstring>({BluetoothBatteryProperty}),
                winrt::Windows::Devices::Enumeration::DeviceInformationKind::Device);
            btBatteryAt=now;btBatteryFor=btSingleContainer;
        }catch(...){btBattery=nullptr;}
    }
    void EndChange(Controls& s,const wchar_t* error=nullptr){
        if(!change)return;auto& p=*change;
        for(auto& op:p.changes)try{op.Cancel();}catch(...){}
        if(p.command.access)try{p.command.access.Cancel();}catch(...){}
        s.completed[unsigned(p.command.target)]=p.command.serial;
        if(error)Error(s,error);change.reset();RadioRead(s);
    }
public:
    // The hotspot page: settings and clients of the manager the tile controls.
    // The passphrase goes to key for Metrics' own buffer; nothing here keeps it.
    void HotspotDetails(Controls& s,std::wstring& key){
        using winrt::Windows::Networking::HostNameType;
        auto& h=s.hotspotInfo;
        h.ready=true;h.ssid.clear();h.band=-1;h.keyLength=0;h.bands={};h.clients.clear();h.maxClients=0;h.source=tetherProfile;
        if(!tether)return;
        try{
            h.maxClients=tether.MaxClientCount();
            auto config=tether.GetCurrentAccessPointConfiguration();
            h.ssid=config.Ssid();h.band=int(config.Band());
            for(int band=0;band<3;++band)try{h.bands[band]=config.IsBandSupported(winrt::Windows::Networking::NetworkOperators::TetheringWiFiBand(band));}catch(...){}
            {const auto pass=config.Passphrase();key.assign(pass.data(),pass.size());h.keyLength=pass.size();}
            if(tether.TetheringOperationalState()==TetherState::On)for(auto const& client:tether.GetTetheringClients()){
                HotspotClient c;std::wstring ip;
                for(auto const& host:client.HostNames()){
                    if(host.Type()==HostNameType::DomainName&&c.name.empty()){
                        c.name=host.DisplayName();if(auto dot=c.name.find(L'.');dot!=std::wstring::npos&&dot>0)c.name.resize(dot);}
                    else if(host.Type()==HostNameType::Ipv4&&ip.empty())ip=host.DisplayName();
                }
                c.address=ip.empty()?std::wstring(client.MacAddress()):ip;
                h.clients.push_back(std::move(c));
            }
        }catch(...){Log(L"hotspot-details-failed",winrt::to_hresult());}
    }
    // Settings change only while the hotspot is off; the page offers no edit
    // otherwise. The key is wiped once WinRT has its copy.
    void HotspotConfigure(ControlCommand& c,Controls& s){
        using winrt::Windows::Networking::NetworkOperators::TetheringWiFiBand;
        auto wipe=[&]{SecureZeroMemory(c.secret.data(),c.secret.size()*sizeof(wchar_t));c.secret.clear();};
        auto& h=s.hotspotInfo;
        if(configure||tetherCommand){wipe();s.completed[unsigned(c.target)]=c.serial;return;}
        try{
            if(!tether||tether.TetheringOperationalState()!=TetherState::Off)winrt::throw_hresult(E_NOT_VALID_STATE);
            auto config=tether.GetCurrentAccessPointConfiguration();
            config.Ssid(c.key);if(!c.secret.empty())config.Passphrase(c.secret);wipe();
            if(c.value>=0&&c.value<3)config.Band(TetheringWiFiBand(c.value));
            configure=tether.ConfigureAccessPointAsync(config);configureAt=GetTickCount64();configureSerial=c.serial;
            h.saving=true;h.failure.clear();
        }catch(...){
            wipe();Log(L"hotspot-configure-failed",winrt::to_hresult());
            h.saving=false;h.failure=L"现在不能修改热点设置";s.completed[unsigned(c.target)]=c.serial;
        }
    }
    void RefreshTargets(Controls& s){
        RadioRead(s);
        if(s.wifi==1||s.wifi==2)WifiRead(s);
        if(s.bluetooth==1||s.bluetooth==2){
            if(btQuery){try{btQuery.Cancel();}catch(...){}btQuery=nullptr;}
            btReadAt=0;btBatteryReadAt=0;StartBtQuery(GetTickCount64());
        }
    }
    void Open(){
        theme.Refresh();
        if(enumeration)try{enumeration.Cancel();}catch(...){}enumeration=nullptr;radios.clear();
        wifiState={};BtReset();
        // Fresh snapshot per panel opening, not once per second.
        try{enumeration=Radio::GetRadiosAsync();enumAt=GetTickCount64();}catch(...){}
        HotspotOpen();
    }
    bool Pending()const{return bool(enumeration)||bool(change)||bool(tetherCommand)||bool(configure)||bool(btQuery)||bool(btBattery)||theme.Pending()||brightnessPacer.Active()||brightnessSettle.Active();}
    void Pump(Controls& s){
        BrightnessPump(s);
        if(theme.Pending())ThemePump(s);
        HotspotPump(s);ConfigurePump(s);
        bool enumFinished=false;
        if(enumeration)try{auto status=enumeration.Status();
            if(status==AsyncStatus::Completed){
                auto list=enumeration.GetResults();radios.assign(list.begin(),list.end());
                enumeration=nullptr;enumFinished=true;
            }else if(status!=AsyncStatus::Started||GetTickCount64()-enumAt>3000){
                enumeration.Cancel();enumeration=nullptr;enumFinished=true;
            }
        }catch(...){enumeration=nullptr;radios.clear();enumFinished=true;}
        const int prevWifi=s.wifi,prevBt=s.bluetooth;
        if(change)try{
            auto& p=*change;
            if(GetTickCount64()-p.at>15000){EndChange(s,L"无线控制超时，请重试");return;}
            if(!p.started){
                if(!accessAllowed){
                    if(!p.command.access){EndChange(s,L"系统不允许更改无线状态");return;}
                    auto status=p.command.access.Status();if(status==AsyncStatus::Started){RadioRead(s);return;}
                    if(status!=AsyncStatus::Completed||p.command.access.GetResults()!=RadioAccess::Allowed){
                        EndChange(s,L"无线控制未获系统授权");return;}
                    accessAllowed=true;p.command.access=nullptr;
                }
                auto kind=p.command.target==Control::Wifi?RadioKind::WiFi:RadioKind::Bluetooth;
                for(auto const& radio:radios)if(radio.Kind()==kind){auto state=radio.State();
                    if(state==RadioState::On||state==RadioState::Off)
                        p.changes.push_back(radio.SetStateAsync(p.command.value?RadioState::On:RadioState::Off));}
                if(p.changes.empty()){EndChange(s,L"无线设备不可控制");return;}
                p.started=true;p.at=GetTickCount64();
            }
            bool done=true;
            for(auto const& op:p.changes){auto status=op.Status();
                if(status==AsyncStatus::Started){done=false;continue;}
                if(status!=AsyncStatus::Completed||op.GetResults()!=RadioAccess::Allowed){EndChange(s,L"系统未能更改无线状态");return;}}
            if(done){RadioRead(s);const int state=p.command.target==Control::Wifi?s.wifi:s.bluetooth;
                if(state==p.command.value){++s.writes;EndChange(s);}
                else if(GetTickCount64()-p.at>3000)EndChange(s,L"设备状态未改变，请重试");}
        }catch(...){EndChange(s,L"无线设备操作失败");}
        RadioRead(s);
        if((enumFinished||s.wifi!=prevWifi)&&(s.wifi==1||s.wifi==2))WifiRead(s);
        else if(s.wifi!=1&&s.wifi!=2)wifiState={};
        const auto btNow=std::max<uint64_t>(GetTickCount64(),1);
        if(btQuery){
            AsyncStatus status=AsyncStatus::Error;
            try{status=btQuery.Status();}catch(...){}
            if(status==AsyncStatus::Completed){
                try{
                    std::vector<BluetoothTarget> items;
                    for(auto const& d:btQuery.GetResults()){
                        auto p=d.Properties();
                        if(!AepValue<bool>(p,AepConnected).value_or(false))continue;
                        items.push_back({AepValue<winrt::guid>(p,AepContainer).value_or(winrt::guid{}),
                            std::wstring(d.Id()),std::wstring(d.Name())});
                    }
                    const auto summary=SummarizeBluetoothTargetState(items);
                    btTarget=summary.label;btKnown=true;
                    if(summary.container!=btSingleContainer){
                        btSingleContainer=summary.container;
                        BtResetBattery();
                    }
                }catch(...){}
                btQuery=nullptr;btReadAt=btNow;
            }else if(status!=AsyncStatus::Started||btNow-btQueryAt>BtQueryLimit){
                if(status==AsyncStatus::Started)try{btQuery.Cancel();}catch(...){}
                btQuery=nullptr;btReadAt=btNow;
            }
        }
        if(btBattery){
            AsyncStatus status=AsyncStatus::Error;
            try{status=btBattery.Status();}catch(...){}
            if(status==AsyncStatus::Completed){
                try{
                    const int level=ReadBluetoothBattery(btBattery.GetResults());
                    if(btBatteryFor==btSingleContainer)btBatteryLevel=level;
                }catch(...){}
                btBattery=nullptr;btBatteryReadAt=btNow;
            }else if(status!=AsyncStatus::Started||btNow-btBatteryAt>BtQueryLimit){
                if(status==AsyncStatus::Started)try{btBattery.Cancel();}catch(...){}
                btBattery=nullptr;btBatteryReadAt=btNow;
            }
        }
        if(!enumeration){
            if(s.bluetooth!=1&&s.bluetooth!=2){
                BtReset();
            }else{
                if(!btQuery&&(enumFinished||s.bluetooth!=prevBt||btReadAt==0))StartBtQuery(btNow);
                if(btSingleContainer!=winrt::guid{}&&!btBattery&&btBatteryReadAt==0)StartBtBatteryQuery(btNow);
            }
        }
        RadioRead(s);
    }
    // full=false is the first round after opening: WMI is left for ReadBrightness.
    void Read(Controls& s,bool full=true){
        ++s.reads;AudioRead(s);if(full&&!brightnessPacer.Active()&&!brightnessSettle.Active())BrightnessPoll(s,false);ThemeRead(s);HotspotRead(s,true);Pump(s);
        if(s.wifi==1||s.wifi==2)WifiRead(s);
        if(!enumeration&&(s.bluetooth==1||s.bluetooth==2)){
            const auto now=std::max<uint64_t>(GetTickCount64(),1);
            if(!btQuery&&now-btReadAt>=BtReadEvery)StartBtQuery(now);
            if(btSingleContainer!=winrt::guid{}&&!btBattery&&(btBatteryReadAt==0||now-btBatteryReadAt>=BtBatteryEvery))
                StartBtBatteryQuery(now);
        }
        if(!s.error.empty()&&GetTickCount64()-s.errorAt>5000)s.error.clear();
    }
    void ReadBrightness(Controls& s){BrightnessPoll(s,true);}
    void Apply(ControlCommand command,Controls& s){
        if(command.target==Control::Hotspot){HotspotSet(std::move(command),s);return;}
        if(command.target==Control::DarkTheme){ThemeSet(command,s);return;}
        if(command.target==Control::Wifi||command.target==Control::Bluetooth){
            if(change||tetherCommand||configure){s.completed[unsigned(command.target)]=command.serial;return;}
            s.error.clear();change=RadioChange{std::move(command),GetTickCount64()};Pump(s);return;
        }
        if(command.target==Control::Brightness){
            const int target=std::clamp(command.value,0,100);
            brightnessSettle.Clear(s.completed[unsigned(Control::Brightness)]);
            const auto now=std::max<uint64_t>(GetTickCount64(),1);
            if(brightnessPacer.Request(command.serial,target,now))
                BrightnessDispatch(command.serial,target,s);
            return;
        }
        try{
            auto audio=Audio();
            if(command.target==Control::Mute)winrt::check_hresult(audio->SetMute(command.value!=0,nullptr));
            else{winrt::check_hresult(audio->SetMasterVolumeLevelScalar(std::clamp(command.value,0,100)/100.f,nullptr));
                if(command.value>0)winrt::check_hresult(audio->SetMute(FALSE,nullptr));}
            ++s.writes;s.error.clear();
        }catch(...){AudioReset();Error(s,L"音频设备未能应用更改");}
        s.completed[unsigned(command.target)]=command.serial;
        AudioRead(s);
    }
    void Close(Controls& s,bool flush=true){
        if(enumeration)try{enumeration.Cancel();}catch(...){}enumeration=nullptr;
        BtReset();s.bluetoothBattery=-1;
        if(wlanClient){WlanCloseHandle(wlanClient,nullptr);wlanClient=nullptr;}
        wifiState={};
        if(flush&&brightnessPacer.Active()){
            const auto waitMs=std::min<uint64_t>(brightnessPacer.Remaining(std::max<uint64_t>(GetTickCount64(),1)),BrightnessPacer::PaceMs);
            if(waitMs>0)Sleep(DWORD(waitMs));
            uint64_t dueSerial=0;int dueTarget=-1;
            if(brightnessPacer.TakeDue(std::max<uint64_t>(GetTickCount64(),1),dueSerial,dueTarget))
                BrightnessDispatch(dueSerial,dueTarget,s);
        }
        brightnessPacer.Clear(s.completed[unsigned(Control::Brightness)]);
        brightnessSettle.Clear(s.completed[unsigned(Control::Brightness)]);
        EndChange(s);radios.clear();wmi=nullptr;brightnessInstance.clear();brightnessAt=0;AudioReset();
        // Cancel our wait only. Never stop the user's active hotspot on hide/unload;
        // a settings change already sent finishes in the system without us.
        EndHotspot(s);tether=nullptr;tetherProfile.clear();s.hotspotBusy=false;
        if(configure){s.completed[unsigned(Control::HotspotConfigure)]=configureSerial;configure=nullptr;}
        s.hotspotInfo.saving=false;
    }
    ~ControlBackend(){Controls ignored;Close(ignored,false);}
};
}
