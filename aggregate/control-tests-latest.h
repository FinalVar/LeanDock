#pragma once
#include "control-panel-latest.h"
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
inline int ControlsProbe(){
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    const auto checks=VerifyControlModel(),themeChecks=VerifyThemeModel(),metricChecks=VerifyMetricModel();
    const auto themeBefore=ThemeControl::Read();
    Metrics metrics;metrics.Start(nullptr);metrics.Visible(true);
    const auto deadline=GetTickCount64()+6000;
    Sample data;
    do{Sleep(100);data=metrics.Read();}while(GetTickCount64()<deadline&&(data.reads<2||data.wifi<0||data.bluetooth<0));
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
        "\"darkTheme\":%d,\"hotspot\":%d,\"hotspotCapability\":%d,\"hotspotClients\":%u,\"themePolicyChecks\":%u}\n",
        checks,Elevated()?"false":"true",data.volume,data.mute?"true":"false",data.brightness,data.wifi,data.bluetooth,
        data.reads,idle.writes,noHiddenReads?"true":"false",readonly?"true":"false",stopMs,
        data.darkTheme,data.hotspot,data.hotspotCapability,data.hotspotClients,themeChecks);fclose(f);}
    winrt::uninit_apartment();
    return readonly&&noHiddenReads&&data.reads>=2&&stopMs<2000?0:1;
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
                PaintControlPanel(draw,dc.get(),s,p,L"13:26:08",L"10月23日",L"农历九月十四",layout,hover,gesture,motion,0);
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
            PaintControlPanel(draw,dc,data,power,L"13:26:08",L"10月23日",L"农历九月十四",layout,PanelPart::None,gesture,motion,0);
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
