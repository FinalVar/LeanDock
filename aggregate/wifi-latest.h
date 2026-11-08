#pragma once
#include "platform-latest.h"
#include <string_view>
#include <wlanapi.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.WiFi.h>
#include <winrt/Windows.Networking.Connectivity.h>
#include <winrt/Windows.Security.Credentials.h>

namespace Capsule {
struct WifiNetwork {
    std::wstring ssid;int bars=0,rssi=-200;
    bool secure=false,enterprise=false,psk=false,saved=false,connected=false;
};
// What the Wi-Fi page shows. Read only while that page is open.
struct WifiList {
    std::vector<WifiNetwork> networks;
    std::wstring connecting,failed,failure,problem; // failed: the SSID failure belongs to
    bool ready=false,scanning=false,wrongPassword=false;
    bool unreadable=false; // the WLAN service has not given a list for a while; retried
};
// Grouped by SSID (one row per network, not per access point), connected
// first, then by coarse signal and name so rows do not jump between reads.
inline void SortNetworks(std::vector<WifiNetwork>& list){
    std::sort(list.begin(),list.end(),[](WifiNetwork const& a,WifiNetwork const& b){
        if(a.connected!=b.connected)return a.connected;
        if(a.bars!=b.bars)return a.bars>b.bars;
        return a.ssid<b.ssid;
    });
}
inline std::wstring WifiFailure(int status){
    switch(status){ // WiFiConnectionStatus
    case 2:return L"系统不允许连接这个网络";
    case 3:return L"密码不正确";
    case 4:return L"网络不在范围内";
    case 5:return L"连接超时";
    case 6:return L"不支持这个网络的认证方式";
    default:return L"无法连接到这个网络";
    }
}

// The list the last scan found, for the next time the page opens: the panel
// process lives for one hover, and between scans the WLAN service itself
// knows little more than the connected network (measured 2026-11-07: 1 access
// point before a scan, 16 after). Names and signal only; whether a network is
// connected is always read live. Damage loses the rest of the list, no more.
inline std::string PackNetworks(std::vector<WifiNetwork> const& list){
    std::string out;
    for(size_t i=0;i<list.size()&&i<64;++i){
        auto const& n=list[i];
        const size_t length=std::min<size_t>(n.ssid.size(),DOT11_SSID_MAX_LENGTH);
        out.push_back(char(length));
        out.push_back(char((n.secure?1:0)|(n.enterprise?2:0)|(n.psk?4:0)|(n.saved?8:0)));
        out.push_back(char(n.bars));out.push_back(char(int8_t(std::clamp(n.rssi,-128,0))));
        out.append(reinterpret_cast<const char*>(n.ssid.data()),length*sizeof(wchar_t));
    }
    return out;
}
inline std::vector<WifiNetwork> UnpackNetworks(std::string_view data){
    std::vector<WifiNetwork> list;
    while(data.size()>=4&&list.size()<64){
        const size_t length=uint8_t(data[0]),bytes=length*sizeof(wchar_t);
        const unsigned flags=uint8_t(data[1]),bars=uint8_t(data[2]);
        if(!length||length>DOT11_SSID_MAX_LENGTH||flags>15||bars>4||data.size()<4+bytes)break;
        WifiNetwork n;n.ssid.resize(length);memcpy(n.ssid.data(),data.data()+4,bytes);
        n.secure=flags&1;n.enterprise=flags&2;n.psk=flags&4;n.saved=flags&8;
        n.bars=int(bars);n.rssi=int8_t(data[3]);
        list.push_back(std::move(n));data.remove_prefix(4+bytes);
    }
    return list;
}

// Belongs to the Metrics MTA worker like ControlBackend: no callbacks, the
// asynchronous operations are polled and cancelled when the page closes.
// The page opens on the list the last scan saved, under what the WLAN service
// has now. A WinRT scan (about 2 s on 2026-11-07, 5-10 s on 2026-10-31)
// refreshes the service's list and replaces the saved rows when it ends. WinRT
// connects, since it builds profiles for every kind of network.
class WifiBackend {
    using Adapter=winrt::Windows::Devices::WiFi::WiFiAdapter;
    using Adapters=winrt::Windows::Foundation::Collections::IVectorView<Adapter>;
    using Access=winrt::Windows::Devices::WiFi::WiFiAccessStatus;
    using Result=winrt::Windows::Devices::WiFi::WiFiConnectionResult;
    using Credential=winrt::Windows::Security::Credentials::PasswordCredential;
    using AsyncStatus=winrt::Windows::Foundation::AsyncStatus;
    static constexpr uint64_t ScanLimit=30000,ScanEvery=15000,ConnectLimit=40000,ReadEvery=1000,ScanReadEvery=250;
    static constexpr DOT11_AUTH_ALGORITHM Wpa3Enterprise=DOT11_AUTH_ALGORITHM(11); // DOT11_AUTH_ALGO_WPA3_ENT, missing from MinGW
    winrt::Windows::Foundation::IAsyncOperation<Access> access{nullptr};
    winrt::Windows::Foundation::IAsyncOperation<Adapters> finding{nullptr};
    winrt::Windows::Foundation::IAsyncAction scan{nullptr};
    winrt::Windows::Foundation::IAsyncOperation<Result> connect{nullptr};
    Adapter adapter{nullptr};GUID id{};HANDLE client{};
    uint64_t startedAt=0,scanAt=0,scannedAt=0,readAt=0,connectAt=0;
    // A network the last WinRT report has not seen yet waits for the scan.
    std::wstring waiting;Credential waitingCredential{nullptr};
    bool active=false;
    std::vector<WifiNetwork> remembered;std::string savedList;
    unsigned readFailures=0;
    static std::wstring LastPath(){return Directory()+L"\\wifi-last-latest.bin";}
    void Recall(){
        remembered.clear();savedList.clear();
        FILE* f=_wfopen(LastPath().c_str(),L"rb");if(!f)return;
        char buffer[8192];const auto n=fread(buffer,1,sizeof buffer,f);fclose(f);
        savedList.assign(buffer,n);remembered=UnpackNetworks(savedList);
    }
    void Remember(std::vector<WifiNetwork> const& list){
        if(list.empty())return; // Says nothing about what is nearby.
        auto data=PackNetworks(list);if(data==savedList)return;
        FILE* f=_wfopen(LastPath().c_str(),L"wb");if(!f)return;
        fwrite(data.data(),1,data.size(),f);fclose(f);savedList=std::move(data);
    }
    static std::wstring Ssid(DOT11_SSID const& s){
        const int length=int(std::min<ULONG>(s.uSSIDLength,DOT11_SSID_MAX_LENGTH));
        const int n=MultiByteToWideChar(CP_UTF8,0,reinterpret_cast<const char*>(s.ucSSID),length,nullptr,0);
        std::wstring text(size_t(std::max(0,n)),L'\0');
        if(n>0)MultiByteToWideChar(CP_UTF8,0,reinterpret_cast<const char*>(s.ucSSID),length,text.data(),n);
        return text;
    }
    void Scan(uint64_t now){scanAt=now;try{scan=adapter.ScanAsync();}catch(...){scan=nullptr;scannedAt=now;}}
    // A fresh handle on the first interface. False, and no handle, without one.
    bool OpenClient(){
        if(client){WlanCloseHandle(client,nullptr);client=nullptr;}
        DWORD version=0;if(WlanOpenHandle(2,nullptr,&version,&client)!=ERROR_SUCCESS){client=nullptr;return false;}
        PWLAN_INTERFACE_INFO_LIST interfaces=nullptr;bool found=false;
        if(WlanEnumInterfaces(client,nullptr,&interfaces)==ERROR_SUCCESS&&interfaces){
            if(interfaces->dwNumberOfItems){id=interfaces->InterfaceInfo[0].InterfaceGuid;found=true;}
            WlanFreeMemory(interfaces);
        }
        if(!found){WlanCloseHandle(client,nullptr);client=nullptr;}
        return found;
    }
    void Read(WifiList& w,uint64_t now){
        readAt=now;if(!client)return;
        PWLAN_AVAILABLE_NETWORK_LIST report=nullptr;
        const DWORD error=WlanGetAvailableNetworkList(client,&id,0,nullptr,&report);
        if(error!=ERROR_SUCCESS||!report){
            // The service restarted or the adapter was reset: every third failed
            // read (about 3 s) takes a fresh handle. Without a list yet, say so
            // rather than looking for networks forever.
            if(++readFailures%3)return;
            if(readFailures==3)Log(L"wifi-read-failed",long(error));
            if(!w.ready)w.unreadable=true;
            OpenClient();return;
        }
        readFailures=0;w.unreadable=false;
        std::vector<WifiNetwork> list;
        for(DWORD i=0;i<report->dwNumberOfItems;++i){
            auto const& network=report->Network[i];
            auto ssid=Ssid(network.dot11Ssid);if(ssid.empty())continue; // Hidden networks need a name typed in Settings.
            const auto auth=network.dot11DefaultAuthAlgorithm;
            // One row per name: the service lists a saved profile and the
            // plain network separately.
            auto row=std::find_if(list.begin(),list.end(),[&](WifiNetwork const& n){return n.ssid==ssid;});
            if(row==list.end()){
                WifiNetwork n;n.ssid=std::move(ssid);
                n.enterprise=auth==DOT11_AUTH_ALGO_WPA||auth==DOT11_AUTH_ALGO_RSNA||auth==DOT11_AUTH_ALGO_WPA3||auth==Wpa3Enterprise;
                n.secure=network.bSecurityEnabled&&auth!=DOT11_AUTH_ALGO_OWE;n.psk=n.secure&&!n.enterprise;
                list.push_back(std::move(n));row=list.end()-1;
            }
            // Signal quality is linear from -100 dBm (0) to -50 dBm (100).
            const ULONG q=network.wlanSignalQuality;const int bars=q>=75?4:q>=50?3:q>=25?2:q>0?1:0;
            if(-100+int(q)/2>row->rssi){row->rssi=-100+int(q)/2;row->bars=bars;}
            row->saved|=(network.dwFlags&WLAN_AVAILABLE_NETWORK_HAS_PROFILE)!=0;
            row->connected|=(network.dwFlags&WLAN_AVAILABLE_NETWORK_CONNECTED)!=0;
        }
        WlanFreeMemory(report);
        for(auto const& n:remembered)
            if(std::none_of(list.begin(),list.end(),[&](WifiNetwork const& o){return o.ssid==n.ssid;}))list.push_back(n);
        // Rows keep their places while the page is open, so the one being
        // typed into never jumps; only joining moves a row to the top.
        SortNetworks(list);
        auto rank=[&](WifiNetwork const& n){
            const auto at=std::find_if(w.networks.begin(),w.networks.end(),[&](WifiNetwork const& o){return o.ssid==n.ssid;});
            return size_t(at-w.networks.begin());
        };
        std::stable_sort(list.begin(),list.end(),[&](WifiNetwork const& a,WifiNetwork const& b){
            return a.connected!=b.connected?a.connected:rank(a)<rank(b);});
        w.networks=std::move(list);w.ready=true;
    }
    winrt::Windows::Devices::WiFi::WiFiAvailableNetwork Find(std::wstring const& ssid){
        winrt::Windows::Devices::WiFi::WiFiAvailableNetwork best{nullptr};
        for(auto const& network:adapter.NetworkReport().AvailableNetworks())
            if(network.Ssid()==ssid&&(!best||network.NetworkRssiInDecibelMilliwatts()>best.NetworkRssiInDecibelMilliwatts()))best=network;
        return best;
    }
    void Fail(WifiList& w,std::wstring const& ssid,std::wstring text,bool password=false){
        w.connecting.clear();w.failed=ssid;w.failure=std::move(text);w.wrongPassword=password;
    }
    void Start(winrt::Windows::Devices::WiFi::WiFiAvailableNetwork const& network,Credential const& credential){
        using winrt::Windows::Devices::WiFi::WiFiReconnectionKind;
        connect=credential?adapter.ConnectAsync(network,WiFiReconnectionKind::Automatic,credential):
            adapter.ConnectAsync(network,WiFiReconnectionKind::Automatic);
        connectAt=GetTickCount64();
    }
    void Forget(){waiting.clear();waitingCredential=nullptr;}
public:
    bool Active()const{return active;}
    bool Pending()const{return active&&(access||finding||scan||connect);}
    void Open(){
        active=true;startedAt=GetTickCount64();Recall();readFailures=0;
        OpenClient();
        try{access=Adapter::RequestAccessAsync();}catch(...){access=nullptr;}
    }
    // Returns true when the page should repaint.
    bool Pump(WifiList& w,uint64_t& writes){
        const auto now=GetTickCount64();bool changed=false;
        try{
            if(!client&&w.problem.empty()){w.problem=L"没有找到无线网卡";return true;}
            if(!readAt){Read(w,now);changed=true;}
            if(access){
                const auto status=access.Status();
                if(status==AsyncStatus::Started&&now-startedAt<5000)return changed;
                const bool allowed=status==AsyncStatus::Completed&&access.GetResults()==Access::Allowed;
                access=nullptr;
                if(!allowed){w.problem=L"系统不允许读取 Wi-Fi 网络";Forget();return true;}
                finding=Adapter::FindAllAdaptersAsync();startedAt=now;
            }
            if(finding){
                const auto status=finding.Status();
                if(status==AsyncStatus::Started&&now-startedAt<5000)return changed;
                if(status==AsyncStatus::Completed)for(auto const& a:finding.GetResults()){
                    const winrt::guid adapterId=a.NetworkAdapter().NetworkAdapterId();
                    if(!memcmp(&adapterId,&id,sizeof(id))){adapter=a;break;}
                }
                finding=nullptr;
                if(!adapter){w.problem=L"没有找到无线网卡";Forget();return true;}
                Scan(now);
            }
            if(!adapter)return changed;
            if(scan){
                const auto status=scan.Status();
                if(status!=AsyncStatus::Started||now-scanAt>ScanLimit){
                    if(status==AsyncStatus::Started)scan.Cancel();
                    scan=nullptr;scannedAt=now;remembered.clear();Read(w,now);changed=true;
                    if(status==AsyncStatus::Completed)Remember(w.networks);
                    if(!waiting.empty()){
                        auto ssid=waiting;auto credential=waitingCredential;Forget();
                        if(auto network=Find(ssid))Start(network,credential);else Fail(w,ssid,L"网络不在范围内");
                    }
                }
            }
            if(connect){
                const auto status=connect.Status();
                if(status==AsyncStatus::Started&&now-connectAt>ConnectLimit){connect.Cancel();connect=nullptr;Fail(w,w.connecting,L"连接超时");changed=true;}
                else if(status!=AsyncStatus::Started){
                    const int result=status==AsyncStatus::Completed?int(connect.GetResults().ConnectionStatus()):0;
                    connect=nullptr;
                    if(result==1){++writes;w.connecting.clear();w.failed.clear();w.failure.clear();w.wrongPassword=false;}
                    else{Log(L"wifi-connect-failed",result);Fail(w,w.connecting,WifiFailure(result),result==3);}
                    Read(w,now);changed=true;
                }
            }
            // Networks arrive through a scan, so look more often during one.
            if(now-readAt>=(scan?ScanReadEvery:ReadEvery)){Read(w,now);changed=true;}
            if(!scan&&now-scannedAt>=ScanEvery)Scan(now);
            if(w.scanning!=bool(scan)){w.scanning=bool(scan);changed=true;}
        }catch(...){
            Log(L"wifi-page-error",winrt::to_hresult());
            if(connect){try{connect.Cancel();}catch(...){}connect=nullptr;}
            if(!w.connecting.empty())Fail(w,w.connecting,L"无法连接到这个网络");
            Forget();changed=true;
        }
        return changed;
    }
    // The password is used once and wiped; the credential copy lives only in
    // the WinRT object until Windows has stored the profile.
    void Connect(std::wstring const& ssid,std::wstring const& user,std::wstring& secret,WifiList& w){
        auto wipe=[&]{SecureZeroMemory(secret.data(),secret.size()*sizeof(wchar_t));secret.clear();};
        if(connect||!waiting.empty()){wipe();return;}
        if(!adapter&&!access&&!finding){wipe();Fail(w,ssid,L"无法连接到这个网络");return;}
        try{
            Credential credential{nullptr};
            if(!secret.empty()){credential=Credential();if(!user.empty())credential.UserName(user);credential.Password(secret);}
            wipe();
            w.connecting=ssid;w.failed.clear();w.failure.clear();w.wrongPassword=false;
            if(adapter)if(auto network=Find(ssid)){Start(network,credential);return;}
            // Not in WinRT's report yet (or the adapter is still being found):
            // connect when the scan that follows ends.
            waiting=ssid;waitingCredential=credential;if(adapter&&!scan)Scan(GetTickCount64());
        }catch(...){wipe();connect=nullptr;Forget();Fail(w,ssid,L"无法连接到这个网络");}
    }
    void Disconnect(uint64_t& writes,WifiList& w){
        if(!adapter)return;
        try{adapter.Disconnect();++writes;w.failed.clear();w.failure.clear();Read(w,GetTickCount64());}catch(...){Log(L"wifi-disconnect-failed",winrt::to_hresult());}
    }
    void Close(WifiList& w){
        if(access)try{access.Cancel();}catch(...){}
        if(finding)try{finding.Cancel();}catch(...){}
        if(scan)try{scan.Cancel();}catch(...){}
        if(connect)try{connect.Cancel();}catch(...){}
        access=nullptr;finding=nullptr;scan=nullptr;connect=nullptr;adapter=nullptr;Forget();remembered.clear();
        if(client){WlanCloseHandle(client,nullptr);client=nullptr;}
        active=false;scanAt=scannedAt=readAt=0;id={};w=WifiList{};
    }
    ~WifiBackend(){WifiList ignored;Close(ignored);}
};
}
