// Real-desktop material check: shows the production floating Scene with
// fixture panel content near the bottom-right of the primary monitor for about
// a second, reads that screen area with GDI and writes PNGs. Unlike the hidden
// probe this needs the real host backdrop, so it does show windows; it never
// sends input, takes focus, or touches the running panel or Explorer.
// Usage: latest.exe --glass-check <output dir> [--raw] [--backdrop] [--light|--dark] [--wifi]
//        [--name suffix] [--set field=value ...] (Scene::Look fields, alpha 0-255)
// --wifi shows the Wi-Fi page with fixture networks instead of the main page.
#define wWinMain AggregateProductionEntry
#include "../aggregate/latest.cpp"
#undef wWinMain
#include <shellscalingapi.h>

namespace {
using namespace Capsule;
LRESULT CALLBACK BackdropProc(HWND w,UINT m,WPARAM a,LPARAM b){
    if(m!=WM_PAINT)return DefWindowProcW(w,m,a,b);
    PAINTSTRUCT p;HDC dc=BeginPaint(w,&p);RECT r;GetClientRect(w,&r);
    const COLORREF stops[]={RGB(255,214,107),RGB(255,95,109),RGB(43,16,85)};
    for(int y=0;y<r.bottom;y+=4){
        const float t=float(y)/std::max<LONG>(1,r.bottom)*2;const int i=std::min(1,int(t));const float f=t-i;
        auto mix=[&](int s){return int(GetRValue(stops[i])*(1-f)+GetRValue(stops[i+1])*f)|int(GetGValue(stops[i])*(1-f)+GetGValue(stops[i+1])*f)<<8|
            int(GetBValue(stops[i])*(1-f)+GetBValue(stops[i+1])*f)<<16;(void)s;};
        HBRUSH brush=CreateSolidBrush(COLORREF(mix(0)));RECT band{0,y,r.right,y+4};FillRect(dc,&band,brush);DeleteObject(brush);
    }
    auto disc=[&](int x,int y,int d,COLORREF c){HBRUSH b=CreateSolidBrush(c);auto old=SelectObject(dc,b);SelectObject(dc,GetStockObject(NULL_PEN));
        Ellipse(dc,x,y,x+d,y+d);SelectObject(dc,old);DeleteObject(b);};
    disc(-60,r.bottom/3,r.right/2,RGB(61,224,255));disc(r.right/2,r.bottom/2,r.right*2/3,RGB(11,29,81));disc(r.right*2/3,30,r.right/4,RGB(255,255,255));
    HFONT font=CreateFontW(-22,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
    for(int y=40,i=0;y<r.bottom;y+=56,++i){
        SetTextColor(dc,i%2?RGB(255,255,255):RGB(10,10,10));
        TextOutW(dc,24+(i%3)*30,y,L"液态玻璃 Liquid Glass 1234567890 ABCDEFG",38);
    }
    SelectObject(dc,old);DeleteObject(font);EndPaint(w,&p);return 0;
}
void Pump(uint64_t ms){
    const auto end=GetTickCount64()+ms;
    while(GetTickCount64()<end){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(5);}
}
void Grab(Drawing& draw,RECT r,std::wstring const& name){
    const int w=r.right-r.left,h=r.bottom-r.top;HDC screen=GetDC(nullptr);HDC mem=CreateCompatibleDC(screen);
    BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),w,-h,1,32,BI_RGB};void* bits=nullptr;
    HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto old=SelectObject(mem,bitmap);
    BitBlt(mem,0,0,w,h,screen,r.left,r.top,SRCCOPY|CAPTUREBLT);
    std::vector<BYTE> pixels(size_t(w)*h*4);memcpy(pixels.data(),bits,pixels.size());
    for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;
    SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);ReleaseDC(nullptr,screen);
    SavePng(draw,pixels,UINT(w),UINT(h),name);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);
    if(count<3||wcscmp(args[1],L"--glass-check")){LocalFree(args);return 2;}
    const std::wstring output=args[2];bool raw=false,backdrop=false,wifi=false;int theme=-1;std::wstring suffix;
    std::vector<std::pair<std::wstring,float>> sets;
    for(int i=3;i<count;++i){
        if(!wcscmp(args[i],L"--raw"))raw=true;else if(!wcscmp(args[i],L"--backdrop"))backdrop=true;
        else if(!wcscmp(args[i],L"--wifi"))wifi=true;
        else if(!wcscmp(args[i],L"--light"))theme=1;else if(!wcscmp(args[i],L"--dark"))theme=0;
        else if(!wcscmp(args[i],L"--name")&&i+1<count)suffix=args[++i];
        else if(!wcscmp(args[i],L"--set")&&i+1<count){std::wstring v=args[++i];auto at=v.find(L'=');
            if(at!=std::wstring::npos)sets.emplace_back(v.substr(0,at),std::wcstof(v.c_str()+at+1,nullptr));}
    }
    LocalFree(args);
    int result=0;
    try{
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        std::filesystem::create_directories(output);
        POINT origin{0,0};HMONITOR monitor=MonitorFromPoint(origin,MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mon{sizeof(mon)};GetMonitorInfoW(monitor,&mon);UINT dx=96,dy=96;GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&dx,&dy);
        const float dpi=dx/96.f;
        const auto data=FixtureSample(0);const auto power=FixturePower();PanelLayout layout(data,0);
        {Drawing draw;Popup popup;popup.Init(draw,dpi,PanelWidth,layout.height,14,false);
        if(theme>=0){popup.scene.Theme(theme);draw.colors.Set(theme!=0,theme?0x005FB8:0x60CDFF);}
        if(!sets.empty()){
            DWORD light=1,size=sizeof(light);
            RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"SystemUsesLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);
            auto& look=popup.scene.look;look=Scene::Defaults(theme>=0?theme!=0:light!=0);
            auto alpha=[](Color& c,float v){c.A=uint8_t(std::clamp(v,0.f,255.f));};
            for(auto const& [name,v]:sets){
                if(name==L"saturation")look.saturation=v;else if(name==L"amplitude")look.amplitude=v;
                else if(name==L"exponent")look.exponent=v;else if(name==L"offset")look.offset=v;
                else if(name==L"tint")alpha(look.tint,v);
                else if(name==L"shade")alpha(look.shade,v);else if(name==L"hairline")alpha(look.hairline,v);
            }
            popup.scene.tuned=true;popup.scene.Theme(theme);
        }
        // Mid-bottom of the work area: what usually sits behind a panel.
        const double left=(mon.rcWork.left+mon.rcWork.right)/2.0-PanelWidth/2.0*dpi,top=mon.rcWork.bottom-(layout.height+10)*dpi;
        popup.Position(left,top);
        HWND fixture=nullptr;
        if(backdrop){
            WNDCLASSW c{};c.lpfnWndProc=BackdropProc;c.hInstance=instance;c.lpszClassName=L"LeanDock.GlassCheck.Backdrop";RegisterClassW(&c);
            fixture=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_LAYERED,c.lpszClassName,L"",WS_POPUP,
                int(left-60*dpi),int(top-40*dpi),int((PanelWidth+120)*dpi),int((layout.height+50)*dpi),nullptr,nullptr,instance,nullptr);
            SetLayeredWindowAttributes(fixture,0,255,LWA_ALPHA);
            ShowWindow(fixture,SW_SHOWNOACTIVATE);UpdateWindow(fixture);Pump(150);
        }
        PanelGesture gesture;PanelMotion motion;motion.Update(data,PanelPart::None,gesture,0,false);
        if(raw){
            // The host backdrop exactly as delivered: no tone, tint, rim or
            // content.
            auto& s=popup.scene;s.glass.Brush(s.Masked(s.Glass(0,1),s.shape));s.tint.Color({0,0,0,0});
        }else popup.Paint([&](ID2D1DeviceContext* dc){
            PaintControlPanel(draw,dc,data,power,L"13:26:08",L"10月23日",L"农历九月十四",layout,PanelPart::None,gesture,motion,0);
        },layout.Cuts());
        if(wifi&&!raw){
            Sample s=data;s.wifiList=FixtureNetworks();WifiView view;view.expanded=L"@NRadio-B066-2.4G";view.password=L"secret12";
            view.focus=WifiView::Focus::Password;WifiLayout rows(s.wifiList,view,layout.height);
            popup.PaintPage([&](ID2D1DeviceContext* dc){PaintWifiPage(draw,dc,s,rows,view,{PageHit::Row,L"LANSVEHD"},1,1);});
            popup.scene.Navigate(true,false);
        }
        popup.scene.Light(60,30);
        popup.Show(true);Pump(1100);
        RECT area{LONG(left-40*dpi),LONG(top-30*dpi),LONG(left+(PanelWidth+40)*dpi),LONG(top+(layout.height+10)*dpi)};
        std::wstring name=output+L"\\glass"+(raw?L"-raw":L"")+(wifi?L"-wifi":L"")+(backdrop?L"-fixture":L"-desktop")+(theme==0?L"-dark":theme==1?L"-light":L"")+
            (suffix.empty()?L"":L"-"+suffix)+L".png";
        Grab(draw,area,name);
        popup.Hide();if(fixture)DestroyWindow(fixture);Pump(50);popup.Destroy();}
    }catch(...){Log(L"glass-check-failed",winrt::to_hresult());result=1;}
    winrt::uninit_apartment();return result;
}
