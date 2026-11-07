#pragma once
#include "control-panel-latest.h"
#include "wifi-page-latest.h"
#include "bluetooth-page-latest.h"
#include "hotspot-page-latest.h"
#include "sound-page-latest.h"
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

// windows.graphics.directx.direct3d11.interop.h; MinGW lacks the header.
struct CapsuleDxgiAccess : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetInterface(REFIID,void**)=0;
};
template<> inline constexpr winrt::guid winrt::impl::guid_v<CapsuleDxgiAccess>{
    0xA9B3D012,0x3DF2,0x4EE3,{0xB8,0xD1,0x86,0x95,0xF4,0x57,0xD3,0xC1}};


namespace Capsule {
inline unsigned VerifyMetricModel(){
    unsigned count=0;auto check=[&](bool ok){++count;if(!ok)winrt::throw_hresult(E_UNEXPECTED);};
    check(PanelClock(0,0,0)==L"00:00:00");check(PanelClock(23,59,59)==L"23:59:59");
    std::deque<CpuPoint> history;
    for(unsigned i=0;i<62;++i)AppendCpu(history,1000+i*1000,i);
    check(history.size()==60&&history.front().at==3000&&history.back().at==62000);
    auto points=CpuTrend(history,84,16);
    check(points.size()==60&&points.front().x==0&&points.back().x==84);
    for(auto p:points)check(p.x>=0&&p.x<=84&&p.y>=0&&p.y<=16);
    AppendCpu(history,61000,20);check(history.back().at==62000);
    AppendCpu(history,64000,80);points=CpuTrend(history,84,16);
    check(points.size()==59&&points.back().x-points[points.size()-2].x>2.8f);
    AppendCpu(history,200000,10);check(history.size()==1);
    check(CpuTrend(history,84,16).front().x==84); // No invented pre-open history.
    // Adapter type words observed on the reference machine: AMD iGPU,
    // NVIDIA dGPU, an indirect-display alias of the iGPU, Basic Render.
    check(GpuRenders(0x232B));check(GpuRenders(0x2311));check(!GpuRenders(0x342));check(!GpuRenders(0x105));check(!GpuRenders(0));
    check(NodeBusy(100,600,1000)==50);check(NodeBusy(0,0,1000)==0);check(NodeBusy(0,5000,1000)==100);
    check(NodeBusy(600,100,1000)<0);check(NodeBusy(0,100,0)<0);check(NodeBusy(0,100,-1)<0);
    GpuBusy busy;check(busy.Percent()==-1);busy.Add(-1);check(busy.Percent()==-1);
    busy.Add(30);busy.Add(49.6);busy.Add(20);check(busy.Percent()==50); // Busiest node, not a sum.
    busy.Add(NAN);check(busy.Percent()==50);busy.Add(0);check(busy.Percent()==50);
    Sample s;s.gpus={{1,L"AMD Radeon(TM) Graphics",7}};
    PanelLayout one(s,0);s.gpus.push_back({2,L"NVIDIA GeForce GTX 1650",0});PanelLayout two(s,0);
    check(two.height-one.height==22&&two.gpuY==one.gpuY);
    check(two.gpuY+44<two.height);
    return count;
}
inline unsigned VerifyThemeModel(){
    unsigned count=0;auto check=[&](bool value){++count;if(!value)winrt::throw_hresult(E_UNEXPECTED);};
    check(ThemeControl::Classify(0,0)==1);check(ThemeControl::Classify(1,1)==0);
    check(ThemeControl::Classify(1,0)==2);check(ThemeControl::Classify(0,1)==2);
    check(ThemeControl::Classify(2,0)==-1);check(ThemeControl::Classify(0,2)==-1);
    check(ThemeControl::Classify(MAXDWORD,MAXDWORD)==-1);
    check(ThemeControl::Verified(0,1,1));check(ThemeControl::Verified(0,0,0));
    for(auto exit:{DWORD(70),DWORD(72),DWORD(78),DWORD(79),DWORD(WAIT_TIMEOUT)})check(!ThemeControl::Verified(exit,1,1));
    check(!ThemeControl::Verified(0,0,1));check(!ThemeControl::Verified(0,2,1));check(!ThemeControl::Verified(0,-1,1));
    Controls state;state.darkTheme=0;
    check(!PanelEnabled(PanelPart::DarkTheme,state));state.themeAvailable=true;
    check(PanelEnabled(PanelPart::DarkTheme,state));state.themeBusy=true;
    check(!PanelEnabled(PanelPart::DarkTheme,state));
    state.themeBusy=false;state.darkTheme=2;check(PanelEnabled(PanelPart::DarkTheme,state));
    state.darkTheme=-1;check(!PanelEnabled(PanelPart::DarkTheme,state));
    return count;
}
inline unsigned VerifyControlModel(){
    unsigned count=0;
    auto check=[&](bool value){++count;if(!value)winrt::throw_hresult(E_UNEXPECTED);};
    {   // Not clicked: leaves HoverDismiss after the pointer, unless it returns.
        PanelStay s;
        check(!s.Close(true,false,false,100));check(!s.Close(false,false,false,1000));
        check(!s.Close(false,false,false,1000+HoverDismiss-1));check(s.Close(false,false,false,1000+HoverDismiss));
        PanelStay back;check(!back.Close(false,false,false,1000));check(!back.Close(true,false,false,1100));
        check(!back.Close(false,false,false,1200));check(!back.Close(false,false,false,1200+HoverDismiss-1));
        // A press elsewhere closes at once in every state, a press on the key or panel never.
        PanelStay press;check(press.Close(false,true,false,1000));check(!press.Close(true,true,false,1000));
        // Clicked: the pointer may go anywhere, for any time, until a press elsewhere.
        PanelStay held;held.clicked=true;
        check(!held.Close(false,false,false,1000));check(!held.Close(false,false,false,600000));
        check(!held.Close(true,true,false,600050));check(held.Close(false,true,false,600100));
        // Typing behaves the same, and a timer started earlier does not carry over.
        PanelStay typed;check(!typed.Close(false,false,false,1000));check(!typed.Close(false,false,true,5000));
        check(!typed.Close(false,false,false,5050));check(typed.Close(false,false,false,5050+HoverDismiss));
    }
    for(auto scale:{1.f,1.25f,1.5f,1.75f,2.f}){
        auto hit=[&](float x,float y){return PanelHit((x*scale)/scale,(y*scale)/scale);};
        check(hit(20,80)==PanelPart::Wifi);check(hit(135,80)==PanelPart::WifiMore);
        check(hit(170,80)==PanelPart::Bluetooth);check(hit(245,80)==PanelPart::Bluetooth);check(hit(275,80)==PanelPart::BluetoothMore);
        check(hit(24,182)==PanelPart::Mute);check(hit(140,182)==PanelPart::Volume);
        check(hit(270,182)==PanelPart::VolumeMore);check(hit(240,182)==PanelPart::None);check(hit(270,224)==PanelPart::None);
        check(hit(140,224)==PanelPart::Brightness);check(hit(150,80)==PanelPart::None);
        check(hit(14,286)==PanelPart::None);check(hit(140,252)==PanelPart::None);
        check(hit(20,130)==PanelPart::DarkTheme);check(hit(135,130)==PanelPart::DarkTheme);
        check(hit(170,130)==PanelPart::Hotspot);check(hit(245,130)==PanelPart::Hotspot);check(hit(275,130)==PanelPart::HotspotMore);
        check(hit(30,107)==PanelPart::None);check(hit(150,130)==PanelPart::None);
    }
    check(SliderValue(-100)==0);check(SliderValue(500)==100);
    check(SliderValue(SliderLeft)==0);check(SliderValue(SliderRight)==100);
    check(SliderValue((SliderLeft+SliderRight)/2)==50);
    PanelGesture g;g.Begin(PanelPart::Brightness,60);
    check(g.Dragging());check(g.Send(60,100));check(!g.Send(180,120));
    check(g.preview==76);check(g.Send(180,180));check(!g.Send(180,280));
    check(g.Send(244,290,true));check(g.preview==100);
    g.Cancel();check(!g.Dragging()&&g.pressed==PanelPart::None);check(!g.Send(244,500,true));
    g.Begin(PanelPart::Wifi,20);check(!g.Dragging());g.Cancel();
    Controls state;check(!PanelEnabled(PanelPart::Volume,state));check(!PanelEnabled(PanelPart::Brightness,state));
    check(!PanelEnabled(PanelPart::Wifi,state));check(PanelEnabled(PanelPart::WifiMore,state));
    state.volume=20;state.brightness=100;state.wifi=1;state.bluetooth=0;
    check(PanelEnabled(PanelPart::Mute,state));check(PanelEnabled(PanelPart::Brightness,state));
    check(PanelEnabled(PanelPart::Wifi,state));state.bluetoothBusy=true;check(!PanelEnabled(PanelPart::Wifi,state));
    state.bluetoothBusy=false;state.wifi=3;check(!PanelEnabled(PanelPart::Wifi,state));
    check(!PanelEnabled(PanelPart::DarkTheme,state));check(!PanelEnabled(PanelPart::Hotspot,state));
    check(PanelEnabled(PanelPart::HotspotMore,state)&&PanelEnabled(PanelPart::BluetoothMore,state)&&PanelEnabled(PanelPart::VolumeMore,state));
    state.darkTheme=0;state.themeAvailable=true;state.hotspot=0;state.wifi=1;
    check(PanelEnabled(PanelPart::DarkTheme,state));check(PanelEnabled(PanelPart::Hotspot,state));
    state.hotspotBusy=true;check(!PanelEnabled(PanelPart::Hotspot,state));check(!PanelEnabled(PanelPart::Wifi,state));
    ControlQueue queue;
    for(uint64_t i=1;i<=1000;++i)queue.Put({Control::Volume,int(i%101),i});
    queue.Put({Control::Brightness,63,1001});auto commands=queue.Take();
    check(commands.size()==2&&commands[0].serial==1000&&commands[1].value==63);
    check(queue.Empty());queue.Put({Control::Mute,1,1002});queue.Put({Control::Volume,40,1003});
    commands=queue.Take();check(commands[0].target==Control::Mute&&commands[1].target==Control::Volume);
    PanelValueRequest desired{100,1000,81};
    check(desired.Display(50,99,1100)==81);check(desired.Display(79,100,1200)==79);
    check(desired.Display(-1,100,1200)==-1);check(desired.Display(50,99,5000)==50);
    PanelMotion motion;Controls live;live.wifi=1;live.volume=20;PanelGesture idle;
    motion.Update(live,PanelPart::Wifi,idle,1000,true);
    check(motion.on[0].At(1000)==1&&motion.hover[unsigned(PanelPart::Wifi)].At(1000)==1&&!motion.Active(1000)); // First frame is settled.
    live.wifi=0;live.volume=80;motion.Update(live,PanelPart::None,idle,2000,true);
    check(motion.Active(2000)&&motion.on[0].At(2000)==1&&motion.on[0].At(2090)>0&&motion.on[0].At(2090)<1);
    check(motion.volume.At(2120)>20&&motion.volume.At(2120)<80&&motion.volume.At(2240)==80&&!motion.Active(2240));
    PanelGesture drag;drag.Begin(PanelPart::Volume,SliderLeft);motion.Update(live,PanelPart::Volume,drag,3000,true);
    check(motion.volume.At(3000)==0&&!motion.volume.Active(3000)); // A drag is never eased.
    live.volume=-1;motion.Update(live,PanelPart::None,idle,4000,true);check(motion.volume.At(4000)==0);
    live.volume=50;motion.Update(live,PanelPart::None,idle,5000,false);check(motion.volume.At(5000)==50&&!motion.Active(5000));
    Sample s;PanelLayout empty(s,0);check(empty.mediaY<0&&empty.temperatureY<0&&empty.trayY<0);
    auto cuts=empty.Cuts();check(cuts.front()==0&&cuts.back()==empty.height&&std::is_sorted(cuts.begin(),cuts.end()));
    s.media=L"▶ 测试标题";s.error=L"设备已断开";PanelLayout content(s,7);
    check(content.mediaY>=0&&content.errorY>content.mediaY&&content.trayY>content.errorY);
    check(content.height>empty.height&&content.trayY+52<content.height);
    return count;
}
inline WifiList FixtureNetworks(){
    WifiList w;w.ready=true;
    auto add=[&](std::wstring ssid,int bars,int kind,bool saved,bool connected){ // kind: 0 open, 1 personal, 2 enterprise
        WifiNetwork n;n.ssid=std::move(ssid);n.bars=bars;n.secure=kind>0;n.psk=kind==1;n.enterprise=kind==2;
        n.saved=saved;n.connected=connected;w.networks.push_back(std::move(n));};
    add(L"NWAFU",4,0,true,true);add(L"@NRadio-B066-2.4G",3,1,false,false);add(L"LANSVEHD",2,1,false,false);
    add(L"eduroam",3,2,false,false);add(L"德莉莎大人的胖次",2,1,false,false);add(L"TP-LINK_5G_8A2C",1,1,true,false);
    add(L"ChinaNet-Free",1,0,false,false);add(L"Xiaomi_9E31",2,1,false,false);add(L"CMCC-Lab",0,1,false,false);
    SortNetworks(w.networks);return w;
}
inline unsigned VerifyWifiModel(){
    unsigned count=0;auto check=[&](bool value){++count;if(!value)winrt::throw_hresult(E_UNEXPECTED);};
    auto list=FixtureNetworks();
    check(list.networks.front().connected&&list.networks.front().ssid==L"NWAFU");
    for(size_t i=2;i<list.networks.size();++i)check(list.networks[i-1].bars>=list.networks[i].bars);
    check(WifiSubtitle(list.networks[0],list)==L"已连接，开放");
    WifiView view;WifiLayout closed(list,view,406);
    check(closed.rows.size()==list.networks.size()&&closed.content==list.networks.size()*(WifiRowHeight+2));
    check(closed.Clamp(-20)==0&&closed.Clamp(1e6f)==closed.content-closed.view);
    check(WifiHit(closed,view,20,20).hit==PageHit::Back&&WifiHit(closed,view,262,28).hit==PageHit::Toggle);
    check(WifiHit(closed,view,150,PageListTop+10).key==L"NWAFU"&&WifiHit(closed,view,150,30).hit==PageHit::None);
    check(WifiHit(closed,view,150,PageListTop+WifiRowHeight+5).key==list.networks[1].ssid);
    check(WifiHit(closed,view,150,closed.bottom+1).hit==PageHit::None);
    // A personal network without a profile asks for its key; enterprise also for a user.
    auto personal=std::find_if(list.networks.begin(),list.networks.end(),[](auto const& n){return n.psk&&!n.saved;})->ssid;
    view.expanded=personal;WifiLayout open(list,view,406);auto row=open.Find(personal);
    check(row&&row->open&&row->form&&!row->user&&row->h>WifiRowHeight&&!WifiFormReady(*row,view));
    view.password=L"x";check(WifiFormReady(*row,view));
    const float at=PageListTop+row->passwordBox.y+17-view.scroll;
    check(WifiHit(open,view,40,at).hit==PageHit::Field&&WifiHit(open,view,265,at).hit==PageHit::Reveal);
    check(WifiHit(open,view,230,PageListTop+row->button.y+10).hit==PageHit::Primary);
    check(WifiHit(open,view,100,PageListTop+row->button.y+10).hit==PageHit::None);
    view.expanded=L"eduroam";WifiLayout enterprise(list,view,406);auto edu=enterprise.Find(L"eduroam");
    check(edu&&edu->form&&edu->user&&!WifiFormReady(*edu,view));view.user=L"u";check(WifiFormReady(*edu,view));
    view.Wipe();check(view.password.empty()&&view.user.empty()&&view.focus==WifiView::Focus::None);
    // Saved, open and connected rows need no form; a wrong saved key asks again.
    view.expanded=L"TP-LINK_5G_8A2C";check(!WifiLayout(list,view,406).Find(view.expanded)->form);
    list.failed=view.expanded;list.failure=L"密码不正确";list.wrongPassword=true;
    auto retry=WifiLayout(list,view,406);check(retry.Find(view.expanded)->form&&retry.Find(view.expanded)->status);
    view.expanded=L"NWAFU";check(!WifiLayout(list,view,406).Find(L"NWAFU")->form);
    list.connecting=L"NWAFU";check(WifiSubtitle(list.networks[0],list)==L"正在连接…");
    // Revealing the last row scrolls it fully into view.
    view.expanded=list.networks.back().ssid;WifiLayout end(list,view,406);auto last=end.Find(view.expanded);
    const float scroll=end.Reveal(*last,0);check(scroll>0&&last->y+last->h<=scroll+end.view+.01f);
    check(WifiFailure(3)==L"密码不正确"&&WifiFailure(99)==L"无法连接到这个网络");
    // The list saved between hovers: everything but the live connected state
    // comes back, and damage loses only what follows it.
    auto packed=PackNetworks(list.networks);auto back=UnpackNetworks(packed);
    check(back.size()==list.networks.size());
    for(size_t i=0;i<back.size();++i){
        auto const& was=list.networks[i];auto const& n=back[i];
        check(n.ssid==was.ssid&&n.bars==was.bars&&n.secure==was.secure&&n.enterprise==was.enterprise&&n.psk==was.psk&&n.saved==was.saved&&!n.connected);
    }
    check(UnpackNetworks(std::string_view(packed).substr(0,packed.size()-1)).size()==back.size()-1);
    check(UnpackNetworks({}).empty()&&UnpackNetworks(std::string("\xff\0\0\0",4)).empty()&&UnpackNetworks(std::string("\x02\0\x09\0abcd",8)).empty());
    return count;
}
inline BluetoothList FixtureDevices(){
    BluetoothList b;b.ready=true;
    auto add=[&](std::wstring name,BluetoothKind kind,bool connected,bool audio,int battery){
        BluetoothDevice d;d.id=L"aep:"+name;d.name=std::move(name);d.kind=kind;d.connected=connected;d.audio=audio;d.battery=battery;
        b.devices.push_back(std::move(d));};
    add(L"WH-1000XM4",BluetoothKind::Headphones,true,true,80);add(L"MX Master 3",BluetoothKind::Mouse,true,false,-1);
    add(L"Xiaomi 13",BluetoothKind::Phone,false,false,-1);add(L"小爱音箱 Play",BluetoothKind::Speaker,false,true,-1);
    SortDevices(b.devices);return b;
}
inline SoundList FixtureSound(){
    SoundList s;s.ready=true;
    s.outputs={{L"out:1",L"扬声器",L"Realtek(R) Audio",1,true},{L"out:2",L"耳机",L"WH-1000XM4",3,false}};
    s.apps={{SystemSoundsKey,L"系统声音",L"",100,false,true},{L"c:\\edge.exe",L"Microsoft Edge",L"",62,false,false},
        {L"c:\\music.exe",L"网易云音乐",L"",35,true,false}};
    return s;
}
inline HotspotInfo FixtureHotspot(){
    HotspotInfo h;h.ready=true;h.ssid=L"xcan";h.source=L"NWAFU";h.band=2;h.keyLength=8;h.maxClients=8;h.bands={true,true,true};
    h.clients={{L"Xiaomi-13",L"192.168.137.23"},{L"iPad",L"192.168.137.104"}};
    return h;
}
// Bluetooth, hotspot and sound pages: layout, hits, labels and limits.
inline unsigned VerifyPageModels(){
    unsigned count=0;auto check=[&](bool value){++count;if(!value)winrt::throw_hresult(E_UNEXPECTED);};
    // Class of device: headphones, keyboard and mouse bits, gamepad; LE appearance.
    check(ClassicKind(4,6)==BluetoothKind::Headphones&&ClassicKind(4,5)==BluetoothKind::Speaker);
    check(ClassicKind(5,0x10)==BluetoothKind::Keyboard&&ClassicKind(5,0x20)==BluetoothKind::Mouse&&ClassicKind(5,2)==BluetoothKind::Gamepad);
    check(ClassicKind(2,3)==BluetoothKind::Phone&&ClassicKind(9,0)==BluetoothKind::Other);
    check(LeKind(0x00F,2)==BluetoothKind::Mouse&&LeKind(0x00F,1)==BluetoothKind::Keyboard&&LeKind(0x001,0)==BluetoothKind::Phone);
    auto devices=FixtureDevices();
    check(devices.devices[0].connected&&devices.devices[1].connected&&!devices.devices[2].connected);
    auto named=[&](std::wstring const& name)->BluetoothDevice const&{
        return *std::find_if(devices.devices.begin(),devices.devices.end(),[&](auto const& d){return d.name==name;});};
    check(BluetoothSubtitle(named(L"WH-1000XM4"),devices)==L"已连接 · 电量 80%"&&BluetoothSubtitle(named(L"MX Master 3"),devices)==L"已连接");
    check(BluetoothSubtitle(devices.devices[2],devices)==L"未连接");
    BluetoothView bt;BluetoothLayout closed(devices,bt,406);
    check(closed.rows.size()==4&&closed.content==4*(DeviceRowHeight+2));
    check(BluetoothHit(closed,bt,20,20).hit==PageHit::Back&&BluetoothHit(closed,bt,262,28).hit==PageHit::Toggle);
    // Audio devices open; the others are status only.
    const auto mouse=std::find_if(devices.devices.begin(),devices.devices.end(),[](auto const& d){return !d.audio;})->id;
    const auto speaker=std::find_if(devices.devices.begin(),devices.devices.end(),[](auto const& d){return d.audio&&!d.connected;})->id;
    auto rowAt=[&](BluetoothLayout const& l,std::wstring const& id){return PageListTop+l.Find(id)->y+10;};
    check(BluetoothHit(closed,bt,150,rowAt(closed,mouse)).hit==PageHit::None);
    check(BluetoothHit(closed,bt,150,rowAt(closed,speaker)).key==speaker);
    bt.expanded=speaker;BluetoothLayout open(devices,bt,406);auto row=open.Find(speaker);
    check(row->open&&row->h>DeviceRowHeight&&BluetoothHit(open,bt,230,PageListTop+row->button.y+10).hit==PageHit::Primary);
    bt.expanded=mouse;check(!BluetoothLayout(devices,bt,406).Find(mouse)->open);
    devices.busy=speaker;devices.connecting=true;check(BluetoothSubtitle(*std::find_if(devices.devices.begin(),devices.devices.end(),
        [&](auto const& d){return d.id==speaker;}),devices)==L"正在连接…");
    BluetoothLayout footer(devices,bt,406,true);check(footer.bottom==closed.bottom-PageFooter);
    // Hotspot limits are Windows': SSID 32 bytes, key 8-63 printable ASCII.
    check(!HotspotProblem(L"xcan",L"12345678")&&HotspotProblem(L"",L"12345678")&&HotspotProblem(L"xcan",L"1234567"));
    check(HotspotProblem(std::wstring(11,L'热'),L"12345678")&&!HotspotProblem(std::wstring(10,L'热'),L"12345678"));
    check(HotspotProblem(L"xcan",L"密码12345678")&&!HotspotProblem(std::wstring(32,L'a'),std::wstring(63,L'~')));
    HotspotView hv;check(hv.password.capacity()>=64&&hv.shown.capacity()>=64);
    check(HotspotHit(hv,265,105).hit==PageHit::Reveal&&HotspotHit(hv,230,200).hit==PageHit::Secondary&&HotspotHit(hv,100,90).hit==PageHit::None);
    hv.editing=true;
    check(HotspotHit(hv,100,90).hit==PageHit::Name&&HotspotHit(hv,100,150).hit==PageHit::Field&&HotspotHit(hv,265,152).hit==PageHit::Reveal);
    check(HotspotHit(hv,150,205).key==L"1"&&HotspotHit(hv,230,270).hit==PageHit::Primary&&HotspotHit(hv,130,270).hit==PageHit::Secondary);
    hv.password=L"secret12";hv.shown=L"secret12";hv.reveal=true;hv.Reset();
    check(hv.password.empty()&&hv.shown.empty()&&!hv.reveal&&!hv.editing);
    Sample hs;hs.hotspot=0;hs.hotspotInfo=FixtureHotspot();check(HotspotEditable(hs));
    hs.hotspot=1;check(!HotspotEditable(hs));hs.hotspot=0;hs.hotspotInfo.saving=true;check(!HotspotEditable(hs));
    check(std::wstring(BandLabel(2))==L"5 GHz"&&std::wstring(BandLabel(-1))==L"—");
    // Sound: master on top, outputs, then apps; the selected output does nothing.
    auto sound=FixtureSound();SoundView sv;SoundLayout sl(sound,406);
    check(sl.top==SoundTop&&sl.rows.size()==2+2+3&&sl.rows[1].kind==SoundRow::Kind::Output);
    check(SoundHit(sl,sv,24,MasterY).hit==PageHit::Mute&&SoundHit(sl,sv,24,MasterY).key.empty());
    check(SoundHit(sl,sv,150,MasterY).hit==PageHit::Slider&&SoundHit(sl,sv,150,MasterY).key.empty());
    check(SoundHit(sl,sv,150,SoundTop+sl.rows[1].y+10).hit==PageHit::None);
    check(SoundHit(sl,sv,150,SoundTop+sl.rows[2].y+10).key==L"out:2");
    auto const& app=sl.rows[5];
    check(app.kind==SoundRow::Kind::App&&SoundHit(sl,sv,24,SoundTop+app.y+12).hit==PageHit::Mute);
    check(SoundHit(sl,sv,150,SoundTop+app.y+37).hit==PageHit::Slider&&SoundHit(sl,sv,150,SoundTop+app.y+37).key==app.app.key);
    check(SoundHit(sl,sv,150,SoundTop+app.y+10).hit==PageHit::None);
    SoundList quiet=sound;quiet.apps.clear();check(SoundLayout(quiet,406).rows.back().kind==SoundRow::Kind::Note);
    SoundDrag d;d.Begin(L"k",AppSliderLeft,SoundRight,AppSliderLeft);check(d.active&&d.Send(AppSliderLeft,100)&&d.preview==0);
    check(!d.Send(SoundRight,120)&&d.Send(SoundRight,120,true)&&d.preview==100);d.Cancel();check(!d.active&&!d.Send(10,500));
    return count;
}
inline int ControlsProbe(){
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    const auto checks=VerifyControlModel()+VerifyWifiModel()+VerifyPageModels(),themeChecks=VerifyThemeModel(),metricChecks=VerifyMetricModel();
    const auto themeBefore=ThemeControl::Read();
    Metrics metrics;metrics.Start(nullptr);metrics.Visible(true);
    const auto deadline=GetTickCount64()+6000;
    Sample data;
    do{Sleep(100);data=metrics.Read();}while(GetTickCount64()<deadline&&(data.reads<2||data.wifi<0||data.bluetooth<0));
    // Each page's own reads on this machine, still read-only.
    auto page=[&](PanelPage p,auto ready){
        metrics.Page(p);const auto end=GetTickCount64()+3000;Sample d;
        do{Sleep(100);d=metrics.Read();}while(GetTickCount64()<end&&!ready(d));return d;
    };
    const auto bt=page(PanelPage::Bluetooth,[](Sample const& d){return d.bluetoothList.ready;});
    const auto sound=page(PanelPage::Sound,[](Sample const& d){return d.sound.ready;});
    const auto hotspot=page(PanelPage::Hotspot,[](Sample const& d){return d.hotspotInfo.ready;});
    metrics.Page(PanelPage::Main);Sleep(200);const bool pagesCleared=!metrics.Read().hotspotInfo.ready&&!metrics.Read().sound.ready;
    metrics.Visible(false);Sleep(300);auto hidden=metrics.Read();Sleep(1200);auto idle=metrics.Read();
    const auto beforeStop=GetTickCount64();metrics.Stop();const auto stopMs=GetTickCount64()-beforeStop;metrics.Stop();
    const bool readonly=data.writes==0&&idle.writes==0&&themeBefore==ThemeControl::Read();
    const bool noHiddenReads=hidden.reads==idle.reads&&hidden.detailReads==idle.detailReads&&
        hidden.diskReads==idle.diskReads&&hidden.history.size()==idle.history.size()&&hidden.at==idle.at;
    FILE* names=_wfopen((Directory()+L"\\metrics-probe-latest.txt").c_str(),L"wb");
    if(names){for(auto const& gpu:data.gpus){const auto name=winrt::to_string(gpu.name);
        fprintf(names,"%s | LUID %llX | 3D %d%%\n",name.c_str(),gpu.luid,gpu.percent);}fclose(names);}
    FILE* detail=_wfopen((Directory()+L"\\metrics-probe-latest.json").c_str(),L"w");
    if(detail){fprintf(detail,"{\"metricChecks\":%u,\"cpu\":%d,\"historySamples\":%zu,\"detailReads\":%llu,\"diskReads\":%llu,\"gpus\":[",
        metricChecks,data.cpu,data.history.size(),data.detailReads,data.diskReads);
        for(size_t i=0;i<data.gpus.size();++i){auto const& gpu=data.gpus[i];
            fprintf(detail,"%s{\"luid\":\"%llX\",\"percent3D\":%d}",i?",":"",gpu.luid,gpu.percent);}
        fprintf(detail,"],\"hiddenSamplingUnchanged\":%s}\n",noHiddenReads?"true":"false");fclose(detail);}
    FILE* f=_wfopen((Directory()+L"\\controls-probe-latest.json").c_str(),L"w");
    if(f){fprintf(f,"{\"policyChecks\":%u,\"normalUser\":%s,\"volume\":%d,\"mute\":%s,\"brightness\":%d,"
        "\"wifi\":%d,\"bluetooth\":%d,\"reads\":%llu,\"writes\":%llu,\"hiddenReadsUnchanged\":%s,"
        "\"readOnly\":%s,\"stopMs\":%llu,\"visibleWindows\":false,\"actualControlWritesTested\":false,"
        "\"darkTheme\":%d,\"hotspot\":%d,\"hotspotCapability\":%d,\"hotspotClients\":%u,\"themePolicyChecks\":%u,"
        "\"bluetoothPage\":{\"ready\":%s,\"devices\":%zu},\"soundPage\":{\"ready\":%s,\"outputs\":%zu,\"apps\":%zu},"
        "\"hotspotPage\":{\"ready\":%s,\"named\":%s,\"keyRead\":%s,\"band\":%d},\"pagesCleared\":%s}\n",
        checks,Elevated()?"false":"true",data.volume,data.mute?"true":"false",data.brightness,data.wifi,data.bluetooth,
        data.reads,idle.writes,noHiddenReads?"true":"false",readonly?"true":"false",stopMs,
        data.darkTheme,data.hotspot,data.hotspotCapability,data.hotspotClients,themeChecks,
        bt.bluetoothList.ready?"true":"false",bt.bluetoothList.devices.size(),sound.sound.ready?"true":"false",
        sound.sound.outputs.size(),sound.sound.apps.size(),hotspot.hotspotInfo.ready?"true":"false",
        hotspot.hotspotInfo.ssid.empty()?"false":"true",hotspot.hotspotInfo.keyLength?"true":"false",hotspot.hotspotInfo.band,
        pagesCleared?"true":"false");fclose(f);}
    winrt::uninit_apartment();
    return readonly&&noHiddenReads&&data.reads>=2&&stopMs<2000&&pagesCleared&&bt.bluetoothList.ready&&sound.sound.ready?0:1;
}
inline Sample FixtureSample(int column){
    Sample s;s.volume=column?-1:42;s.brightness=column?-1:78;s.wifi=column?3:1;s.bluetooth=column?-1:0;
    s.darkTheme=column?-1:1;s.themeAvailable=!column;s.hotspot=column?-1:1;s.hotspotLabel=column?L"未连接网络":L"2 台已连接";
    s.cpu=24;s.memory=67;s.capacity=L"10.4 / 15.4 GB";
    s.gpus={{1,L"AMD Radeon(TM) Graphics",15},{2,L"NVIDIA GeForce GTX 1650",column?-1:0}};
    for(unsigned i=0;i<60;++i)AppendCpu(s.history,1000+i*1000,10+35*std::abs(std::sin(i/6.)));
    s.network=L"↓ 12.3 MB/s　↑ 84 KB/s";s.disks=L"C 123 GB　D 429 GB";
    s.battery=L"插电中，约 20 分钟充满";
    if(column){s.media=L"▶ 一首正在播放的音乐";s.error=L"无线控制未获系统授权";}
    return s;
}
inline Power FixturePower(){Power p;p.valid=true;p.value.BatteryFlag=0;p.value.BatteryLifePercent=97;p.value.ACLineStatus=1;return p;}
inline void SavePng(Drawing& draw,std::vector<BYTE>& pixels,UINT width,UINT height,std::wstring const& name){
    winrt::com_ptr<IWICBitmap> bitmap;
    winrt::check_hresult(draw.wic->CreateBitmapFromMemory(width,height,GUID_WICPixelFormat32bppPBGRA,width*4,UINT(pixels.size()),pixels.data(),bitmap.put()));
    winrt::com_ptr<IWICStream> stream;winrt::check_hresult(draw.wic->CreateStream(stream.put()));
    winrt::check_hresult(stream->InitializeFromFilename(name.c_str(),GENERIC_WRITE));
    winrt::com_ptr<IWICBitmapEncoder> encoder;winrt::check_hresult(draw.wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.put()));
    winrt::check_hresult(encoder->Initialize(stream.get(),WICBitmapEncoderNoCache));
    winrt::com_ptr<IWICBitmapFrameEncode> frame;winrt::check_hresult(encoder->CreateNewFrame(frame.put(),nullptr));
    winrt::check_hresult(frame->Initialize(nullptr));winrt::check_hresult(frame->SetSize(width,height));
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;winrt::check_hresult(frame->SetPixelFormat(&format));
    winrt::check_hresult(frame->WriteSource(bitmap.get(),nullptr));winrt::check_hresult(frame->Commit());winrt::check_hresult(encoder->Commit());
}
inline void RenderControlPanel(Drawing& draw){
    VerifyControlModel();VerifyMetricModel();const auto output=Directory()+L"\\control-panel-probe";
    std::filesystem::create_directories(output);const auto original=draw.colors;
    try{
        for(float scale:{1.f,1.25f,1.5f,2.f}){
            const UINT width=UINT(std::lround(644*scale)),height=UINT(std::lround(520*scale));
            winrt::com_ptr<ID2D1DeviceContext> dc;winrt::check_hresult(draw.device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,dc.put()));
            D2D1_BITMAP_PROPERTIES1 props{};props.pixelFormat={DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED};
            props.dpiX=props.dpiY=96*scale;props.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            winrt::com_ptr<ID2D1Bitmap1> target;winrt::check_hresult(dc->CreateBitmap({width,height},nullptr,0,&props,target.put()));
            dc->SetTarget(target.get());dc->SetDpi(96*scale,96*scale);dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            dc->BeginDraw();dc->Clear(Ink(0x7D8DAB));
            for(int column=0;column<2;++column){
                // Windows 11 default blue: AccentDark1 / AccentLight2.
                draw.colors.Set(column==0,column?0x60CDFF:0x005FB8);
                const auto s=FixtureSample(column);const auto p=FixturePower();
                PanelLayout layout(s,0);PanelGesture gesture;PanelMotion motion;
                const auto hover=column?PanelPart::None:PanelPart::Bluetooth;motion.Update(s,hover,gesture,0,false);
                dc->SetTransform(D2D1::Matrix3x2F::Translation(14+column*316.f,14));
                draw.Round(dc.get(),{0,0,PanelWidth,layout.height},14,Ink(column?0x1D1F25:0xF2F3F6));
                if(ClockWidth(draw,L"23:59:59")>120)winrt::throw_hresult(E_BOUNDS);
                PaintControlPanel(draw,dc.get(),s,p,L"13:26:08",L"10月23日 周五",L"农历九月十四",layout,hover,gesture,motion,0);
            }
            dc->SetTransform(D2D1::Matrix3x2F::Identity());winrt::check_hresult(dc->EndDraw());dc->SetTarget(nullptr);
            props.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            winrt::com_ptr<ID2D1Bitmap1> readback;winrt::check_hresult(dc->CreateBitmap({width,height},nullptr,0,&props,readback.put()));
            winrt::check_hresult(readback->CopyFromBitmap(nullptr,target.get(),nullptr));D2D1_MAPPED_RECT mapped{};
            winrt::check_hresult(readback->Map(D2D1_MAP_OPTIONS_READ,&mapped));
            std::vector<BYTE> pixels(size_t(width)*height*4);
            for(UINT row=0;row<height;++row)memcpy(pixels.data()+size_t(row)*width*4,mapped.bits+size_t(row)*mapped.pitch,width*4);
            readback->Unmap();
            SavePng(draw,pixels,width,height,output+L"\\dpi-"+std::to_wstring(int(scale*100))+L".png");
        }
    }catch(...){draw.colors=original;throw;}draw.colors=original;
}
// Second-level pages over the same plain panel colour as RenderControlPanel,
// light and dark side by side, at 100% and 200%: <name>-dpi-100/200.png.
template<class F> inline void RenderPageFixture(Drawing& draw,std::wstring const& name,F&& paint){
    const auto output=Directory()+L"\\control-panel-probe";
    std::filesystem::create_directories(output);const auto original=draw.colors;
    try{
        for(float scale:{1.f,2.f}){
            const UINT width=UINT(std::lround(644*scale)),height=UINT(std::lround(436*scale));
            winrt::com_ptr<ID2D1DeviceContext> dc;winrt::check_hresult(draw.device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,dc.put()));
            D2D1_BITMAP_PROPERTIES1 props{};props.pixelFormat={DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED};
            props.dpiX=props.dpiY=96*scale;props.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            winrt::com_ptr<ID2D1Bitmap1> target;winrt::check_hresult(dc->CreateBitmap({width,height},nullptr,0,&props,target.put()));
            dc->SetTarget(target.get());dc->SetDpi(96*scale,96*scale);dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            dc->BeginDraw();dc->Clear(Ink(0x7D8DAB));
            for(int column=0;column<2;++column){
                draw.colors.Set(column==0,column?0x60CDFF:0x005FB8);
                dc->SetTransform(D2D1::Matrix3x2F::Translation(14+column*316.f,14));
                draw.Round(dc.get(),{0,0,PanelWidth,406},14,Ink(column?0x1D1F25:0xF2F3F6));
                paint(column,dc.get());
            }
            dc->SetTransform(D2D1::Matrix3x2F::Identity());winrt::check_hresult(dc->EndDraw());dc->SetTarget(nullptr);
            props.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            winrt::com_ptr<ID2D1Bitmap1> readback;winrt::check_hresult(dc->CreateBitmap({width,height},nullptr,0,&props,readback.put()));
            winrt::check_hresult(readback->CopyFromBitmap(nullptr,target.get(),nullptr));D2D1_MAPPED_RECT mapped{};
            winrt::check_hresult(readback->Map(D2D1_MAP_OPTIONS_READ,&mapped));
            std::vector<BYTE> pixels(size_t(width)*height*4);
            for(UINT row=0;row<height;++row)memcpy(pixels.data()+size_t(row)*width*4,mapped.bits+size_t(row)*mapped.pitch,width*4);
            readback->Unmap();
            SavePng(draw,pixels,width,height,output+L"\\"+name+L"-dpi-"+std::to_wstring(int(scale*100))+L".png");
        }
    }catch(...){draw.colors=original;throw;}draw.colors=original;
}
// Wi-Fi: light with a key being typed, dark with an enterprise sign-in failing.
inline void RenderWifiPage(Drawing& draw){
    VerifyWifiModel();
    RenderPageFixture(draw,L"wifi",[&](int column,ID2D1DeviceContext* dc){
        Sample s=FixtureSample(0);s.wifiList=FixtureNetworks();WifiView view;PageTarget hover;
        if(column==0){view.expanded=L"@NRadio-B066-2.4G";view.password=L"secret12";view.focus=WifiView::Focus::Password;
            hover={PageHit::Row,L"LANSVEHD"};}
        else{view.expanded=L"eduroam";view.user=L"2021012345";view.focus=WifiView::Focus::User;
            s.wifiList.failed=L"eduroam";s.wifiList.failure=L"不支持这个网络的认证方式";}
        WifiLayout layout(s.wifiList,view,406);
        PaintWifiPage(draw,dc,s,layout,view,hover,1,1);
    });
}
// Bluetooth (light: audio device open; dark: a failed connect), hotspot
// (light: details with clients; dark: the form) and sound (light: a slider
// under the pointer; dark: a muted app and a control error).
inline void RenderPages(Drawing& draw){
    VerifyPageModels();
    RenderPageFixture(draw,L"bluetooth",[&](int column,ID2D1DeviceContext* dc){
        Sample s=FixtureSample(0);s.bluetooth=1;s.bluetoothList=FixtureDevices();BluetoothView view;PageTarget hover;
        if(column==0){view.expanded=L"aep:WH-1000XM4";hover={PageHit::Primary,L"aep:WH-1000XM4"};}
        else{view.expanded=L"aep:小爱音箱 Play";s.bluetoothList.failed=view.expanded;s.bluetoothList.failure=L"没有连上，请确认设备已开机且在附近";}
        BluetoothLayout layout(s.bluetoothList,view,406);
        PaintBluetoothPage(draw,dc,s,layout,view,hover,1,1);
    });
    RenderPageFixture(draw,L"hotspot",[&](int column,ID2D1DeviceContext* dc){
        Sample s=FixtureSample(0);s.hotspotInfo=FixtureHotspot();HotspotView view;
        if(column==0){s.hotspot=1;PaintHotspotPage(draw,dc,s,view,{PageHit::Reveal},1,1,406,false);return;}
        s.hotspot=0;s.hotspotInfo.clients.clear();view.editing=true;view.name=L"xcan";view.password=L"secret1";view.band=2;
        view.focus=HotspotView::Focus::Password;
        PaintHotspotPage(draw,dc,s,view,{PageHit::Option,L"1"},1,0,406,false);
    });
    RenderPageFixture(draw,L"sound",[&](int column,ID2D1DeviceContext* dc){
        Sample s=FixtureSample(0);s.sound=FixtureSound();SoundView view;SoundDrag drag;PageTarget hover;
        if(column==0)hover={PageHit::Slider,L"c:\\edge.exe"};else s.error=L"未能切换输出设备";
        SoundLayout layout(s.sound,406,!s.error.empty());
        PaintSoundPage(draw,dc,s,layout,view,hover,1,drag,[](std::wstring const&){return static_cast<IWICBitmapSource*>(nullptr);});
        PaintPageFooter(draw,dc,406,s.error);
    });
}
// Composites the real floating Scene (glass, edge lensing, rim and specular
// layers, content bands) over a fixture wallpaper and reads it back through
// Windows.Graphics.Capture. A BackdropBrush stands in for the host backdrop,
// which exists only behind a shown window. Never shows or captures a window;
// returns false where visual capture is unavailable.
inline bool RenderMaterial(Drawing& draw,Scene& shared){
    namespace cap=winrt::Windows::Graphics::Capture;
    using winrt::Windows::Graphics::DirectX::DirectXPixelFormat;
    using Create=HRESULT(WINAPI*)(IDXGIDevice*,void**); // out: IInspectable*
    const auto create=reinterpret_cast<Create>(GetProcAddress(GetModuleHandleW(L"d3d11.dll"),"CreateDirect3D11DeviceFromDXGIDevice"));
    if(!create||!cap::GraphicsCaptureSession::IsSupported())return false;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device{nullptr};
    winrt::check_hresult(create(draw.d3d.as<IDXGIDevice>().get(),winrt::put_abi(device)));
    const auto output=Directory()+L"\\control-panel-probe";std::filesystem::create_directories(output);
    const auto original=draw.colors;
    try{
    for(int light=1;light>=0;--light){
        const auto data=FixtureSample(0);const auto power=FixturePower();PanelLayout layout(data,0);
        const float width=PanelWidth+64,height=layout.height+64;
        Scene scene;scene.floating=true;scene.host=false;scene.radius=14;scene.full=scene.base=PanelWidth;scene.height=layout.height;
        scene.Init(nullptr,1,&shared);scene.Theme(light);scene.FloatShow(false);scene.Light(64,34);
        auto stage=scene.compositor.CreateContainerVisual();stage.Size({width,height});
        comp::CompositionDrawingSurface wallpaper{nullptr},content{nullptr};
        draw.Redraw(wallpaper,width,height,1,[&](ID2D1DeviceContext* dc){
            auto sky=draw.Gradient(dc,{0,0},{width,height},{{0,Ink(0xFFD66B)},{.45f,Ink(0xFF5F6D)},{1,Ink(0x2B1055)}});
            dc->FillRectangle({0,0,width,height},sky.get());
            draw.Round(dc,{-40,150,180,180},90,Ink(0x3DE0FF));draw.Round(dc,{200,260,240,240},120,Ink(0x0B1D51));
            draw.Round(dc,{230,20,90,90},45,Ink(0xFFFFFF));
            for(float y=300;y<380;y+=8)draw.Line(dc,20,y,230,y,Ink(0x111111,.8f),2);
        });
        stage.Children().InsertAtTop(draw.Visual(scene.compositor,wallpaper,width,height));
        stage.Children().InsertAtTop(scene.root);
        draw.colors.Set(light!=0,light?0x005FB8:0x60CDFF);
        PanelGesture gesture;PanelMotion motion;motion.Update(data,PanelPart::None,gesture,0,false);
        draw.Redraw(content,PanelWidth,layout.height,1,[&](ID2D1DeviceContext* dc){
            PaintControlPanel(draw,dc,data,power,L"13:26:08",L"10月23日 周五",L"农历九月十四",layout,PanelPart::None,gesture,motion,0);
        });
        scene.Content(content,layout.Cuts());
        auto item=cap::GraphicsCaptureItem::CreateFromVisual(stage);
        auto pool=cap::Direct3D11CaptureFramePool::CreateFreeThreaded(device,DirectXPixelFormat::B8G8R8A8UIntNormalized,2,item.Size());
        auto session=pool.CreateCaptureSession(item);session.StartCapture();
        cap::Direct3D11CaptureFrame frame{nullptr};
        const auto start=GetTickCount64();
        while(GetTickCount64()-start<3000&&!(frame&&GetTickCount64()-start>=500)){
            MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
            if(auto next=pool.TryGetNextFrame())frame=next;
            Sleep(15);
        }
        UINT fw=0,fh=0;
        auto read=[&](cap::Direct3D11CaptureFrame const& f){
            winrt::com_ptr<ID3D11Texture2D> texture;
            winrt::check_hresult(f.Surface().as<CapsuleDxgiAccess>()->GetInterface(__uuidof(ID3D11Texture2D),texture.put_void()));
            D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);fw=desc.Width;fh=desc.Height;
            desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
            winrt::com_ptr<ID3D11Texture2D> staging;winrt::check_hresult(draw.d3d->CreateTexture2D(&desc,nullptr,staging.put()));
            winrt::com_ptr<ID3D11DeviceContext> context;draw.d3d->GetImmediateContext(context.put());
            context->CopyResource(staging.get(),texture.get());
            D3D11_MAPPED_SUBRESOURCE mapped{};winrt::check_hresult(context->Map(staging.get(),0,D3D11_MAP_READ,0,&mapped));
            std::vector<BYTE> pixels(size_t(fw)*fh*4);
            for(UINT row=0;row<fh;++row)memcpy(pixels.data()+size_t(row)*fw*4,static_cast<BYTE*>(mapped.pData)+size_t(row)*mapped.RowPitch,fw*4);
            context->Unmap(staging.get(),0);return pixels;
        };
        bool saved=false;
        if(frame){
            auto pixels=read(frame);
            SavePng(draw,pixels,fw,fh,output+(light?L"\\material-light.png":L"\\material-dark.png"));saved=true;
        }
        // Filmstrips of the real reveal and leave animations, sampled by the
        // compositor's frame times.
        // Filmstrips of the reveal and leave, one static frame per time step
        // from the same timing table as the compositor animations.
        // An unchanged step produces no new frame; it repeats the last one.
        std::vector<BYTE> last;
        auto film=[&](std::function<void(double)> step,std::initializer_list<int> at,wchar_t const* name){
            std::vector<BYTE> strip;size_t column=0;unsigned captured=0;
            for(int ms:at){
                while(pool.TryGetNextFrame()){}
                step(ms);cap::Direct3D11CaptureFrame shot{nullptr};const auto begin=GetTickCount64();
                while(GetTickCount64()-begin<(last.empty()?1500:400)&&!(shot&&GetTickCount64()-begin>=150)){
                    MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
                    if(auto next=pool.TryGetNextFrame())shot=next;
                    Sleep(5);
                }
                if(shot)last=read(shot);else if(last.empty())break;
                auto const& pixels=last;++captured;
                if(strip.empty())strip.resize(size_t(fw)*at.size()*fh*4);
                for(UINT row=0;row<fh;++row)memcpy(strip.data()+(size_t(row)*at.size()+column)*fw*4,pixels.data()+size_t(row)*fw*4,fw*4);
                ++column;
            }
            Log(L"material-film-frames",long(captured));
            if(captured==at.size())SavePng(draw,strip,UINT(fw*at.size()),fh,output+name);
        };
        if(saved&&light){
            film([&](double t){scene.RevealFrame(t);},{0,40,80,120,180,260,400,700},L"\\material-reveal.png");
            film([&](double t){scene.LeaveFrame(t);},{0,40,80,120,160},L"\\material-leave.png");
        }
        session.Close();pool.Close();stage.Children().RemoveAll();scene.Close();
        if(!saved){draw.colors=original;return false;}
    }
    }catch(...){draw.colors=original;throw;}
    draw.colors=original;return true;
}
}
