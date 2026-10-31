#pragma once
#include "platform-latest.h"
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <functiondiscoverykeys_devpkey.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <algorithm>
#include <optional>

namespace Capsule {
enum class BluetoothKind : int { Other, Headphones, Speaker, Mouse, Keyboard, Gamepad, Phone, Computer };
struct BluetoothDevice {
    std::wstring id,name;winrt::guid container{};
    BluetoothKind kind=BluetoothKind::Other;int battery=-1;
    bool connected=false,audio=false; // audio: has Bluetooth audio endpoints the page can connect
};
// What the Bluetooth page shows. Read only while that page is open.
struct BluetoothList {
    std::vector<BluetoothDevice> devices;
    std::wstring busy,failed,failure; // busy: the device being connected or disconnected
    bool ready=false,connecting=false; // connecting: what busy is doing
};
// Class of Device for classic devices (Assigned Numbers 2.8: major, then
// the six minor bits) and GAP Appearance for LE ones (2.6).
inline BluetoothKind ClassicKind(unsigned major,unsigned minor){
    switch(major){
    case 1:return BluetoothKind::Computer;
    case 2:return BluetoothKind::Phone;
    case 4:return minor==1||minor==2||minor==6?BluetoothKind::Headphones:BluetoothKind::Speaker;
    case 5:if(minor&0x10)return BluetoothKind::Keyboard;if(minor&0x20)return BluetoothKind::Mouse;
        return (minor&0xF)==1||(minor&0xF)==2?BluetoothKind::Gamepad:BluetoothKind::Other;
    default:return BluetoothKind::Other;
    }
}
inline BluetoothKind LeKind(unsigned category,unsigned subcategory){
    switch(category){
    case 0x001:return BluetoothKind::Phone;
    case 0x002:return BluetoothKind::Computer;
    case 0x00F:return subcategory==1?BluetoothKind::Keyboard:subcategory==2?BluetoothKind::Mouse:
        subcategory==3||subcategory==4?BluetoothKind::Gamepad:BluetoothKind::Other;
    case 0x021:return BluetoothKind::Speaker;
    default:return BluetoothKind::Other;
    }
}
// Connected first, then by name, so rows do not jump between reads.
inline void SortDevices(std::vector<BluetoothDevice>& list){
    std::sort(list.begin(),list.end(),[](BluetoothDevice const& a,BluetoothDevice const& b){
        if(a.connected!=b.connected)return a.connected;
        return a.name<b.name;
    });
}

// Paired devices come from the association endpoints Windows keeps for
// them: no inquiry and no radio traffic. Belongs to the Metrics MTA worker
// like WifiBackend; queries are polled and cancelled when the page closes.
class BluetoothBackend {
    using Collection=winrt::Windows::Devices::Enumeration::DeviceInformationCollection;
    using Query=winrt::Windows::Foundation::IAsyncOperation<Collection>;
    using AsyncStatus=winrt::Windows::Foundation::AsyncStatus;
    static constexpr uint64_t ReadEvery=2000,QueryLimit=5000,ChangeLimit=20000,BatteryEvery=30000;
    static constexpr wchar_t Connected[]=L"System.Devices.Aep.IsConnected",Container[]=L"System.Devices.Aep.ContainerId",
        Major[]=L"System.Devices.Aep.Bluetooth.Cod.Major",Minor[]=L"System.Devices.Aep.Bluetooth.Cod.Minor",
        Category[]=L"System.Devices.Aep.Bluetooth.Le.Appearance.Category",Subcategory[]=L"System.Devices.Aep.Bluetooth.Le.Appearance.Subcategory",
        // Battery level some drivers publish on the device nodes (shown by Settings).
        Battery[]=L"{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2";
    // Endpoints and KS filters behind them, from the audio service.
    static constexpr PROPERTYKEY FormFactor{{0x1da5d803,0xd492,0x4edd,{0x8c,0x23,0xe0,0xc0,0xff,0xee,0x7f,0x0e}},0};
    static constexpr GUID BtAudio{0x7fa06c40,0xb8f6,0x4c7e,{0x85,0x56,0xe8,0xc3,0x3a,0x12,0xe5,0x4d}}; // KSPROPSETID_BtAudio
    Query query{nullptr},battery{nullptr};
    uint64_t queryAt=0,readAt=0,batteryAt=0;winrt::guid batteryFor{};
    std::vector<std::pair<winrt::guid,std::pair<int,uint64_t>>> levels; // container -> level, read at
    struct Change {std::wstring id;bool connect=false;uint64_t at=0;};
    std::optional<Change> change;
    bool active=false;
    using Properties=winrt::Windows::Foundation::Collections::IMapView<winrt::hstring,winrt::Windows::Foundation::IInspectable>;
    template<class T> static std::optional<T> Value(Properties const& p,const wchar_t* key){
        auto v=p.TryLookup(key);if(!v)return std::nullopt;
        if(auto r=v.try_as<winrt::Windows::Foundation::IReference<T>>())return r.Value();return std::nullopt;
    }
    // Class-of-device and appearance fields arrive as 8-, 16- or 32-bit values.
    static int Number(Properties const& p,const wchar_t* key){
        if(auto v=Value<uint8_t>(p,key))return *v;
        if(auto v=Value<uint16_t>(p,key))return *v;
        if(auto v=Value<uint32_t>(p,key))return int(*v);
        return -1;
    }
    static winrt::com_ptr<IMMDeviceEnumerator> Endpoints(){
        winrt::com_ptr<IMMDeviceEnumerator> e;
        winrt::check_hresult(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,__uuidof(IMMDeviceEnumerator),e.put_void()));
        return e;
    }
    static std::optional<winrt::guid> EndpointContainer(IMMDevice* device,int* form=nullptr){
        winrt::com_ptr<IPropertyStore> store;if(FAILED(device->OpenPropertyStore(STGM_READ,store.put())))return std::nullopt;
        PROPVARIANT v;PropVariantInit(&v);std::optional<winrt::guid> id;
        if(SUCCEEDED(store->GetValue(PKEY_Device_ContainerId,&v))&&v.vt==VT_CLSID&&v.puuid)id=winrt::guid(*v.puuid);
        PropVariantClear(&v);
        if(form&&SUCCEEDED(store->GetValue(FormFactor,&v))&&v.vt==VT_UI4)*form=int(v.ulVal);
        PropVariantClear(&v);return id;
    }
    // The Bluetooth audio driver's KS filter behind an endpoint, if any.
    static winrt::com_ptr<IKsControl> Filter(IMMDevice* device,std::wstring& id){
        try{
            winrt::com_ptr<IDeviceTopology> topology;
            winrt::check_hresult(device->Activate(__uuidof(IDeviceTopology),CLSCTX_INPROC_SERVER,nullptr,topology.put_void()));
            winrt::com_ptr<IConnector> connector,to;winrt::check_hresult(topology->GetConnector(0,connector.put()));
            winrt::check_hresult(connector->GetConnectedTo(to.put()));
            auto part=to.as<IPart>();winrt::com_ptr<IDeviceTopology> filter;winrt::check_hresult(part->GetTopologyObjects(filter.put()));
            LPWSTR raw=nullptr;winrt::check_hresult(filter->GetDeviceId(&raw));id=raw;CoTaskMemFree(raw);
            winrt::com_ptr<IKsControl> ks;
            if(SUCCEEDED(part->Activate(CLSCTX_INPROC_SERVER,__uuidof(IKsControl),ks.put_void())))return ks;
        }catch(...){}
        return nullptr;
    }
    template<class F> static void EachEndpoint(F&& f){
        auto e=Endpoints();winrt::com_ptr<IMMDeviceCollection> all;
        winrt::check_hresult(e->EnumAudioEndpoints(eAll,DEVICE_STATE_ACTIVE|DEVICE_STATE_UNPLUGGED,all.put()));
        UINT n=0;winrt::check_hresult(all->GetCount(&n));
        for(UINT i=0;i<n;++i){winrt::com_ptr<IMMDevice> d;if(SUCCEEDED(all->Item(i,d.put())))f(d.get());}
    }
    // Containers of paired devices that have Bluetooth audio endpoints, with
    // the endpoint's form factor (headphones, speakers...) for the icon.
    static void MarkAudio(std::vector<BluetoothDevice>& list){
        try{EachEndpoint([&](IMMDevice* d){
            int form=-1;const auto id=EndpointContainer(d,&form);if(!id)return;
            auto device=std::find_if(list.begin(),list.end(),[&](BluetoothDevice const& b){return b.container==*id;});
            if(device==list.end()||device->audio)return;
            std::wstring filter;if(!Filter(d,filter))return;
            device->audio=true;
            if(form==3||form==5)device->kind=BluetoothKind::Headphones;      // Headphones, Headset
            else if(form==1&&device->kind==BluetoothKind::Other)device->kind=BluetoothKind::Speaker;
        });}catch(...){}
    }
    // Bluetooth audio drivers connect and disconnect on request through a
    // one-shot KS property of the filter (ToothTray does the same). Returns
    // how many filters accepted it; A2DP and hands-free each have one.
    static int Oneshot(winrt::guid const& container,bool connect){
        int sent=0;std::vector<std::wstring> done;
        EachEndpoint([&](IMMDevice* d){
            const auto id=EndpointContainer(d);if(!id||*id!=container)return;
            std::wstring filter;auto ks=Filter(d,filter);
            if(!ks||std::find(done.begin(),done.end(),filter)!=done.end())return;
            done.push_back(filter);
            KSPROPERTY property{};property.Set=BtAudio;property.Id=connect?0:1;property.Flags=1; // ONESHOT_RECONNECT / _DISCONNECT, GET
            ULONG bytes=0;if(SUCCEEDED(ks->KsProperty(&property,sizeof(property),nullptr,0,&bytes)))++sent;
        });
        return sent;
    }
    void Read(BluetoothList& b,Collection const& found,uint64_t now){
        std::vector<BluetoothDevice> list;
        for(auto const& info:found){
            auto const p=info.Properties();
            BluetoothDevice d;d.id=info.Id();d.name=info.Name();if(d.name.empty())continue;
            d.connected=Value<bool>(p,Connected).value_or(false);
            if(auto c=Value<winrt::guid>(p,Container))d.container=*c;
            const int major=Number(p,Major),minor=Number(p,Minor),category=Number(p,Category),sub=Number(p,Subcategory);
            d.kind=major>=0?ClassicKind(unsigned(major),unsigned(std::max(0,minor))):
                category>=0?LeKind(unsigned(category),unsigned(std::max(0,sub))):BluetoothKind::Other;
            for(auto const& [c,level]:levels)if(c==d.container&&d.connected)d.battery=level.first;
            list.push_back(std::move(d));
        }
        MarkAudio(list);
        // Rows keep their places while the page is open, as on the Wi-Fi page.
        SortDevices(list);
        auto rank=[&](BluetoothDevice const& d){
            const auto at=std::find_if(b.devices.begin(),b.devices.end(),[&](BluetoothDevice const& o){return o.id==d.id;});
            return size_t(at-b.devices.begin());
        };
        std::stable_sort(list.begin(),list.end(),[&](BluetoothDevice const& a,BluetoothDevice const& c){return rank(a)<rank(c);});
        b.devices=std::move(list);b.ready=true;readAt=now;
    }
    void Fail(BluetoothList& b,std::wstring const& id,const wchar_t* text){
        b.busy.clear();b.failed=id;b.failure=text;change.reset();
    }
    // One connected device's battery, one query at a time, each at most
    // every 30 s while the page is open.
    void BatteryPump(BluetoothList const& b,uint64_t now){
        using namespace winrt::Windows::Devices::Enumeration;
        if(battery){
            const auto status=battery.Status();
            if(status==AsyncStatus::Started&&now-batteryAt<QueryLimit)return;
            int level=-1;
            if(status==AsyncStatus::Completed){
                for(auto const& node:battery.GetResults())
                    if(auto v=Value<uint8_t>(node.Properties(),Battery);v&&*v<=100)level=std::max(level,int(*v));
            }else if(status==AsyncStatus::Started)battery.Cancel();
            battery=nullptr;
            auto at=std::find_if(levels.begin(),levels.end(),[&](auto const& l){return l.first==batteryFor;});
            if(at==levels.end())levels.push_back({batteryFor,{level,now}});else at->second={level,now};
        }
        for(auto const& d:b.devices){
            if(!d.connected||d.container==winrt::guid{})continue;
            auto at=std::find_if(levels.begin(),levels.end(),[&](auto const& l){return l.first==d.container;});
            if(at!=levels.end()&&now-at->second.second<BatteryEvery)continue;
            const auto aqs=L"System.Devices.ContainerId:=\""+std::wstring(winrt::to_hstring(d.container))+L"\"";
            battery=DeviceInformation::FindAllAsync(aqs,winrt::single_threaded_vector<winrt::hstring>({Battery}),DeviceInformationKind::Device);
            batteryAt=now;batteryFor=d.container;return;
        }
    }
public:
    bool Active()const{return active;}
    bool Pending()const{return active&&(query||battery||change);}
    void Open(){active=true;readAt=0;}
    // Returns true when the page should repaint.
    bool Pump(BluetoothList& b,uint64_t& writes){
        using namespace winrt::Windows::Devices::Enumeration;
        const auto now=GetTickCount64();bool changed=false;
        try{
            if(query){
                const auto status=query.Status();
                if(status==AsyncStatus::Started&&now-queryAt<QueryLimit)return false;
                if(status==AsyncStatus::Completed){Read(b,query.GetResults(),now);changed=true;}
                else{if(status==AsyncStatus::Started)query.Cancel();readAt=now;}
                query=nullptr;
                if(change)if(auto d=std::find_if(b.devices.begin(),b.devices.end(),[&](auto const& d){return d.id==change->id;});
                    d!=b.devices.end()&&d->connected==change->connect){++writes;b.busy.clear();change.reset();}
            }
            if(change&&now-change->at>ChangeLimit){Fail(b,change->id,change->connect?L"没有连上，请确认设备已开机且在附近":L"设备没有断开，请重试");changed=true;}
            if(!query&&(!readAt||now-readAt>=(change?500:ReadEvery))){
                // Paired devices of both kinds: classic (BR/EDR) and LE.
                const wchar_t* aqs=L"System.Devices.Aep.IsPaired:=System.StructuredQueryType.Boolean#True AND "
                    L"(System.Devices.Aep.ProtocolId:=\"{e0cbf06c-cd8b-4647-bb8a-263b43f0f974}\" OR "
                    L"System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\")";
                query=DeviceInformation::FindAllAsync(aqs,winrt::single_threaded_vector<winrt::hstring>(
                    {Connected,Container,Major,Minor,Category,Subcategory}),DeviceInformationKind::AssociationEndpoint);
                queryAt=now;
            }
            if(b.ready)BatteryPump(b,now);
        }catch(...){
            Log(L"bluetooth-page-error",winrt::to_hresult());
            if(query)try{query.Cancel();}catch(...){}query=nullptr;readAt=now;
            if(battery)try{battery.Cancel();}catch(...){}battery=nullptr;
            if(change)Fail(b,change->id,L"蓝牙设备操作失败");
            if(!b.ready){b.ready=true;changed=true;}
        }
        return changed;
    }
    // Only audio devices can be connected from here; Windows reconnects
    // input devices by itself when they wake.
    void Connect(std::wstring const& id,bool connect,BluetoothList& b){
        if(change)return;
        auto d=std::find_if(b.devices.begin(),b.devices.end(),[&](auto const& d){return d.id==id;});
        if(d==b.devices.end()||!d->audio){Fail(b,id,L"这个设备不能在这里连接");return;}
        b.failed.clear();b.failure.clear();
        try{if(!Oneshot(d->container,connect)){Fail(b,id,L"设备驱动没有接受请求");return;}}
        catch(...){Fail(b,id,L"蓝牙设备操作失败");return;}
        change=Change{id,connect,GetTickCount64()};b.busy=id;b.connecting=connect;readAt=0;
    }
    void Close(BluetoothList& b){
        if(query)try{query.Cancel();}catch(...){}
        if(battery)try{battery.Cancel();}catch(...){}
        query=nullptr;battery=nullptr;change.reset();levels.clear();active=false;readAt=0;b=BluetoothList{};
    }
    ~BluetoothBackend(){BluetoothList ignored;Close(ignored);}
};
}
