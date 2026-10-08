#include <cstdio>
#include <cstdlib>
#include "quick-position-latest.h"

static int checks,failed;
static void Check(bool ok,const char* name){++checks;if(!ok){++failed;std::printf("FAIL: %s\n",name);}}
static RECT dock{300,920,1400,1000},monitor{0,0,1600,1000};
static bool enabled;
static RECT changed{};
static LRESULT CALLBACK TestProc(HWND w,UINT m,WPARAM p,LPARAM l){
    const auto result=DefWindowProcW(w,m,p,l);
    if(m==WM_WINDOWPOSCHANGING&&enabled){RECT current{};GetWindowRect(w,&current);
        LeanDockQuick::Adjust(*reinterpret_cast<WINDOWPOS*>(l),current,dock,monitor,144);
    }
    if(m==WM_WINDOWPOSCHANGED)GetWindowRect(w,&changed);
    return result;
}
int main(){
    RECT current{1000,100,1600,900};
    for(UINT dpi:{96,120,144,192}){
        WINDOWPOS p{nullptr,HWND_TOPMOST,1000,200,600,800,SWP_NOACTIVATE|SWP_NOZORDER};
        auto flags=p.flags;
        Check(LeanDockQuick::Adjust(p,current,dock,monitor,dpi),"native move adjusted");
        Check(p.x==1400+MulDiv(12,dpi,96)-600&&p.y==920+MulDiv(4,dpi,96)-800,"gap scales with DPI");
        Check(p.cx==600&&p.cy==800&&p.hwndInsertAfter==HWND_TOPMOST&&p.flags==flags,"native size z-order and activation preserved");
        RECT positioned{p.x,p.y,p.x+600,p.y+800};
        Check(!LeanDockQuick::Adjust(p,positioned,dock,monitor,dpi),"already positioned is idempotent");
    }
    WINDOWPOS show{nullptr,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW|SWP_NOACTIVATE};
    Check(LeanDockQuick::Adjust(show,current,dock,monitor,144),"show without move is anchored before visibility");
    Check(show.x==818&&show.y==126&&show.cx==0&&show.cy==0,"show reads actual dimensions");
    Check((show.flags&(SWP_SHOWWINDOW|SWP_NOACTIVATE|SWP_NOSIZE))==(SWP_SHOWWINDOW|SWP_NOACTIVATE|SWP_NOSIZE)&&!(show.flags&SWP_NOMOVE),"show flags retained");
    WINDOWPOS onlySize{nullptr,nullptr,0,0,600,600,SWP_NOMOVE|SWP_NOACTIVATE};
    Check(LeanDockQuick::Adjust(onlySize,current,dock,monitor,144)&&onlySize.y==326,"media height change reanchors bottom");
    WINDOWPOS onlyMove{nullptr,nullptr,1000,200,0,0,SWP_NOSIZE};
    Check(LeanDockQuick::Adjust(onlyMove,current,dock,monitor,144)&&onlyMove.y==126,"move keeps current dimensions");
    WINDOWPOS z{nullptr,HWND_TOPMOST,999,999,0,0,SWP_NOMOVE|SWP_NOSIZE};
    Check(!LeanDockQuick::Adjust(z,current,dock,monitor,144)&&z.x==999&&z.flags==(SWP_NOMOVE|SWP_NOSIZE),"pure z-order request not rewritten");
    WINDOWPOS tall{nullptr,nullptr,1000,100,600,980,0};
    Check(LeanDockQuick::Adjust(tall,current,dock,monitor,144)&&tall.y==0,"tall panel kept on monitor");
    RECT negativeDock{-1200,-80,-200,0},negativeMonitor{-1600,-1000,0,0};
    WINDOWPOS negative{nullptr,nullptr,-600,-900,600,800,0};
    Check(LeanDockQuick::Adjust(negative,current,negativeDock,negativeMonitor,144)&&negative.x==-782&&negative.y==-874,"negative monitor coordinates");
    RECT leftDock{0,920,400,1000};WINDOWPOS left{nullptr,nullptr,1000,0,600,800,0};
    Check(LeanDockQuick::Adjust(left,current,leftDock,monitor,144)&&left.x==0,"left edge clamp");
    RECT rightDock{1000,920,1600,1000};WINDOWPOS right{nullptr,nullptr,0,0,600,800,0};
    Check(LeanDockQuick::Adjust(right,current,rightDock,monitor,144)&&right.x==1000,"right edge clamp");
    for(auto size:{99,1601}){WINDOWPOS bad{nullptr,nullptr,1000,0,size,800,0};
        Check(!LeanDockQuick::Adjust(bad,current,dock,monitor,144)&&bad.x==1000,"invalid width leaves native request");}
    for(auto size:{99,1001}){WINDOWPOS bad{nullptr,nullptr,1000,0,600,size,0};
        Check(!LeanDockQuick::Adjust(bad,current,dock,monitor,144)&&bad.x==1000,"invalid height leaves native request");}
    for(UINT dpi:{0,47,769}){WINDOWPOS bad{nullptr,nullptr,1000,0,600,800,0};
        Check(!LeanDockQuick::Adjust(bad,current,dock,monitor,dpi),"invalid DPI leaves native request");}
    WINDOWPOS badDock{nullptr,nullptr,1000,0,600,800,0};RECT outside{-1,920,1400,1000};
    Check(!LeanDockQuick::Adjust(badDock,current,outside,monitor,144),"invalid Dock leaves native request");
    // Actual Win32 message-cycle regression, on a hidden test-owned window.
    // This checks that input-relevant WM_WINDOWPOSCHANGED receives the adjusted
    // committed coordinates. It does not claim Quick Settings UI acceptance.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW wc{};wc.lpfnWndProc=TestProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"LeanDockPositionTest";
    Check(RegisterClassW(&wc)!=0,"hidden test window registered");
    HWND w=CreateWindowW(wc.lpszClassName,L"",WS_POPUP,0,0,600,800,nullptr,nullptr,wc.hInstance,nullptr);
    Check(w!=nullptr,"hidden test window created");enabled=true;
    for(int i=0;i<5;++i){
        Check(SetWindowPos(w,nullptr,1000,200,600,800,SWP_NOACTIVATE|SWP_NOZORDER)!=0,"real native move accepted");
        RECT actual{};GetWindowRect(w,&actual);
        Check(actual.left==818&&actual.top==126&&actual.right==1418&&actual.bottom==926,"OS commits target coordinates directly");
        Check(EqualRect(&actual,&changed),"native changed message sees same coordinates");
        Check(!IsWindowVisible(w),"test window stays hidden");
    }
    enabled=false;SetWindowPos(w,nullptr,1000,200,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
    RECT restored{};GetWindowRect(w,&restored);
    Check(restored.left==1000&&restored.top==200,"disabled policy restores native movement");
    DestroyWindow(w);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    std::printf("Quick position: %d checks, %d failures\n",checks,failed);return failed?1:0;
}
