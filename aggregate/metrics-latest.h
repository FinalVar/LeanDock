#pragma once
#include "platform-latest.h"
#include "controls-latest.h"
#include "metrics-policy-latest.h"
#include "gpu-latest.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Devices.Power.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>
#include <map>
#include <optional>
#include <set>

namespace Capsule {
inline std::wstring Fixed(double v,int digits=1) {
    wchar_t b[64]{};swprintf_s(b,L"%.*f",digits,v);return b;
}
inline uint64_t Ticks(FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
inline std::wstring Rate(double v) {return v>=1024*1024?Fixed(v/(1024*1024))+L" MB/s":Fixed(v/1024,0)+L" KB/s";}
inline std::wstring Lunar(SYSTEMTIME local) {
    // ICU shipped with Windows, loaded only from System32. Read calendar
    // fields rather than locale-dependent month spellings.
    static HMODULE library=LoadLibraryExW(L"icu.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!library)return L"农历—";
    using Open=void*(__cdecl*)(const wchar_t*,int32_t,const char*,int32_t,int32_t*);
    using Close=void(__cdecl*)(void*);
    using Set=void(__cdecl*)(void*,double,int32_t*);
    using Get=int32_t(__cdecl*)(const void*,int32_t,int32_t*);
    auto open=reinterpret_cast<Open>(GetProcAddress(library,"ucal_open"));
    auto close=reinterpret_cast<Close>(GetProcAddress(library,"ucal_close"));
    auto set=reinterpret_cast<Set>(GetProcAddress(library,"ucal_setMillis"));
    auto get=reinterpret_cast<Get>(GetProcAddress(library,"ucal_get"));
    if(!open||!close||!set||!get)return L"农历—";
    SYSTEMTIME utc{};FILETIME file{};
    if(!TzSpecificLocalTimeToSystemTime(nullptr,&local,&utc)||!SystemTimeToFileTime(&utc,&file))return L"农历—";
    int32_t error=0;void* calendar=open(nullptr,0,"zh_CN@calendar=chinese",0,&error);
    if(!calendar||error>0){if(calendar)close(calendar);return L"农历—";}
    set(calendar,double(Ticks(file)/10000)-11644473600000.0,&error);
    int m=get(calendar,2,&error),d=get(calendar,5,&error),leap=get(calendar,22,&error);
    close(calendar);
    if(error>0||m<0||m>11||d<1||d>30)return L"农历—";
    static const wchar_t* months[]={L"正月",L"二月",L"三月",L"四月",L"五月",L"六月",
        L"七月",L"八月",L"九月",L"十月",L"冬月",L"腊月"};
    static const wchar_t* days[]={L"",L"初一",L"初二",L"初三",L"初四",L"初五",L"初六",L"初七",
        L"初八",L"初九",L"初十",L"十一",L"十二",L"十三",L"十四",L"十五",L"十六",L"十七",
        L"十八",L"十九",L"二十",L"廿一",L"廿二",L"廿三",L"廿四",L"廿五",L"廿六",L"廿七",
        L"廿八",L"廿九",L"三十"};
    return std::wstring(L"农历")+(leap?L"闰":L"")+months[m]+days[d];
}
struct Power {
    SYSTEM_POWER_STATUS value{};
    bool valid=false;
    void Read(){valid=GetSystemPowerStatus(&value)!=FALSE;}
    bool Present()const{return valid&&!(value.BatteryFlag&128)&&value.BatteryLifePercent<=100;}
    bool AC()const{return valid&&value.ACLineStatus==1;}
    std::wstring Percent()const{return Present()?std::to_wstring(value.BatteryLifePercent)+L"%":L"";}
};
struct Sample : Controls {
    int cpu=-1,memory=-1;
    std::deque<CpuPoint> history;
    std::vector<GpuSample> gpus;
    std::wstring capacity=L"—",temperature=L"CPU —　GPU —",network=L"↓ —　↑ —";
    std::wstring disks=L"—",battery=L"正在估算",media=L"没有在播放";
    uint64_t at=0,detailReads=0,diskReads=0;
};
class Metrics {
    std::jthread worker;
    std::mutex mutex,waitMutex;
    std::condition_variable_any wake;
    Sample data;
    std::atomic<bool> visible=false;
    ControlQueue commands;
    bool changed=false,accepting=true,refreshControls=false;
    uint64_t serial=0;
    std::atomic<DWORD> workerThread=0;
    HWND notify{};
    uint64_t idle=0,kernel=0,user=0;
    bool cpuReady=false;
    std::atomic<bool> urgent=false;
    uint64_t adaptersAt=0,disksAt=0;
    std::vector<Kmt::Adapter> gpuCatalogue;
    std::map<std::pair<uint64_t,ULONG>,int64_t> gpuPrevious;
    LARGE_INTEGER gpuAt{};
    std::map<uint64_t,std::pair<uint64_t,uint64_t>> networkPrevious;
    uint64_t networkAt=0;
    using Manager=winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;
    using Media=winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionMediaProperties;
    winrt::Windows::Foundation::IAsyncOperation<Manager> request{nullptr};
    winrt::Windows::Foundation::IAsyncOperation<Media> metadata{nullptr};
    Manager manager{nullptr};
    std::wstring mediaIdentity;
    uint64_t requestAt=0,metadataAt=0;
    void MediaRead(Sample& s) {
        using namespace winrt::Windows::Media::Control;
        using winrt::Windows::Foundation::AsyncStatus;
        const auto now=GetTickCount64();
        try {
            if(!manager&&!request){request=Manager::RequestAsync();requestAt=now;}
            if(request){
                auto status=request.Status();
                if(status==AsyncStatus::Completed){manager=request.GetResults();request=nullptr;}
                else if(status!=AsyncStatus::Started||now-requestAt>4000){request.Cancel();request=nullptr;}
            }
            if(!manager)return;
            auto current=manager.GetCurrentSession();
            if(!current){if(metadata)metadata.Cancel();metadata=nullptr;s.media=L"没有在播放";return;}
            const std::wstring identity(current.SourceAppUserModelId());
            if(identity!=mediaIdentity){
                if(metadata)metadata.Cancel();metadata=nullptr;mediaIdentity=identity;
                s.media=L"没有在播放";
            }
            auto status=current.GetPlaybackInfo().PlaybackStatus();
            if(status==GlobalSystemMediaTransportControlsSessionPlaybackStatus::Closed||
               status==GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped){
                if(metadata)metadata.Cancel();metadata=nullptr;s.media=L"没有在播放";return;
            }
            if(metadata){
                auto st=metadata.Status();
                if(st==AsyncStatus::Completed){
                    auto m=metadata.GetResults();metadata=nullptr;
                    s.media=status==GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing?L"▶ ":L"⏸ ";
                    s.media+=m.Title();
                    if(!m.Artist().empty())s.media+=L" — "+std::wstring(m.Artist());
                }else if(st!=AsyncStatus::Started||now-metadataAt>3000){metadata.Cancel();metadata=nullptr;}
            }
            if(!metadata){metadata=current.TryGetMediaPropertiesAsync();metadataAt=now;}
        }catch(...){if(metadata)try{metadata.Cancel();}catch(...){}metadata=nullptr;}
    }
    void Gpu(Sample& s) {
        const auto now=GetTickCount64();
        if(!adaptersAt||now-adaptersAt>=10000){adaptersAt=now;gpuCatalogue=Kmt::RenderAdapters();}
        LARGE_INTEGER at{},frequency{};QueryPerformanceCounter(&at);QueryPerformanceFrequency(&frequency);
        const double elapsed=gpuAt.QuadPart&&frequency.QuadPart?double(at.QuadPart-gpuAt.QuadPart)*1e7/double(frequency.QuadPart):0;
        std::map<std::pair<uint64_t,ULONG>,int64_t> next;s.gpus.clear();
        for(auto const& adapter:gpuCatalogue){
            GpuBusy busy;
            for(auto node:adapter.nodes){
                int64_t running=0;if(!Kmt::NodeRunning(adapter.luid,node,running))continue;
                const std::pair key{adapter.luid,node};next[key]=running;
                if(auto p=gpuPrevious.find(key);p!=gpuPrevious.end())busy.Add(NodeBusy(p->second,running,elapsed));
            }
            s.gpus.push_back({adapter.luid,adapter.name,busy.Percent()});
        }
        gpuPrevious=std::move(next);gpuAt=at;
    }
    void Network(Sample& s) {
        PMIB_IF_TABLE2 table=nullptr;const auto now=GetTickCount64();
        if(GetIfTable2(&table)!=NO_ERROR)return;
        std::map<uint64_t,std::pair<uint64_t,uint64_t>> next;double down=0,up=0;
        for(ULONG i=0;i<table->NumEntries;i++){
            const auto& row=table->Table[i];
            if(!row.InterfaceAndOperStatusFlags.HardwareInterface||row.OperStatus!=IfOperStatusUp)continue;
            uint64_t id=row.InterfaceLuid.Value;next[id]={row.InOctets,row.OutOctets};
            auto p=networkPrevious.find(id);
            if(p!=networkPrevious.end()&&row.InOctets>=p->second.first&&row.OutOctets>=p->second.second){
                down+=double(row.InOctets-p->second.first);up+=double(row.OutOctets-p->second.second);
            }
        }
        FreeMibTable(table);
        if(networkAt&&now>networkAt)s.network=L"↓ "+Rate(down*1000/(now-networkAt))+L"　↑ "+Rate(up*1000/(now-networkAt));
        networkPrevious=std::move(next);networkAt=now;
    }
    void Details(Sample& s) {
        ++s.detailReads;
        MEMORYSTATUSEX mem{sizeof(mem)};
        if(GlobalMemoryStatusEx(&mem)){
            s.memory=mem.dwMemoryLoad;s.capacity=Fixed(double(mem.ullTotalPhys-mem.ullAvailPhys)/1073741824)+
                L" / "+Fixed(double(mem.ullTotalPhys)/1073741824)+L" GB";
        }
        Gpu(s);
        Network(s);
        const auto now=GetTickCount64();
        if(!disksAt||now-disksAt>=10000){
        disksAt=now;++s.diskReads;s.disks.clear();
        DWORD mask=GetLogicalDrives();
        for(int i=0;i<26;i++)if(mask&(1<<i)){
            wchar_t drive[]{wchar_t(L'A'+i),L':',L'\\',0};
            if(GetDriveTypeW(drive)!=DRIVE_FIXED)continue;
            ULARGE_INTEGER available{};
            if(GetDiskFreeSpaceExW(drive,&available,nullptr,nullptr)){
                double gb=double(available.QuadPart)/1073741824;
                if(!s.disks.empty())s.disks+=L"　";
                s.disks+=wchar_t(L'A'+i);s.disks+=L" ";
                s.disks+=gb>=1000?Fixed(gb/1024)+L" TB":Fixed(gb,0)+L" GB";
            }
        }
        if(s.disks.empty())s.disks=L"—";
        }
        Power p;p.Read();s.battery=L"正在估算";
        if(!p.Present())s.battery=L"未提供电池信息";
        else if(p.AC()){
            s.battery=p.value.BatteryLifePercent==100?L"插电中，已充满":L"插电中";
            if(p.value.BatteryLifePercent<100)try{
                auto report=winrt::Windows::Devices::Power::Battery::AggregateBattery().GetReport();
                auto rate=report.ChargeRateInMilliwatts(),full=report.FullChargeCapacityInMilliwattHours(),
                    remaining=report.RemainingCapacityInMilliwattHours();
                if(rate&&full&&remaining&&rate.Value()>0&&full.Value()>remaining.Value()){
                    auto minutes=int(std::ceil((full.Value()-remaining.Value())*60.0/rate.Value()));
                    s.battery=L"插电中，约 "+std::to_wstring(minutes)+L" 分钟充满";
                }
            }catch(...){}
        }else if(p.value.BatteryLifeTime!=DWORD(-1)){
            unsigned minutes=(p.value.BatteryLifeTime+30)/60;
            s.battery=L"约 "+(minutes>=60?std::to_wstring(minutes/60)+L" 小时 ":L"")+
                std::to_wstring(minutes%60)+L" 分钟";
            if(minutes>=60)s.battery=s.battery.substr(0,s.battery.size()-1);
        }
        MediaRead(s);
    }
    // A hover panel is read in its first second. Rates (CPU, network, GPU)
    // need two samples, so the first interval after opening is shorter. The
    // reveal may cut it further, but never below RevealMs of real interval.
    static constexpr uint64_t WarmupMs=300,RevealMs=100;
    void Publish(Sample& s){
        s.at=GetTickCount64();{std::lock_guard lock(mutex);data=s;}
        if(visible&&notify)PostMessageW(notify,WM_APP+28,0,0);
    }
    void Run(std::stop_token stop) {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        workerThread=GetCurrentThreadId();CoEnableCallCancellation(nullptr);
        auto controls=std::make_unique<ControlBackend>();
        uint64_t nextSample=0,nextDetails=0,sampledAt=0,detailedAt=0;unsigned samples=0,details=0;bool opened=false,slow=false;
        while(!stop.stop_requested()){
            Sample s;{std::lock_guard lock(mutex);s=data;}
            std::vector<ControlCommand> pending;
            bool refresh=false;
            {std::lock_guard lock(waitMutex);changed=false;refresh=refreshControls;refreshControls=false;pending=commands.Take();}
            const auto now=GetTickCount64();const bool show=visible;
            if(refresh&&opened){controls->Close(s);opened=false;}
            if(show&&!opened){controls->Open();opened=true;nextDetails=nextSample=0;samples=details=0;cpuReady=false;s.history.clear();}
            // The panel became visible while warming up hidden: read the rates
            // over the standby interval now rather than at the warm-up tick.
            if(urgent.exchange(false)&&show){
                if(samples)nextSample=std::min(nextSample,std::max(now,sampledAt+RevealMs));
                if(details)nextDetails=std::min(nextDetails,std::max(now,detailedAt+RevealMs));
            }
            bool publish=false;
            // Commands wake this thread, but must not increase CPU/history or
            // network sampling frequency beyond one second after the warm-up.
            if(show&&now>=nextSample){nextSample=now+(samples++?1000:WarmupMs);sampledAt=now;
            FILETIME i{},k{},u{};
            if(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)<=64&&GetSystemTimes(&i,&k,&u)){
                auto ii=Ticks(i),kk=Ticks(k),uu=Ticks(u);
                auto total=kk-kernel+uu-user,off=ii-idle;
                if(cpuReady&&total&&off<=total){s.cpu=int(std::lround(100.0*(total-off)/total));AppendCpu(s.history,now,s.cpu);}
                idle=ii;kernel=kk;user=uu;cpuReady=true;
            }
            publish=true;}
            if(show&&now>=nextDetails){nextDetails=now+(details++?1000:WarmupMs);detailedAt=now;
                // WMI brightness waits until the first values are published.
                slow=details==1;
                try{Details(s);controls->Read(s,!slow);}catch(...){Log(L"detail-sample-error");}publish=true;}
            for(auto& command:pending){if(stop.stop_requested())break;controls->Apply(std::move(command),s);publish=true;}
            if(!stop.stop_requested()&&controls->Pending()){controls->Pump(s);publish=true;}
            if(!show&&opened){
                controls->Close(s);opened=false;publish=true;slow=false;
                networkPrevious.clear();networkAt=0;s.network=L"—";
                gpuPrevious.clear();gpuAt={};
                if(metadata){try{metadata.Cancel();}catch(...){}metadata=nullptr;}
                if(request){try{request.Cancel();}catch(...){}request=nullptr;}
            }
            if(publish)Publish(s);
            if(slow&&show&&!stop.stop_requested()){
                slow=false;
                try{controls->ReadBrightness(s);}catch(...){Log(L"detail-sample-error");}
                Publish(s);
            }
            auto delay=std::max<int64_t>(1,int64_t(nextSample)-int64_t(GetTickCount64()));
            if(show)delay=std::min(delay,std::max<int64_t>(1,int64_t(nextDetails)-int64_t(GetTickCount64())));
            if(controls->Pending())delay=std::min<int64_t>(delay,50);
            std::unique_lock waitLock(waitMutex);
            if(!show&&!controls->Pending())wake.wait(waitLock,stop,[&]{return changed||!commands.Empty();});
            else wake.wait_for(waitLock,stop,std::chrono::milliseconds(delay),[&]{return changed||!commands.Empty();});
        }
        controls.reset();
        if(metadata)try{metadata.Cancel();}catch(...){}
        if(request)try{request.Cancel();}catch(...){}
        metadata=nullptr;request=nullptr;manager=nullptr;
        workerThread=0;CoDisableCallCancellation(nullptr);
        winrt::uninit_apartment();
    }
public:
    void Start(HWND hwnd){notify=hwnd;worker=std::jthread([this](std::stop_token s){Run(s);});}
    void Visible(bool value){if(visible.exchange(value)!=value){std::lock_guard lock(waitMutex);changed=true;wake.notify_all();}}
    // Once per panel, when the warmed-up hidden window is revealed.
    void Urgent(){urgent=true;std::lock_guard lock(waitMutex);changed=true;wake.notify_all();}
    void RefreshControls(){std::lock_guard lock(waitMutex);refreshControls=changed=true;wake.notify_all();}
    // Called only from a genuine panel gesture. Consent may be shown by
    // Windows, so request it on the UI thread, never while probing/read-only.
    uint64_t Set(Control target,int value){
        if(!visible)return 0;
        AccessRequest access{nullptr};
        if(target==Control::Wifi||target==Control::Bluetooth){
            bool allowed;{std::lock_guard lock(mutex);allowed=data.radioAllowed;}
            if(!allowed)try{access=winrt::Windows::Devices::Radios::Radio::RequestAccessAsync();}catch(...){}
        }
        std::lock_guard lock(waitMutex);if(!accepting)return 0;
        auto id=++serial;commands.Put({target,value,id,std::move(access)});wake.notify_all();return id;
    }
    Sample Read(){std::lock_guard lock(mutex);return data;}
    void Stop(){if(worker.joinable()){
        {std::lock_guard lock(waitMutex);accepting=false;commands.Take();}
        worker.request_stop();wake.notify_all();
        if(auto id=workerThread.load())CoCancelCall(id,0);
        worker.join();}notify=nullptr;}
    ~Metrics(){Stop();}
};
}
