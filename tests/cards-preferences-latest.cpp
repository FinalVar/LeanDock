#include "cards-network-latest.h"
#include "cards-preferences-latest.h"
#include <array>
#include <cstdio>
#include <limits>
int wmain(int argc,wchar_t** argv){
    using namespace LeanDockCards;int count=0,failures=0;
    auto check=[&](bool ok,const char* name){++count;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
    PageOrder order;check(order.pages.front()==Page::Clock,"first use defaults to clock");
    check(order.Step(Page::Clock,1)==Page::Power&&order.Step(Page::Clock,-1)==Page::Network,"default order both directions");
    for(auto key:{L"",L"clock,",L",clock",L"clock,,media",L"clock,clock",L"CLOCK",L"metrics,nonsense",L"    "})
        check(!ParseOrder(key),"reject invalid order");
    check(!ParseOrder(std::wstring(129,L'x')),"reject oversized preference");
    auto spaced=ParseOrder(L" clock , metrics ");check(spaced&&spaced->Text()==L"clock,metrics","normalize supported whitespace");
    for(unsigned mask=1;mask<32;++mask){PageOrder subset;subset.pages.clear();
        for(unsigned i=0;i<5;++i)if(mask&(1u<<i))subset.pages.push_back(DefaultOrder[i]);
        auto parsed=ParseOrder(subset.Text());check(parsed&&parsed->pages==subset.pages,"all enabled subsets roundtrip");
        for(auto page:DefaultOrder){auto next=subset.Step(page,1);check(subset.Has(next),"disabled pages cannot be selected by wheel");
            check(subset.Has(subset.Select(page)),"removed last page falls back to enabled page");
            if(subset.Has(page))check(subset.Step(next,-1)==page,"backward reverses forward");}
    }
    // Model the real host: physical WLAN, Wi-Fi Direct, another physical
    // Ethernet port, TUN and an Ethernet-shaped WAN miniport. Large lifetime
    // counters on the ignored interfaces must never leak into the totals.
    auto adapter=[](uint64_t id,IFTYPE type,bool hardware,NDIS_PHYSICAL_MEDIUM medium){
        MIB_IF_ROW2 row{};row.InterfaceLuid.Value=id;row.Type=type;
        row.InterfaceAndOperStatusFlags.HardwareInterface=hardware;
        row.OperStatus=IfOperStatusUp;row.PhysicalMediumType=medium;return row;
    };
    auto wifi=adapter(1,IF_TYPE_IEEE80211,true,NdisPhysicalMediumNative802_11);
    auto hotspot=adapter(2,IF_TYPE_IEEE80211,false,NdisPhysicalMediumNative802_11);
    auto ethernet=adapter(3,IF_TYPE_ETHERNET_CSMACD,true,NdisPhysicalMedium802_3);
    auto tun=adapter(4,IF_TYPE_PROP_VIRTUAL,false,NdisPhysicalMediumUnspecified);
    auto wan=adapter(5,IF_TYPE_ETHERNET_CSMACD,false,NdisPhysicalMediumUnspecified);
    auto tap=wan;tap.PhysicalMediumType=NdisPhysicalMedium802_3;
    check(IncludeNetworkInterface(wifi)&&IncludeNetworkInterface(ethernet),"physical WLAN and Ethernet stay included");
    check(IncludeNetworkInterface(hotspot),"active virtual native-wireless hotspot is included");
    auto wireless=hotspot;wireless.PhysicalMediumType=NdisPhysicalMediumWirelessLan;
    check(IncludeNetworkInterface(wireless),"wireless-LAN physical-medium variant is supported");
    check(!IncludeNetworkInterface(tun)&&!IncludeNetworkInterface(wan)&&!IncludeNetworkInterface(tap),"TUN, WAN miniport and TAP do not add intermediate traffic");
    auto fakeWireless=hotspot;fakeWireless.PhysicalMediumType=NdisPhysicalMediumUnspecified;
    check(!IncludeNetworkInterface(fakeWireless),"virtual interface needs genuine wireless-medium metadata");
    auto tunnel=hotspot;tunnel.TunnelType=TUNNEL_TYPE_OTHER;
    check(!IncludeNetworkInterface(tunnel),"tunnel metadata is excluded even on wireless-shaped row");
    auto loop=adapter(6,IF_TYPE_SOFTWARE_LOOPBACK,true,NdisPhysicalMediumUnspecified);
    check(!IncludeNetworkInterface(loop),"loopback is excluded");
    for(auto status:{IfOperStatusDown,IfOperStatusTesting,IfOperStatusUnknown,IfOperStatusDormant,IfOperStatusNotPresent,IfOperStatusLowerLayerDown}){
        auto inactive=hotspot;inactive.OperStatus=status;
        check(!IncludeNetworkInterface(inactive),"inactive hotspot is not counted");
    }
    for(auto base:{wifi,hotspot}){
        auto filter=base;filter.InterfaceAndOperStatusFlags.FilterInterface=true;
        auto endpoint=base;endpoint.InterfaceAndOperStatusFlags.EndPointInterface=true;
        auto disconnected=base;disconnected.InterfaceAndOperStatusFlags.NotMediaConnected=true;
        auto paused=base;paused.InterfaceAndOperStatusFlags.Paused=true;
        check(!IncludeNetworkInterface(filter)&&!IncludeNetworkInterface(endpoint)&&
              !IncludeNetworkInterface(disconnected)&&!IncludeNetworkInterface(paused),"filtered, endpoint, disconnected and paused layers stay excluded");
    }
    std::array rows{wifi,hotspot,ethernet,tun,wan};
    rows[0].InOctets=100000000000ull;rows[1].OutOctets=90000000000ull;
    rows[3].InOctets=rows[4].OutOctets=800000000000ull;
    NetworkSampler combined;
    auto initial=NetworkCounters(rows);
    check(initial.size()==3,"only physical and hotspot links enter combined snapshot");
    check(!combined.Sample(initial,1000)&&!combined.value,"combined counters need a real baseline");
    rows[0].InOctets+=10000000;rows[1].OutOctets+=10000000;
    rows[2].InOctets+=250000;rows[2].OutOctets+=125000;
    rows[3].InOctets+=10000000;rows[4].OutOctets+=10000000;
    combined.Sample(NetworkCounters(rows),2000);
    check(combined.value&&combined.value->received==10250000&&combined.value->sent==10125000,"forwarded hotspot RX and TX plus separate Ethernet are aggregated, tunnels excluded");
    rows[1].OperStatus=IfOperStatusDown;rows[0].InOctets+=500000;
    combined.Sample(NetworkCounters(rows),3000);
    check(combined.value&&combined.value->received==500000&&combined.value->sent==0,"hotspot disconnect keeps surviving link rate without subtraction spike");
    rows[1].OperStatus=IfOperStatusUp;rows[1].OutOctets=400000000000ull;
    combined.Sample(NetworkCounters(rows),4000);
    check(!combined.value,"hotspot reconnect cannot expose lifetime bytes");
    rows[0].InOctets+=100000;rows[1].OutOctets+=200000;
    std::reverse(rows.begin(),rows.end());combined.Sample(NetworkCounters(rows),4200);
    check(combined.value&&combined.value->received==500000&&combined.value->sent==1000000,"reordered interfaces match stable LUID after hotspot warmup");
    NetworkSampler sample;
    check(!sample.Sample(NetworkSnapshot{{1,1000,2000}},1000)&&!sample.value,"first network sample is baseline");
    check(!sample.Sample(NetworkSnapshot{{1,1100,2100}},1100)&&!sample.value,"reject early tick");
    check(sample.Sample(NetworkSnapshot{{1,1400,2200}},1200)&&sample.value->received==2000&&sample.value->sent==1000,"real elapsed rate");
    check(sample.IntervalMs()==1000,"steady sampling one second");
    sample.Pause();check(!sample.baseline&&!sample.value,"hidden page clears rate and baseline");
    check(!sample.Sample(NetworkSnapshot{{1,999000,888000}},10000),"hidden traffic never mixed into rate");
    sample.Sample(NetworkSnapshot{{1,999500,888250}},10500);check(sample.value&&sample.value->received==1000&&sample.value->sent==500,"resume real delta");
    sample.Sample(NetworkSnapshot{{1,10,10}},11000);check(!sample.value,"counter reset cannot underflow");
    sample.Sample(NetworkSnapshot{{1,510,1010},{2,500000,400000}},11500);check(!sample.value,"new adapter requires baseline");
    sample.Sample(NetworkSnapshot{{1,1010,1510},{2,500500,400500}},12000);check(sample.value&&sample.value->received==2000&&sample.value->sent==2000,"sum distinct stable adapters");
    sample.Sample(NetworkSnapshot{},13000);check(sample.value&&sample.value->received==0&&!sample.connected,"disconnect clears last rate");
    sample.Sample(NetworkSnapshot{{7,1000000,1000000}},14000);check(!sample.value,"reconnect does not expose lifetime counter");
    sample.Sample(NetworkSnapshot{{7,1000100,1000100}},13900);check(!sample.value,"time rollback resets");
    sample.Sample(NetworkSnapshot{{7,9999999,9999999}},99999);check(!sample.value,"long suspend resets baseline");
    sample.Sample({},100999);check(!sample.value&&!sample.baseline,"failed query is unknown not zero");
    check(RateText({})==L"—"&&RateText(-1)==L"—","unknown speed formatting");
    check(RateText(std::numeric_limits<double>::infinity())==L"—","nonfinite speed rejected");
    check(RateText(0)==L"0 B/s"&&RateText(1250)==L"1.2 KB/s"&&RateText(12500000)==L"12.5 MB/s","decimal byte rates");
    check(RateText(1e30)==L">9999 GB/s","bounded display");
    const auto upload=FormatRate(129000),download=FormatRate(3300),unknown=FormatRate({});
    check(upload.number==L"129"&&upload.unit==L"KB"&&download.number==L"3.3"&&download.unit==L"KB","separate aligned values and compact units");
    check(unknown.number==L"—"&&unknown.unit.empty()&&unknown.PerSecondText()==L"—","unknown rate has no stale unit");
    check(FormatRate(0).unit==L"B"&&FormatRate(12500000).unit==L"MB"&&FormatRate(1e30).number==L">9999","compact unit transitions and bounds");
    check(FormatRate(500).CompactText()==L"500 B"&&FormatRate(1500).CompactText()==L"1.5 KB","number and variable-length unit use one space");
    check(FormatRate(12500000).CompactText()==L"12.5 MB"&&FormatRate(1e30).CompactText()==L">9999 GB","joined compact labels keep larger units and limits");
    check(FormatRate({}).CompactText()==L"—"&&FormatRate(0).CompactText()==L"0 B","unknown and zero compact labels have no padding");
    if(argc!=2)return 2;std::wstring root=argv[1];
    check(CreateDirectoryW(root.c_str(),nullptr)!=FALSE,"isolated nonexisting test root");
    Page selected=Page::Power;PageOrder saved;
    check(ReadPreferences(root,saved,selected)&&selected==Page::Clock,"missing file defaults without write");
    check(GetFileAttributesW(PreferencesPath(root).c_str())==INVALID_FILE_ATTRIBUTES,"reading defaults creates no file");
    saved=*ParseOrder(L"power,clock,network");check(SaveOrder(root,saved),"save order");
    check(SaveSelection(root,Page::Network),"remember selection");
    PageOrder reread;check(ReadPreferences(root,reread,selected)&&reread.Text()==saved.Text()&&selected==Page::Network,"read persisted order and page");
    check(SaveOrder(root,*ParseOrder(L"clock,power")),"disable selected page");
    check(ReadPreferences(root,reread,selected)&&selected==Page::Clock,"disabled saved page falls back");
    check(SaveSelection(root,Page::Power)&&ReadPreferences(root,reread,selected)&&reread.Text()==L"clock,power","selection preserves order");
    WritePrivateProfileStringW(L"cards",L"Order",L"clock,unknown",PreferencesPath(root).c_str());
    auto before=reread.Text();check(!ReadPreferences(root,reread,selected)&&reread.Text()==before&&selected==Page::Power,"invalid reload preserves last good state");
    PageOrder empty;empty.pages.clear();check(!SaveOrder(root,empty),"prevent all-disabled saved state");
    check(!SaveSelection(root,Page::Count),"invalid selected page cannot be written");
    LARGE_INTEGER f{},a{},b{};QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
    auto real=ReadNetwork();QueryPerformanceCounter(&b);
    check(bool(real),"current physical and hotspot counter API succeeds");
    std::printf("Live network query: %zu active physical/hotspot interfaces, %.3f ms (no traffic generated)\n",real?real->size():0,1000.0*(b.QuadPart-a.QuadPart)/f.QuadPart);
    std::printf("Card preferences/network: %d checks, %d failures\n",count,failures);return failures?1:0;
}
