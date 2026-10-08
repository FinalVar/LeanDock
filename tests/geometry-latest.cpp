#include "../include/dock-geometry-latest.h"
#include <cstdio>
#include <thread>
#include <atomic>
int main() {
    using namespace LeanDockGeometry;
    unsigned checks=0,failed=0;
    auto check=[&](bool ok){++checks;if(!ok)++failed;};
    for(LONG origin:{-2560L,0L,2560L})for(int dpi:{96,120,144,192}) {
        RECT mon{origin,-200,origin+2560,1400};
        RECT dock{origin+600,1400-MulDiv(64,dpi,96),origin+1960,1400};
        check(!InSpan(dock,dock.left-1));check(InSpan(dock,dock.left));
        check(InSpan(dock,dock.right-1));check(!InSpan(dock,dock.right));
        auto p=HostPosition(dock,mon,800,600,dpi,false);
        check(p.x==origin+880 && p.y+600-MulDiv(12,dpi,96)==dock.top-MulDiv(8,dpi,96));
        p=HostPosition(dock,mon,576,600,dpi,true);
        check(p.x+576-MulDiv(12,dpi,96)==dock.right);
        p=HostPosition(dock,mon,2560,1400,dpi,false);
        check(p.x==mon.left);
    }
    check(!ValidSpan(0,0,64,2560));check(!ValidSpan(-1,1200,64,2560));
    check(!ValidSpan(600,2561,64,2560));check(!ValidSpan(600,1900,1000,2560));
    check(ValidSpan(600,1900,96,2560));
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW type{};type.lpfnWndProc=DefWindowProcW;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"LeanDockGeometryTest";
    RegisterClassW(&type);
    HWND window=CreateWindowW(type.lpszClassName,L"",WS_POPUP,0,0,1000,100,nullptr,nullptr,type.hInstance,nullptr);
    check(window!=nullptr);
    if(window){
        RECT value{};Publish(window,100,700,64);check(Read(window,value));
        auto revision=Revision(window);Publish(window,100,700,64);check(Revision(window)==revision);
        SetPropW(window,Ready,reinterpret_cast<HANDLE>(revision+1));check(!Read(window,value));
        Publish(window,100,700,64);check(Read(window,value)); // recover incomplete publication
        std::atomic<bool> done=false;std::atomic<unsigned> torn=0;
        std::thread writer([&]{for(int i=0;i<20000;++i){Publish(window,100,700,64);Publish(window,200,900,96);}done=true;});
        unsigned reads=0;
        do{if(Read(window,value)){++reads;
            if(!((value.left==100&&value.right==700&&value.bottom-value.top==64)||
                 (value.left==200&&value.right==900&&value.bottom-value.top==96)))++torn;
        }}while(!done);
        writer.join();check(torn==0);check(reads>0);check(!IsWindowVisible(window));
        Clear(window);check(!Read(window,value));DestroyWindow(window);
    }
    UnregisterClassW(type.lpszClassName,type.hInstance);
    printf("%u geometry checks, %u failures\n",checks,failed);return failed?1:0;
}
