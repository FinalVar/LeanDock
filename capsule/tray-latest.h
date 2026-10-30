#pragma once
#include "platform-latest.h"
#include <memory>
#include <functional>

// Protocol/lifecycle adaptation of ManagedShell WindowsTray TrayService and
// NotifyIcon, commit 855222035e927910ca53fd3215caef056337a048, Apache-2.0.
// Kept wire handles at 32 bits (including x64), versioned mouse callbacks,
// Explorer forwarding, copied HICON ownership, TaskbarCreated and disposal.
namespace Capsule {
#pragma pack(push,4)
struct TrayWireIcon {
    uint32_t size,hwnd,id,flags,callback,icon;
    wchar_t tip[128];
    uint32_t state,stateMask;
    wchar_t info[256];
    uint32_t version;
    wchar_t infoTitle[64];
    uint32_t infoFlags;
    GUID guid;
    uint32_t balloon;
};
struct TrayWire {uint32_t magic,message;TrayWireIcon icon;};
struct TrayIdentifier {uint32_t magic,message,size,padding,hwnd,id;GUID guid;};
#pragma pack(pop)
struct TrayIcon {
    HWND owner{};UINT id=0,callback=0,version=0;
    GUID guid{};bool guidIdentity=false,hidden=false;
    std::shared_ptr<std::remove_pointer_t<HICON>> image;
    std::wstring tip;
    uint64_t serial=0;
    Rect rect{};
};
class Tray {
    HWND window{},notify{},ui{};
    uint64_t serial=0;
    bool active=false,broadcasting=false;
    static LRESULT CALLBACK Proc(HWND w,UINT m,WPARAM a,LPARAM b){
        auto self=reinterpret_cast<Tray*>(GetWindowLongPtrW(w,GWLP_USERDATA));
        if(m==WM_NCCREATE){self=static_cast<Tray*>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams);
            SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(!self)return DefWindowProcW(w,m,a,b);
        if(m==WM_COPYDATA&&b){
            auto cd=reinterpret_cast<COPYDATASTRUCT*>(b);
            LRESULT own=self->Receive(*cd);
            if(cd->dwData==3&&own)return own;
            // Relay original buffer while its lifetime is valid. Bounded to
            // avoid a blocked Explorer freezing application notifications.
            LRESULT forwarded=self->Forward(m,a,b);
            return own?own:forwarded;
        }
        if(m==WM_WINDOWPOSCHANGED){
            auto pos=reinterpret_cast<WINDOWPOS*>(b);
            if(pos&&(pos->flags&SWP_SHOWWINDOW))
                SetWindowLongPtrW(w,GWL_STYLE,GetWindowLongPtrW(w,GWL_STYLE)&~WS_VISIBLE);
        }
        if(m==WM_ACTIVATEAPP||m==WM_COMMAND||(m>=WM_USER&&m<0xC000))return self->Forward(m,a,b);
        return DefWindowProcW(w,m,a,b);
    }
    static bool Excluded(GUID const& g){
        // Windows volume, network and power icons. Do not exclude other
        // Explorer-owned icons such as safely remove hardware.
        static const GUID volume{0x7820ae73,0x23e3,0x4229,{0x82,0xc1,0xe4,0x1c,0xb6,0x7d,0x5b,0x9c}};
        static const GUID network{0x7820ae74,0x23e3,0x4229,{0x82,0xc1,0xe4,0x1c,0xb6,0x7d,0x5b,0x9c}};
        static const GUID power{0x7820ae75,0x23e3,0x4229,{0x82,0xc1,0xe4,0x1c,0xb6,0x7d,0x5b,0x9c}};
        return g==volume||g==network||g==power;
    }
    LRESULT Forward(UINT m,WPARAM a,LPARAM b) {
        auto bars=ExplorerBars();if(bars.empty())return 0;
        if(m==WM_USER+372){PostMessageW(bars.front(),m,a,b);return 0;}
        DWORD_PTR result=0;SendMessageTimeoutW(bars.front(),m,a,b,SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&result);
        return LRESULT(result);
    }
    void Changed(){if(ui)PostMessageW(ui,WM_APP+29,0,0);}
public:
    std::vector<TrayIcon> icons;
    DWORD callbackPid=0,callbackThread=0;
    uint64_t callbackAt=0;
    bool Available()const{return active;}
    LRESULT Receive(COPYDATASTRUCT const& cd) {
        if(!cd.lpData)return 0;
        if(cd.dwData==3&&cd.cbData>=sizeof(TrayIdentifier)){
            TrayIdentifier n{};memcpy(&n,cd.lpData,sizeof(n));
            for(auto const& icon:icons)
                if((n.guid!=GUID{}&&icon.guid==n.guid)||(icon.owner==reinterpret_cast<HWND>(uintptr_t(n.hwnd))&&icon.id==n.id)){
                    auto r=icon.rect;
                    return n.message==1?MAKELPARAM(int(r.x),int(r.y)):
                        n.message==2?MAKELPARAM(int(r.x+r.w),int(r.y+r.h)):0;
                }
        }
        if(cd.dwData!=1||cd.cbData<8+24||cd.cbData>65536)return 0;
        TrayWire n{};memcpy(&n,cd.lpData,std::min<size_t>(sizeof(n),cd.cbData));
        auto& d=n.icon;HWND owner=reinterpret_cast<HWND>(uintptr_t(d.hwnd));
        bool hasGuid=(d.flags&NIF_GUID)&&cd.cbData>=offsetof(TrayWire,icon)+offsetof(TrayWireIcon,balloon)&&d.guid!=GUID{};
        auto found=std::find_if(icons.begin(),icons.end(),[&](auto const& i){
            return hasGuid?i.guidIdentity&&i.guid==d.guid:i.owner==owner&&i.id==d.id;
        });
        if(n.message==NIM_DELETE){
            if(found!=icons.end()){icons.erase(found);Changed();}return 1;
        }
        if(n.message==NIM_SETVERSION){
            if(found==icons.end())return 0;
            if(d.version>NOTIFYICON_VERSION_4)return 0;
            found->version=d.version;return 1;
        }
        if(n.message==NIM_SETFOCUS)return 1;
        if(n.message!=NIM_ADD&&n.message!=NIM_MODIFY)return 0;
        if(!IsWindow(owner)||(hasGuid&&Excluded(d.guid)))return 0;
        if(found==icons.end()){
            if(icons.size()>=128)return 0;
            TrayIcon item;item.owner=owner;item.id=d.id;item.guid=d.guid;item.guidIdentity=hasGuid;
            item.serial=++serial;icons.push_back(std::move(item));found=icons.end()-1;
        }
        found->owner=owner;
        if(d.flags&NIF_MESSAGE)found->callback=d.callback;
        if(d.flags&NIF_TIP)found->tip.assign(d.tip,wcsnlen(d.tip,128));
        if(d.flags&NIF_STATE)found->hidden=(d.stateMask&NIS_HIDDEN)?(d.state&NIS_HIDDEN)!=0:found->hidden;
        if(d.flags&NIF_ICON){
            auto image=CopyIcon(reinterpret_cast<HICON>(uintptr_t(d.icon)));
            if(image)found->image.reset(image,[](HICON p){DestroyIcon(p);});
        }
        Changed();return 1;
    }
    bool Start(HWND owner) {
        ui=owner;
        WNDCLASSW c{};c.hInstance=GetModuleHandleW(nullptr);c.lpfnWndProc=Proc;c.style=CS_DBLCLKS;
        c.lpszClassName=L"Shell_TrayWnd";RegisterClassW(&c);
        window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE,c.lpszClassName,L"",
            WS_POPUP,0,0,0,0,nullptr,nullptr,c.hInstance,this);
        if(!window)return false;
        c.lpszClassName=L"TrayNotifyWnd";RegisterClassW(&c);
        notify=CreateWindowExW(0,c.lpszClassName,L"",WS_CHILD,0,0,0,0,window,nullptr,c.hInstance,this);
        active=notify!=nullptr;
        if(!active){Stop();return false;}
        Ensure();Broadcast();return true;
    }
    void Ensure() {
        if(!active)return;
        if(FindWindowW(L"Shell_TrayWnd",nullptr)!=window)
            SetWindowPos(window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    }
    void Broadcast() {
        if(broadcasting)return;broadcasting=true;
        SendNotifyMessageW(HWND_BROADCAST,RegisterWindowMessageW(L"TaskbarCreated"),0,0);
        broadcasting=false;
    }
    void Recreated(){if(!active)return;Prune();Ensure();Changed();}
    void Prune() {
        auto n=icons.size();std::erase_if(icons,[](auto const& i){return !IsWindow(i.owner);});
        if(n!=icons.size())Changed();
    }
    void Send(TrayIcon const& i,UINT message,POINT p) {
        if(!IsWindow(i.owner)||!i.callback)return;
        callbackThread=GetWindowThreadProcessId(i.owner,&callbackPid);callbackAt=GetTickCount64();
        AllowSetForegroundWindow(callbackPid);
        WPARAM a=i.version>=4?WPARAM(MAKELONG(p.x,p.y)):i.id;
        LPARAM b=i.version>=4?LPARAM(MAKELONG(message,i.id)):message;
        SendNotifyMessageW(i.owner,i.callback,a,b);
    }
    void Click(size_t index,UINT message,POINT p) {
        if(index>=icons.size())return;
        auto i=icons[index];Send(i,message,p);
        if(i.version>=3&&message==WM_LBUTTONUP)Send(i,NIN_SELECT,p);
        if(i.version>=3&&message==WM_RBUTTONUP)Send(i,WM_CONTEXTMENU,p);
    }
    bool Holding() {
        if(!callbackPid)return false;
        if(GetTickCount64()-callbackAt<600)return true;
        GUITHREADINFO g{sizeof(g)};
        if(callbackThread&&GetGUIThreadInfo(callbackThread,&g)&&(g.flags&(GUI_INMENUMODE|GUI_POPUPMENUMODE)))return true;
        auto foregroundWindow=GetForegroundWindow();
        DWORD foreground=0;GetWindowThreadProcessId(foregroundWindow,&foreground);
        // Activating the owner's main window is not an open tray menu. Holding
        // on the process alone leaves the Dock permanently visible afterwards.
        if(foreground==callbackPid&&GetWindow(foregroundWindow,GW_OWNER)&&
           (GetWindowLongPtrW(foregroundWindow,GWL_STYLE)&WS_POPUP))return true;
        callbackPid=callbackThread=0;return false;
    }
    void Stop(){
        bool announce=active;active=false;ui=nullptr;
        if(notify){DestroyWindow(notify);notify=nullptr;}
        if(window){DestroyWindow(window);window=nullptr;}
        icons.clear();if(announce)Broadcast();
    }
    ~Tray(){Stop();}
};
static_assert(sizeof(TrayWireIcon)==956);
}
