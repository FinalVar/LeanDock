#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <array>
#include "cards-preferences-latest.h"

namespace {
using namespace LeanDockCards;
constexpr wchar_t WindowClass[]=L"LeanDockCardsSettings";
enum {ListId=100,UpId,DownId,SaveId,CancelId};
struct Settings {
    std::wstring root;HWND window=nullptr,list=nullptr;HFONT font=nullptr;UINT dpi=96;
    std::array<Page,5> order{};
    int Px(int n)const{return MulDiv(n,dpi,96);}
    HWND Child(LPCWSTR type,LPCWSTR text,DWORD style,int id,int x,int y,int w,int h) {
        auto control=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,Px(x),Px(y),Px(w),Px(h),window,
            reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        if(control)SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return control;
    }
    void Populate(std::array<bool,5> const& enabled,int selected) {
        ListView_DeleteAllItems(list);
        for(int i=0;i<5;++i){LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=i;item.pszText=const_cast<PWSTR>(PageLabel(order[i]));
            ListView_InsertItem(list,&item);ListView_SetCheckState(list,i,enabled[i]);}
        ListView_SetItemState(list,selected,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    }
    void Move(int step) {
        int selected=ListView_GetNextItem(list,-1,LVNI_SELECTED),target=selected+step;
        if(selected<0||target<0||target>=5)return;
        std::array<bool,5> enabled{};for(int i=0;i<5;++i)enabled[i]=ListView_GetCheckState(list,i);
        std::swap(order[selected],order[target]);std::swap(enabled[selected],enabled[target]);Populate(enabled,target);SetFocus(list);
    }
    bool Init() {
        dpi=GetDpiForWindow(window);
        font=CreateFontW(-Px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        Child(L"STATIC",L"勾选要显示的卡片，上下移动调整滚轮顺序。",0,0,18,16,362,25);
        list=Child(WC_LISTVIEWW,L"",WS_TABSTOP|WS_BORDER|LVS_REPORT|LVS_NOCOLUMNHEADER|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
            ListId,18,50,272,158);
        if(!list)return false;
        ListView_SetExtendedListViewStyle(list,LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        LVCOLUMNW column{};column.mask=LVCF_WIDTH;column.cx=Px(254);ListView_InsertColumn(list,0,&column);
        PageOrder config;Page selection=Page::Clock;ReadPreferences(root,config,selection);
        int i=0;for(auto page:config.pages)order[i++]=page;
        for(auto page:DefaultOrder)if(!config.Has(page))order[i++]=page;
        std::array<bool,5> enabled{};for(i=0;i<5;++i)enabled[i]=config.Has(order[i]);Populate(enabled,0);
        Child(L"BUTTON",L"上移",WS_TABSTOP,UpId,304,76,74,30);
        Child(L"BUTTON",L"下移",WS_TABSTOP,DownId,304,120,74,30);
        Child(L"STATIC",L"始终只显示一张，记住上次选择。\n在卡片上按下鼠标滚轮，可再次打开这里。",0,0,18,224,360,44);
        Child(L"BUTTON",L"保存",WS_TABSTOP|BS_DEFPUSHBUTTON,SaveId,210,286,80,32);
        Child(L"BUTTON",L"取消",WS_TABSTOP,CancelId,300,286,80,32);
        SetFocus(list);return true;
    }
    void Save() {
        PageOrder config;config.pages.clear();for(int i=0;i<5;++i)if(ListView_GetCheckState(list,i))config.pages.push_back(order[i]);
        if(config.pages.empty()){MessageBoxW(window,L"请至少保留一张卡片。",L"卡片设置",MB_OK|MB_ICONINFORMATION);return;}
        if(!SaveOrder(root,config)){MessageBoxW(window,L"未能保存设置，原设置已保留。",L"卡片设置",MB_OK|MB_ICONERROR);return;}
        NotifyPreferences();DestroyWindow(window);
    }
};
LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    auto state=reinterpret_cast<Settings*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    try{
        if(message==WM_NCCREATE){state=static_cast<Settings*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            state->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));}
        if(!state)return DefWindowProcW(window,message,wp,lp);
        switch(message){
        case WM_CREATE:return state->Init()?0:-1;
        case WM_COMMAND:switch(LOWORD(wp)){case UpId:state->Move(-1);return 0;case DownId:state->Move(1);return 0;
            case SaveId:case IDOK:state->Save();return 0;case CancelId:case IDCANCEL:DestroyWindow(window);return 0;}break;
        case WM_CLOSE:DestroyWindow(window);return 0;
        case WM_DESTROY:if(state->font)DeleteObject(state->font);state->font=nullptr;PostQuitMessage(0);return 0;
        }
    }catch(...){MessageBoxW(window,L"设置未完成，请关闭窗口后重试。",L"卡片设置",MB_OK|MB_ICONERROR);}
    return DefWindowProcW(window,message,wp,lp);
}
}

int LeanDockCardsSettings(HINSTANCE instance,LPCWSTR root) {
    // This is a short-lived GUI invocation of the same executable. It never
    // starts a second controller, requests elevation, or installs anything.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    HANDLE single=CreateMutexW(nullptr,FALSE,L"Local\\LeanDockCardsSettings");
    if(!single)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS){if(auto w=FindWindowW(WindowClass,nullptr)){ShowWindow(w,SW_RESTORE);SetForegroundWindow(w);}CloseHandle(single);return 0;}
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&common);
    WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=Procedure;cls.hInstance=instance;cls.lpszClassName=WindowClass;
    cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassExW(&cls)){CloseHandle(single);return 2;}
    Settings state;state.root=root;state.dpi=GetDpiForSystem();
    RECT size{0,0,state.Px(400),state.Px(336)};const DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    AdjustWindowRectEx(&size,style,FALSE,WS_EX_CONTROLPARENT);
    POINT mouse{};GetCursorPos(&mouse);MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromPoint(mouse,MONITOR_DEFAULTTOPRIMARY),&monitor);
    const int width=size.right-size.left,height=size.bottom-size.top;
    auto window=CreateWindowExW(WS_EX_CONTROLPARENT,WindowClass,L"LeanDock · 卡片设置",style,
        monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2,
        monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-height)/2,width,height,nullptr,nullptr,instance,&state);
    if(!window){if(state.font)DeleteObject(state.font);CloseHandle(single);return 3;}
    ShowWindow(window,SW_SHOWNORMAL);UpdateWindow(window);
    MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
    CloseHandle(single);return 0;
}
