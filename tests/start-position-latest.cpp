#include "start-position-latest.h"
#include <cstdio>
#include <cstdlib>
static unsigned checks,failed;
static void Check(bool ok,const char* message){++checks;if(!ok){++failed;std::printf("FAIL %s\n",message);}}
static RECT screen{0,0,2560,1600},dock{800,1504,1760,1600};
static bool enabled;static RECT committed{};
static LRESULT CALLBACK Proc(HWND w,UINT m,WPARAM a,LPARAM b){
    auto result=DefWindowProcW(w,m,a,b);
    if(enabled&&m==WM_WINDOWPOSCHANGING){RECT r{};GetWindowRect(w,&r);LeanDockStartPlacement::Adjust(*reinterpret_cast<WINDOWPOS*>(b),r,dock,screen,144);}
    if(m==WM_WINDOWPOSCHANGED)GetWindowRect(w,&committed);
    return result;
}
int main(){
    const RECT native{0,0,2560,1528};
    for(UINT dpi:{96,120,144,168,192}){
        const LONG bottom=dock.top-MulDiv(8,dpi,96);
        for(LONG height:{500,1131,1528,1600}){
            WINDOWPOS p{nullptr,HWND_TOPMOST,0,0,1287,height,SWP_NOACTIVATE|SWP_NOZORDER};RECT original{};
            Check(LeanDockStartPlacement::Adjust(p,native,dock,screen,dpi,&original),"pending native layout adapted");
            Check(std::abs(2*p.x+p.cx-dock.left-dock.right)<=1&&p.y+p.cy==bottom,"center and exact physical gap");
            Check(p.y>=screen.top&&p.cy==std::min(height,bottom),"tall host fits above Dock");
            Check(p.cx==1287&&(p.flags&SWP_NOACTIVATE)&&(p.flags&SWP_NOZORDER)&&p.hwndInsertAfter==HWND_TOPMOST,"native width activation and z preserved");
            Check(original.left==0&&original.top==0&&original.right==1287&&original.bottom==height,"rollback retains full native request");
            RECT actual{p.x,p.y,p.x+p.cx,p.y+p.cy};
            Check(!LeanDockStartPlacement::Adjust(p,actual,dock,screen,dpi),"repeated layout idempotent");
        }
        WINDOWPOS show{nullptr,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW|SWP_NOACTIVATE};
        Check(LeanDockStartPlacement::Adjust(show,native,dock,screen,dpi),"show-only full-height host reserved");
        Check(show.x==0&&show.y==0&&show.cx==2560&&show.cy==bottom&&!(show.flags&SWP_NOSIZE),"full host is resized once before showing");
        Check((show.flags&(SWP_SHOWWINDOW|SWP_NOACTIVATE))==(SWP_SHOWWINDOW|SWP_NOACTIVATE),"show semantics retained");
    }
    RECT negativeScreen{-2560,-1600,0,0},negativeDock{-1760,-96,-800,0};
    WINDOWPOS n{nullptr,nullptr,-2000,-1400,1287,1500,0};
    Check(LeanDockStartPlacement::Adjust(n,native,negativeDock,negativeScreen,144)&&n.y+n.cy==-108&&n.y>=-1600,"negative monitor origin");
    for(UINT dpi:{0,47,769}){WINDOWPOS p{nullptr,nullptr,0,0,600,800,0};Check(!LeanDockStartPlacement::Adjust(p,native,dock,screen,dpi),"invalid DPI leaves native state");}
    WINDOWPOS z{nullptr,nullptr,42,42,0,0,SWP_NOMOVE|SWP_NOSIZE};
    Check(!LeanDockStartPlacement::Adjust(z,native,dock,screen,144)&&z.x==42,"pure z-order unchanged");
    WINDOWPOS invalid{nullptr,nullptr,0,0,2570,1528,0};
    Check(!LeanDockStartPlacement::Adjust(invalid,native,dock,screen,144),"invalid width not resized");
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW c{};c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"LeanDock.StartPosition.HiddenTest";c.lpfnWndProc=Proc;
    Check(RegisterClassW(&c)!=0,"hidden window registration");
    HWND w=CreateWindowW(c.lpszClassName,L"",WS_POPUP,0,0,2560,1528,nullptr,nullptr,c.hInstance,nullptr);
    Check(w!=nullptr,"hidden window creation");enabled=true;
    for(LONG height:{1528,1131,1600}){
        Check(SetWindowPos(w,nullptr,0,0,1287,height,SWP_NOACTIVATE|SWP_NOZORDER)!=FALSE,"native request accepted");
        RECT r{};GetWindowRect(w,&r);
        Check(r.bottom==1492&&r.top>=0&&r.right-r.left==1287&&EqualRect(&r,&committed),"commit and input coordinates agree");
        Check(!IsWindowVisible(w),"test never shown");
    }
    enabled=false;SetWindowPos(w,nullptr,native.left,native.top,2560,1528,SWP_NOACTIVATE|SWP_NOZORDER);
    RECT restored{};GetWindowRect(w,&restored);Check(EqualRect(&restored,&native),"disable restores native full size");
    DestroyWindow(w);UnregisterClassW(c.lpszClassName,c.hInstance);
    std::printf("Start placement: %u checks, %u failures\n",checks,failed);return failed?1:0;
}
