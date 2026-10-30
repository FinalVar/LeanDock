#pragma once
// Hidden local fixture. Does not create Shell_TrayWnd, intercept notifications,
// send input or invoke a user's application.
namespace Capsule {
struct CallbackRecord {WPARAM w;LPARAM l;};
inline std::vector<CallbackRecord> protocolCallbacks;
inline LRESULT CALLBACK ProtocolOwner(HWND w,UINT m,WPARAM a,LPARAM b) {
    if(m==WM_APP+110){protocolCallbacks.push_back({a,b});return 0;}
    return DefWindowProcW(w,m,a,b);
}
inline int ProtocolTests() {
    int checks=0,failures=0;
    FILE* file=_wfopen((Directory()+L"\\protocol-tests-latest.txt").c_str(),L"w");
    auto check=[&](bool yes,const wchar_t* name){
        ++checks;if(!yes)++failures;
        if(file)fwprintf(file,L"%ls %ls\n",yes?L"PASS":L"FAIL",name);
    };
    WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=ProtocolOwner;
    wc.lpszClassName=L"Capsule.ProtocolFixture";RegisterClassW(&wc);
    HWND owner=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"",
        WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);
    check(owner!=nullptr,L"hidden owner created");
    Tray tray;TrayWire wire{};
    wire.magic=0x34753423;wire.message=NIM_ADD;auto& icon=wire.icon;
    icon.size=sizeof(icon);icon.hwnd=uint32_t(uintptr_t(owner));icon.id=41;
    icon.flags=NIF_MESSAGE|NIF_ICON|NIF_TIP;icon.callback=WM_APP+110;
    icon.icon=uint32_t(uintptr_t(LoadIconW(nullptr,IDI_APPLICATION)));
    wcscpy_s(icon.tip,L"图标测试");
    COPYDATASTRUCT data{1,sizeof(wire),&wire};
    check(tray.Receive(data)==1&&tray.icons.size()==1,L"add notification");
    check(tray.icons[0].tip==L"图标测试"&&bool(tray.icons[0].image),L"unicode tooltip and copied HICON");
    tray.Receive(data);check(tray.icons.size()==1,L"duplicate add updates one identity");
    POINT point{-5,80};
    protocolCallbacks.clear();tray.Click(0,WM_LBUTTONUP,point);
    check(protocolCallbacks.size()==1&&protocolCallbacks[0].w==41&&
        protocolCallbacks[0].l==WM_LBUTTONUP,L"version 0 callback packing");
    wire.message=NIM_SETVERSION;icon.version=3;tray.Receive(data);
    protocolCallbacks.clear();tray.Click(0,WM_LBUTTONUP,point);
    check(protocolCallbacks.size()==2&&protocolCallbacks[1].w==41&&
        protocolCallbacks[1].l==NIN_SELECT,L"version 3 select event");
    icon.version=4;tray.Receive(data);
    protocolCallbacks.clear();tray.Click(0,WM_RBUTTONUP,point);
    check(protocolCallbacks.size()==2&&LOWORD(protocolCallbacks[1].l)==WM_CONTEXTMENU&&
        HIWORD(protocolCallbacks[1].l)==41&&GET_X_LPARAM(protocolCallbacks[1].w)==-5&&
        GET_Y_LPARAM(protocolCallbacks[1].w)==80,L"version 4 context and signed coordinates");
    icon.version=9;check(tray.Receive(data)==0&&tray.icons[0].version==4,L"unsupported version rejected");
    protocolCallbacks.clear();tray.Click(0,WM_LBUTTONDBLCLK,point);
    check(protocolCallbacks.size()==1&&LOWORD(protocolCallbacks[0].l)==WM_LBUTTONDBLCLK,L"double click forwarding");
    wire.message=NIM_MODIFY;icon.flags=NIF_STATE;icon.stateMask=NIS_HIDDEN;icon.state=NIS_HIDDEN;
    tray.Receive(data);check(tray.icons[0].hidden,L"hide state");
    icon.stateMask=0;icon.state=0;tray.Receive(data);check(tray.icons[0].hidden,L"state mask respected");
    icon.stateMask=NIS_HIDDEN;tray.Receive(data);check(!tray.icons[0].hidden,L"unhide state");
    tray.icons[0].rect={100,200,22,22};
    TrayIdentifier id{};id.message=1;id.hwnd=icon.hwnd;id.id=41;
    COPYDATASTRUCT rectData{3,sizeof(id),&id};
    check(tray.Receive(rectData)==MAKELPARAM(100,200),L"native icon top left");
    id.message=2;check(tray.Receive(rectData)==MAKELPARAM(122,222),L"native icon bottom right");
    wire.message=NIM_ADD;icon.id=42;icon.flags=NIF_GUID|NIF_TIP;
    icon.guid={0x7820ae73,0x23e3,0x4229,{0x82,0xc1,0xe4,0x1c,0xb6,0x7d,0x5b,0x9c}};
    check(tray.Receive(data)==0&&tray.icons.size()==1,L"system volume excluded");
    icon.guid.Data1=0x7820ae74;check(tray.Receive(data)==0,L"system network excluded");
    icon.guid.Data1=0x7820ae75;check(tray.Receive(data)==0,L"system power excluded");
    icon.guid.Data1=0x12345678;tray.Receive(data);
    check(tray.icons.size()==2&&tray.icons[1].guidIdentity,L"application GUID accepted");
    icon.id=43;tray.Receive(data);check(tray.icons.size()==2,L"GUID identity survives numeric id change");
    COPYDATASTRUCT shortData{1,4,&wire};check(tray.Receive(shortData)==0,L"truncated input rejected");
    COPYDATASTRUCT nullData{1,sizeof(wire),nullptr};check(tray.Receive(nullData)==0,L"null input rejected");
    wire.message=NIM_DELETE;tray.Receive(data);check(tray.icons.size()==1,L"delete by GUID");
    icon.flags=0;icon.id=41;tray.Receive(data);check(tray.icons.empty(),L"delete legacy identity");
    wire.message=NIM_ADD;icon.id=44;tray.Receive(data);
    DestroyWindow(owner);tray.Prune();check(tray.icons.empty(),L"destroyed owner pruned");
    check(!tray.Available(),L"test never starts notification interception");
    tray.Stop();
    if(file){fwprintf(file,L"checks=%d failures=%d\n",checks,failures);fclose(file);}
    return failures?1:0;
}
inline int NativeTrayProbe() {
    auto before=ExplorerBars();if(before.empty())return 2;
    // In a subsequent capsule update the original receiver is the installed
    // capsule, not necessarily Explorer. Restore exactly that live receiver.
    const HWND beforeReceiver=FindWindowW(L"Shell_TrayWnd",nullptr);
    DWORD beforeReceiverPid{};GetWindowThreadProcessId(beforeReceiver,&beforeReceiverPid);
    if(!beforeReceiver||!beforeReceiverPid)return 2;
    HWND foreground=GetForegroundWindow();
    WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=ProtocolOwner;
    wc.lpszClassName=L"Capsule.NativeTrayFixture";RegisterClassW(&wc);
    HWND owner=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"",
        WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);
    if(!owner)return 3;
    Tray tray;
    if(!tray.Start(owner)){DestroyWindow(owner);return 4;}
    NOTIFYICONDATAW native{sizeof(native)};
    native.hWnd=owner;native.uID=791;
    native.uFlags=NIF_ICON|NIF_TIP|NIF_MESSAGE;native.uCallbackMessage=WM_APP+110;
    native.hIcon=LoadIconW(nullptr,IDI_APPLICATION);wcscpy_s(native.szTip,L"LeanDock isolated tray fixture");
    bool added=Shell_NotifyIconW(NIM_ADD,&native);
    native.uVersion=NOTIFYICON_VERSION_4;bool version=Shell_NotifyIconW(NIM_SETVERSION,&native);
    auto pump=[&](unsigned ms){
        const auto until=GetTickCount64()+ms;
        while(GetTickCount64()<until){
            MsgWaitForMultipleObjectsEx(0,nullptr,20,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
            tray.Ensure();
        }
    };
    pump(2500);
    const auto count=tray.icons.size();
    auto fixture=std::find_if(tray.icons.begin(),tray.icons.end(),[&](auto const& i){return i.owner==owner&&i.id==791;});
    bool received=fixture!=tray.icons.end()&&fixture->version==4;
    tray.Ensure();
    bool removed=Shell_NotifyIconW(NIM_DELETE,&native);
    pump(100);
    bool absent=std::none_of(tray.icons.begin(),tray.icons.end(),[&](auto const& i){return i.owner==owner&&i.id==791;});
    tray.Stop();pump(400);
    auto top=FindWindowW(L"Shell_TrayWnd",nullptr);
    DWORD afterReceiverPid{};GetWindowThreadProcessId(top,&afterReceiverPid);
    bool restored=top==beforeReceiver&&afterReceiverPid==beforeReceiverPid;
    bool focusPreserved=GetForegroundWindow()==foreground;
    DestroyWindow(owner);
    FILE* f=_wfopen((Directory()+L"\\native-tray-probe-latest.json").c_str(),L"w");
    if(f){fprintf(f,"{\"add\":%s,\"version\":%s,\"receivedFixture\":%s,\"capturedIcons\":%zu,"
        "\"removed\":%s,\"fixtureAbsent\":%s,\"originalReceiverRestored\":%s,\"foregroundPreserved\":%s,"
        "\"originalReceiverPid\":%lu,\"restoredReceiverPid\":%lu,"
        "\"appMouseCallbacksExercised\":false}\n",added?"true":"false",version?"true":"false",
        received?"true":"false",count,removed?"true":"false",absent?"true":"false",
        restored?"true":"false",focusPreserved?"true":"false",beforeReceiverPid,afterReceiverPid);fclose(f);}
    return added&&version&&received&&removed&&absent&&restored&&focusPreserved?0:1;
}
}
