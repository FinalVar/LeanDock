#pragma once
// Captures only our own test surface, backed by our own checkerboard window.
// No desktop input, no foreground changes, no taskbar/tray interception.
namespace Capsule {
inline bool CanInspectOwnVisibleSurface() {
    DWORD session=0,bytes=0;LPWSTR buffer=nullptr;
    if(!ProcessIdToSessionId(GetCurrentProcessId(),&session)||
       !WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,session,WTSSessionInfoEx,&buffer,&bytes))return false;
    const auto info=reinterpret_cast<WTSINFOEXW*>(buffer);
    const bool unlocked=bytes>=sizeof(WTSINFOEXW)&&info->Level==1&&
        info->Data.WTSInfoExLevel1.SessionState==WTSActive&&
        info->Data.WTSInfoExLevel1.SessionFlags==WTS_SESSIONSTATE_UNLOCK;
    WTSFreeMemory(buffer);
    if(!unlocked)return false;
    auto input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
    if(!input)return false;
    wchar_t active[256]{},ours[256]{};DWORD needed=0;
    bool ok=GetUserObjectInformationW(input,UOI_NAME,active,sizeof(active),&needed)&&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,ours,sizeof(ours),&needed);
    CloseDesktop(input);
    return ok&&!_wcsicmp(active,ours);
}
inline LRESULT CALLBACK CheckerProc(HWND w,UINT m,WPARAM a,LPARAM b) {
    if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
    if(m==WM_PAINT){
        PAINTSTRUCT p{};HDC dc=BeginPaint(w,&p);RECT r{};GetClientRect(w,&r);
        bool warm=GetPropW(w,L"Capsule.FixtureWarm")!=nullptr;
        HBRUSH light=CreateSolidBrush(warm?RGB(250,200,140):RGB(250,245,225));
        HBRUSH dark=CreateSolidBrush(warm?RGB(165,30,15):RGB(25,100,160));
        for(int y=0;y<r.bottom;y+=12)for(int x=0;x<r.right;x+=12){
            RECT tile{x,y,x+12,y+12};FillRect(dc,&tile,((x/12+y/12)%2)?light:dark);
        }
        DeleteObject(light);DeleteObject(dark);EndPaint(w,&p);return 0;
    }return DefWindowProcW(w,m,a,b);
}
inline HWND Checker(RECT r) {
    WNDCLASSW c{};c.hInstance=GetModuleHandleW(nullptr);c.lpfnWndProc=CheckerProc;c.lpszClassName=L"Capsule.OwnVisualFixture";
    RegisterClassW(&c);
    HWND w=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,c.lpszClassName,L"",
        WS_POPUP,r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,c.hInstance,nullptr);
    if(w){ShowWithoutActivation(w);UpdateWindow(w);}return w;
}
inline void SaveOwnFrame(Drawing& drawing,Rect r,std::wstring path) {
    if(!CanInspectOwnVisibleSurface())winrt::throw_hresult(E_ACCESSDENIED);
    FILE* trace=_wfopen((Directory()+L"\\visual-state-latest.txt").c_str(),L"a");
    if(trace){
        wchar_t desktop[256]{};DWORD required=0;
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktop,sizeof(desktop),&required);
        fwprintf(trace,L"capture x=%.0f y=%.0f w=%.0f h=%.0f desktop=%ls\n",r.x,r.y,r.w,r.h,desktop);
        for(auto cls:{L"Capsule.OwnVisualFixture",WindowClass,L"NativeTaskbarDock.Capsule.Popup.v1"}){
            HWND w=FindWindowW(cls,nullptr);RECT wr{};DWORD cloaked=0,pid=0;
            GetWindowRect(w,&wr);GetWindowThreadProcessId(w,&pid);
            DwmGetWindowAttribute(w,DWMWA_CLOAKED,&cloaked,sizeof(cloaked));
            fwprintf(trace,L"%ls hwnd=%p pid=%lu visible=%d cloaked=%lu rect=%ld,%ld,%ld,%ld\n",
                cls,w,pid,IsWindowVisible(w),cloaked,wr.left,wr.top,wr.right,wr.bottom);
        }
        fclose(trace);
    }
    int width=int(r.w),height=int(r.h);
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,width,height);
    auto old=SelectObject(dc,bitmap);
    BOOL copied=BitBlt(dc,0,0,width,height,screen,int(r.x),int(r.y),SRCCOPY|CAPTUREBLT);
    SelectObject(dc,old);DeleteDC(dc);ReleaseDC(nullptr,screen);
    if(!copied){DeleteObject(bitmap);winrt::throw_last_error();}
    auto image=drawing.Bitmap(bitmap);DeleteObject(bitmap);
    if(!image)winrt::throw_hresult(E_FAIL);
    winrt::com_ptr<IWICStream> stream;
    winrt::check_hresult(drawing.wic->CreateStream(stream.put()));
    winrt::check_hresult(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    winrt::com_ptr<IWICBitmapEncoder> encoder;
    winrt::check_hresult(drawing.wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.put()));
    winrt::check_hresult(encoder->Initialize(stream.get(),WICBitmapEncoderNoCache));
    winrt::com_ptr<IWICBitmapFrameEncode> frame;
    winrt::check_hresult(encoder->CreateNewFrame(frame.put(),nullptr));
    winrt::check_hresult(frame->Initialize(nullptr));
    winrt::check_hresult(frame->WriteSource(image.get(),nullptr));
    winrt::check_hresult(frame->Commit());winrt::check_hresult(encoder->Commit());
}
}
