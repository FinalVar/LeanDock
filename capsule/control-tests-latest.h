#pragma once
#include "control-panel-latest.h"


namespace Capsule {
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
    check(!PanelEnabled(PanelPart::DarkTheme,state));check(PanelEnabled(PanelPart::DarkThemeMore,state));
    state.themeBusy=false;state.darkTheme=2;check(PanelEnabled(PanelPart::DarkTheme,state));
    state.darkTheme=-1;check(!PanelEnabled(PanelPart::DarkTheme,state));
    return count;
}
inline unsigned VerifyControlModel(){
    unsigned count=0;
    auto check=[&](bool value){++count;if(!value)winrt::throw_hresult(E_UNEXPECTED);};
    for(auto scale:{1.f,1.25f,1.5f,1.75f,2.f}){
        auto hit=[&](float x,float y){return PanelHit((x*scale)/scale,(y*scale)/scale);};
        check(hit(20,80)==PanelPart::Wifi);check(hit(135,80)==PanelPart::WifiMore);
        check(hit(170,80)==PanelPart::Bluetooth);check(hit(275,80)==PanelPart::BluetoothMore);
        check(hit(24,182)==PanelPart::Mute);check(hit(140,182)==PanelPart::Volume);
        check(hit(140,224)==PanelPart::Brightness);check(hit(150,80)==PanelPart::None);
        check(hit(14,286)==PanelPart::None);check(hit(140,252)==PanelPart::None);
        check(hit(20,130)==PanelPart::DarkTheme);check(hit(135,130)==PanelPart::DarkThemeMore);
        check(hit(170,130)==PanelPart::Hotspot);check(hit(275,130)==PanelPart::HotspotMore);
        check(hit(30,107)==PanelPart::None);check(hit(150,130)==PanelPart::None);
    }
    check(SliderValue(-100)==0);check(SliderValue(500)==100);
    check(SliderValue(SliderLeft)==0);check(SliderValue(SliderRight)==100);
    check(SliderValue((SliderLeft+SliderRight)/2)==50);
    PanelGesture g;g.Begin(PanelPart::Brightness,60);
    check(g.Dragging());check(g.Send(60,100));check(!g.Send(180,120));
    check(g.preview==65);check(g.Send(180,180));check(!g.Send(180,280));
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
    check(PanelEnabled(PanelPart::DarkThemeMore,state));check(PanelEnabled(PanelPart::HotspotMore,state));
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
    Sample s;PanelLayout empty(s,0);check(empty.mediaY<0&&empty.temperatureY<0&&empty.trayY<0);
    s.media=L"▶ 测试标题";s.error=L"设备已断开";PanelLayout content(s,7);
    check(content.mediaY>=0&&content.errorY>content.mediaY&&content.trayY>content.errorY);
    check(content.height>empty.height&&content.trayY+52<content.height);
    return count;
}
inline int ControlsProbe(){
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    const auto checks=VerifyControlModel(),themeChecks=VerifyThemeModel();
    const auto themeBefore=ThemeControl::Read();
    Metrics metrics;metrics.Start(nullptr);metrics.Visible(true);
    const auto deadline=GetTickCount64()+6000;
    Sample data;
    do{Sleep(100);data=metrics.Read();}while(GetTickCount64()<deadline&&(data.reads<2||data.wifi<0||data.bluetooth<0));
    metrics.Visible(false);Sleep(300);auto hidden=metrics.Read();Sleep(1200);auto idle=metrics.Read();
    const auto beforeStop=GetTickCount64();metrics.Stop();const auto stopMs=GetTickCount64()-beforeStop;metrics.Stop();
    const bool readonly=data.writes==0&&idle.writes==0&&themeBefore==ThemeControl::Read();
    const bool noHiddenReads=hidden.reads==idle.reads&&hidden.detailReads==idle.detailReads;
    FILE* f=_wfopen((Directory()+L"\\controls-probe-latest.json").c_str(),L"w");
    if(f){fprintf(f,"{\"policyChecks\":%u,\"normalUser\":%s,\"volume\":%d,\"mute\":%s,\"brightness\":%d,"
        "\"wifi\":%d,\"bluetooth\":%d,\"reads\":%llu,\"writes\":%llu,\"hiddenReadsUnchanged\":%s,"
        "\"readOnly\":%s,\"stopMs\":%llu,\"visibleWindows\":false,\"actualControlWritesTested\":false,"
        "\"darkTheme\":%d,\"hotspot\":%d,\"hotspotCapability\":%d,\"hotspotClients\":%u,\"themePolicyChecks\":%u}\n",
        checks,Elevated()?"false":"true",data.volume,data.mute?"true":"false",data.brightness,data.wifi,data.bluetooth,
        data.reads,idle.writes,noHiddenReads?"true":"false",readonly?"true":"false",stopMs,
        data.darkTheme,data.hotspot,data.hotspotCapability,data.hotspotClients,themeChecks);fclose(f);}
    winrt::uninit_apartment();
    return readonly&&noHiddenReads&&data.reads>=2&&stopMs<2000?0:1;
}
inline void RenderControlPanel(Drawing& draw){
    VerifyControlModel();const auto output=Directory()+L"\\control-panel-probe";
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
                draw.colors.light=column==0;
                draw.colors.text=Ink(column?0xECEEF2:0x15171B);draw.colors.secondary=Ink(column?0x9EA5B1:0x5E6571);
                draw.colors.accent=Ink(column?0x3CC2B7:0x0A7470);draw.colors.hair=Ink(column?0xFFFFFF:0x141820,.10f);
                draw.colors.hover=Ink(column?0xFFFFFF:0x141820,.08f);
                Sample s;s.volume=column?-1:42;s.brightness=column?-1:78;s.wifi=column?3:1;s.bluetooth=column?-1:0;
                s.darkTheme=column?-1:1;s.themeAvailable=!column;s.hotspot=column?-1:1;s.hotspotLabel=column?L"未连接网络":L"2 台已连接";
                s.cpu=24;s.memory=67;s.gpu=15;s.capacity=L"10.4 / 15.4 GB";s.gpuName=L"AMD Radeon(TM) Graphics";
                s.network=L"↓ 12.3 MB/s　↑ 84 KB/s";s.disks=L"C 123 GB　D 429 GB";
                s.battery=L"插电中，约 20 分钟充满";
                if(column){s.media=L"▶ 一首正在播放的音乐";s.error=L"无线控制未获系统授权";}
                Power p;p.valid=true;p.value.BatteryFlag=0;p.value.BatteryLifePercent=97;p.value.ACLineStatus=1;
                PanelLayout layout(s,0);PanelGesture gesture;
                dc->SetTransform(D2D1::Matrix3x2F::Translation(14+column*316.f,14));
                draw.Round(dc.get(),{0,0,PanelWidth,layout.height},14,Ink(column?0x282D37:0xEEF1F6));
                PaintControlPanel(draw,dc.get(),s,p,L"13:26",L"10月22日",L"农历九月十三",layout,PanelPart::None,gesture);
            }
            dc->SetTransform(D2D1::Matrix3x2F::Identity());winrt::check_hresult(dc->EndDraw());dc->SetTarget(nullptr);
            props.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            winrt::com_ptr<ID2D1Bitmap1> readback;winrt::check_hresult(dc->CreateBitmap({width,height},nullptr,0,&props,readback.put()));
            winrt::check_hresult(readback->CopyFromBitmap(nullptr,target.get(),nullptr));D2D1_MAPPED_RECT mapped{};
            winrt::check_hresult(readback->Map(D2D1_MAP_OPTIONS_READ,&mapped));
            std::vector<BYTE> pixels(size_t(width)*height*4);
            for(UINT row=0;row<height;++row)memcpy(pixels.data()+size_t(row)*width*4,mapped.bits+size_t(row)*mapped.pitch,width*4);
            readback->Unmap();winrt::com_ptr<IWICBitmap> bitmap;
            winrt::check_hresult(draw.wic->CreateBitmapFromMemory(width,height,GUID_WICPixelFormat32bppPBGRA,width*4,UINT(pixels.size()),pixels.data(),bitmap.put()));
            winrt::com_ptr<IWICStream> stream;winrt::check_hresult(draw.wic->CreateStream(stream.put()));
            const auto name=output+L"\\dpi-"+std::to_wstring(int(scale*100))+L".png";
            winrt::check_hresult(stream->InitializeFromFilename(name.c_str(),GENERIC_WRITE));
            winrt::com_ptr<IWICBitmapEncoder> encoder;winrt::check_hresult(draw.wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.put()));
            winrt::check_hresult(encoder->Initialize(stream.get(),WICBitmapEncoderNoCache));
            winrt::com_ptr<IWICBitmapFrameEncode> frame;winrt::check_hresult(encoder->CreateNewFrame(frame.put(),nullptr));
            winrt::check_hresult(frame->Initialize(nullptr));winrt::check_hresult(frame->SetSize(width,height));
            WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;winrt::check_hresult(frame->SetPixelFormat(&format));
            winrt::check_hresult(frame->WriteSource(bitmap.get(),nullptr));winrt::check_hresult(frame->Commit());winrt::check_hresult(encoder->Commit());
        }
    }catch(...){draw.colors=original;throw;}draw.colors=original;
}
}
