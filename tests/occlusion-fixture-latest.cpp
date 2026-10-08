#include "../include/dock-geometry-latest.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
// Own non-activating show operations only. No input injection, no other app's
// window is moved, hidden, closed or sent a command. Lifetime is bounded.
static void Pump(unsigned ms){auto end=GetTickCount64()+ms;MSG m{};while(GetTickCount64()<end){MsgWaitForMultipleObjects(0,nullptr,FALSE,25,QS_ALLINPUT);while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}}}
static long Value(HWND w,const wchar_t* name){auto p=reinterpret_cast<intptr_t>(GetPropW(w,name));return p?p-1:-1;}
int main(int argc,char** argv){
 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 HWND bar=FindWindowW(L"Shell_TrayWnd",nullptr);RECT dock{};MONITORINFO monitor{};
 if(!bar||!LeanDockGeometry::Read(bar,dock,&monitor))return 2;
 WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"LeanDock.OcclusionFixture.v1";wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&wc);
 HWND w=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,wc.lpszClassName,L"LeanDock bounded occlusion test",WS_POPUP,0,0,100,100,nullptr,nullptr,wc.hInstance,nullptr);
 if(!w)return 3;
 // Consume STARTUPINFO's hidden state for this owned window, then show without
 // activation. WS_EX_NOACTIVATE is intentionally not used: the hide policy
 // correctly excludes no-activate overlays from ordinary obstruction.
 ShowWindow(w,SW_HIDE);
 if(argc==3&&!strcmp(argv[1],"--hold-overlap")){
  unsigned seconds=std::clamp(atoi(argv[2]),5,90);
  SetWindowPos(w,HWND_TOPMOST,dock.left+20,dock.top-120,100,180,SWP_NOACTIVATE|SWP_SHOWWINDOW);
  Pump(seconds*1000);DestroyWindow(w);return 0;
 }
 struct Stage{const char* name;RECT rect;bool keep;};
 Stage stages[]={
  {"left-outside",{monitor.rcMonitor.left+10,dock.top-120,dock.left-10,monitor.rcMonitor.bottom},true},
  {"left-one-pixel",{dock.left-80,dock.top-100,dock.left+1,monitor.rcMonitor.bottom},false},
  {"above-touching",{dock.left,dock.top-150,dock.right,dock.top},true},
  {"center-overlap",{dock.left+10,dock.top-120,dock.right-10,monitor.rcMonitor.bottom},false},
  {"right-outside",{dock.right+10,dock.top-120,monitor.rcMonitor.right-10,monitor.rcMonitor.bottom},true},
  {"full-monitor",monitor.rcMonitor,false},
  {"restored-above",{dock.left,dock.top-200,dock.right,dock.top-20},true}
 };
 unsigned failed=0;printf("[\n");
 for(unsigned i=0;i<std::size(stages);++i){auto s=stages[i];
  SetWindowPos(w,HWND_TOPMOST,s.rect.left,s.rect.top,s.rect.right-s.rect.left,s.rect.bottom-s.rect.top,SWP_NOACTIVATE|SWP_SHOWWINDOW);
  Pump(2400);auto keep=Value(bar,L"LeanDock.Hide.KeepShown");auto precise=Value(bar,L"LeanDock.Hide.Precise");
  const bool ok=keep==s.keep&&precise==1;if(!ok)++failed;
  RECT actual{};GetWindowRect(w,&actual);
  printf("%s{\"stage\":\"%s\",\"expectedKeep\":%d,\"keep\":%ld,\"precise\":%ld,\"cardVisible\":%ld,\"window\":[%ld,%ld,%ld,%ld],\"passed\":%s}",i?",\n":"",s.name,int(s.keep),keep,precise,Value(bar,L"LeanDock.Card.Visible"),actual.left,actual.top,actual.right,actual.bottom,ok?"true":"false");fflush(stdout);
 }
 DestroyWindow(w);Pump(1200);printf("\n]\n");return failed?1:0;
}
