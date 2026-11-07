#include "../include/dock-geometry-latest.h"
#include <cstdio>
#include <thread>
#include <atomic>
#include <cmath>
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
        // One gap for everything: the same line as the shell windows' visible edge.
        check(PopupBottom(dock,dpi)==dock.top-MulDiv(8,dpi,96));
        check(HostPosition(dock,mon,800,600,dpi,false).y+600-MulDiv(12,dpi,96)==PopupBottom(dock,dpi));
        {   const LONG w=MulDiv(300,dpi,96),h=MulDiv(408,dpi,96),middle=(dock.left+dock.right)/2,margin=MulDiv(8,dpi,96);
            // In the middle of the Dock: centered on the part, on the shared line.
            auto at=PopupFromPart({middle-50,dock.top+10,middle+50,dock.bottom-10},dock,mon,w,h,dpi);
            check(std::abs(at.x+w/2.0-middle)<=1&&at.y+h==PopupBottom(dock,dpi));
            // At either end: pushed inward until flush with the Dock.
            at=PopupFromPart({dock.right-200,dock.top+10,dock.right-100,dock.bottom-10},dock,mon,w,h,dpi);
            check(at.x+w==dock.right&&at.x<=dock.right-200);
            at=PopupFromPart({dock.left,dock.top+10,dock.left+100,dock.bottom-10},dock,mon,w,h,dpi);
            check(at.x==dock.left);
            // A Dock narrower than the popup leaves it centered on the part.
            const RECT narrow{middle-60,dock.top,middle+60,dock.bottom};
            at=PopupFromPart({middle-50,dock.top+10,middle+50,dock.bottom-10},narrow,mon,w,h,dpi);
            check(std::abs(at.x+w/2.0-middle)<=1);
            // The screen edge wins over the part and the Dock.
            const RECT wide{mon.left,dock.top,mon.right,dock.bottom};
            at=PopupFromPart({mon.right-100,dock.top+10,mon.right,dock.bottom-10},wide,mon,w,h,dpi);
            check(at.x+w==mon.right-margin);
            at=PopupFromPart({mon.left,dock.top+10,mon.left+100,dock.bottom-10},wide,mon,w,h,dpi);
            check(at.x==mon.left+margin);
            // Too tall for the space: keeps the top on screen.
            at=PopupFromPart({middle-50,dock.top+10,middle+50,dock.bottom-10},dock,mon,w,5000,dpi);
            check(at.y==mon.top+margin);
        }
        // Measured hidden or mid-slide, a Y still lands where the Dock comes to rest.
        {   const LONG tall=dock.bottom-dock.top;
            const RECT shown{mon.left,mon.bottom-tall,mon.right,mon.bottom},hidden{mon.left,mon.bottom-2,mon.right,mon.bottom-2+tall};
            const RECT sliding{mon.left,mon.bottom-tall+37,mon.right,mon.bottom+37};
            check(ShownShift(shown,mon)==0);
            check(hidden.top+11+ShownShift(hidden,mon)==shown.top+11);
            check(sliding.top+11+ShownShift(sliding,mon)==shown.top+11);
        }
        RECT side{mon.left, dock.top, dock.left, mon.bottom};
        check(!Overlaps(side,dock)); // merely touching the left edge
        ++side.right;check(Overlaps(side,dock)); // one physical pixel inside
        side={dock.right,dock.top,mon.right,mon.bottom};check(!Overlaps(side,dock));
        --side.left;check(Overlaps(side,dock));
        side={dock.left,mon.top,dock.right,dock.top};check(!Overlaps(side,dock));
        ++side.bottom;check(Overlaps(side,dock));
        check(Overlaps(mon,dock));check(!Overlaps(RECT{0,0,0,0},dock));
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
        RECT fallback{0,1,1000,100};
        check(!OcclusionRect(window,reinterpret_cast<HMONITOR>(uintptr_t(1)),fallback));
        check(fallback.left==0&&fallback.top==1&&fallback.right==1000&&fallback.bottom==100);
        check(OcclusionRect(window,MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),fallback));
        check(fallback.left==100&&fallback.right==700);
        SetWindowPos(window,nullptr,0,1000,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        RECT projected{};check(Read(window,projected)&&EqualRect(&fallback,&projected));
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
