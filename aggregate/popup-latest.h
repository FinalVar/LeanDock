#pragma once
#include "platform-latest.h"
#include "drawing-latest.h"
#include <functional>

namespace Capsule {
inline bool RoundedHit(Rect r,double x,double y,double radius) {
    if(!r.Contains(x,y))return false;
    double dx=x-std::clamp(x,r.x+radius,r.x+r.w-radius);
    double dy=y-std::clamp(y,r.y+radius,r.y+r.h-radius);
    return dx*dx+dy*dy<=radius*radius;
}
struct MenuItem {
    std::wstring text;
    int action=0;
    bool separator=false,disabled=false,danger=false,submenu=false;
};
class Popup {
    static inline const wchar_t* Class=L"LeanDock.Aggregate.Popup.v1";
    static LRESULT CALLBACK Proc(HWND w,UINT m,WPARAM a,LPARAM b) {
        auto self=reinterpret_cast<Popup*>(GetWindowLongPtrW(w,GWLP_USERDATA));
        if(m==WM_NCCREATE){self=static_cast<Popup*>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams);
            SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->window=w;}
        if(!self)return DefWindowProcW(w,m,a,b);
        try{
            switch(m){
            case WM_CLOSE:if(self->cancel)self->cancel();else self->Hide();return 0;
            case WM_MOUSEACTIVATE:return self->menu?MA_ACTIVATE:MA_NOACTIVATE;
            case WM_NCHITTEST:return self->Hit(GET_X_LPARAM(b),GET_Y_LPARAM(b))?HTCLIENT:HTTRANSPARENT;
            case WM_ERASEBKGND:return 1;
            case WM_PAINT:{PAINTSTRUCT p;BeginPaint(w,&p);EndPaint(w,&p);return 0;}
            case WM_MOUSEMOVE:{
                POINT p{GET_X_LPARAM(b),GET_Y_LPARAM(b)};ClientToScreen(w,&p);self->Hover(p);
                if(!self->menu&&self->click)self->click(m,float(GET_X_LPARAM(b))/self->dpi-32,float(GET_Y_LPARAM(b))/self->dpi-32);
                return 0;
            }
            case WM_CAPTURECHANGED:case WM_CANCELMODE:
                if(!self->menu&&self->pointerCancel)self->pointerCancel();
                if(m==WM_CANCELMODE&&GetCapture()==w)ReleaseCapture();return 0;
            case WM_LBUTTONUP:case WM_RBUTTONUP:case WM_LBUTTONDBLCLK:{
                float x=float(GET_X_LPARAM(b))/self->dpi-32,y=float(GET_Y_LPARAM(b))/self->dpi-32;
                if(self->menu){
                    auto i=self->Index(y);
                    if(i>=0&&!self->items[i].disabled&&!self->items[i].separator&&self->select)
                        self->select(self->items[i].action);
                }else if(self->click)self->click(m,x,y);
                return 0;
            }
            case WM_LBUTTONDOWN:case WM_RBUTTONDOWN:
                if(!self->menu&&self->click)self->click(m,float(GET_X_LPARAM(b))/self->dpi-32,float(GET_Y_LPARAM(b))/self->dpi-32);
                return 0;
            case WM_KEYDOWN:
                if(a==VK_ESCAPE){if(self->cancel)self->cancel();else self->Hide();return 0;}
                if(a==VK_UP||a==VK_DOWN){
                    for(size_t n=0;n<self->items.size();n++){
                        self->hover=(self->hover+(a==VK_UP?-1:1)+int(self->items.size()))%int(self->items.size());
                        auto& i=self->items[self->hover];if(!i.disabled&&!i.separator)break;
                    }self->PaintMenu();return 0;
                }
                if(a==VK_RETURN&&self->hover>=0&&self->hover<int(self->items.size())&&self->select&&
                   !self->items[self->hover].disabled&&!self->items[self->hover].separator){
                    self->select(self->items[self->hover].action);return 0;}
                if(a==VK_LEFT&&self->hover>=0&&self->items[self->hover].submenu&&self->select){
                    self->select(self->items[self->hover].action);return 0;
                }break;
            }
        }catch(...){Log(L"popup-error");self->Hide();}
        return DefWindowProcW(w,m,a,b);
    }
public:
    HWND window{};
    Scene scene;
    Drawing* draw{};
    float dpi=1,width=300,height=340,radius=14;
    Rect rect{};
    bool visible=false,menu=false,transparent=true;
    int hover=-1;
    std::vector<MenuItem> items;
    std::function<void(int)> select;
    std::function<void()> cancel;
    std::function<void(UINT,float,float)> click;
    std::function<void()> pointerCancel;
    std::function<void(int)> hovering;
    void Init(Drawing& renderer,float scale,float w,float h,float round,bool isMenu) {
        draw=&renderer;dpi=scale;width=w;height=h;radius=round;menu=isMenu;
        WNDCLASSW c{};c.hInstance=GetModuleHandleW(nullptr);c.lpfnWndProc=Proc;c.lpszClassName=Class;
        c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.style=CS_DBLCLKS;RegisterClassW(&c);
        DWORD ex=WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOREDIRECTIONBITMAP|WS_EX_LAYERED|WS_EX_TRANSPARENT;
        if(!menu)ex|=WS_EX_NOACTIVATE;
        window=CreateWindowExW(ex,Class,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,c.hInstance,this);
        if(!window)winrt::throw_last_error();
        SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);
        scene.full=scene.base=width;scene.height=height;scene.radius=radius;scene.floating=true;
        scene.Init(window,dpi);renderer.Init(scene.compositor);
    }
    void Position(double left,double top) {
        rect={left,top,width*dpi,height*dpi};
        SetWindowPos(window,HWND_TOPMOST,int(std::lround(left-32*dpi)),int(std::lround(top-32*dpi)),
            int(std::ceil((width+64)*dpi)),int(std::ceil((height+64)*dpi)),SWP_NOACTIVATE);
    }
    void Paint(std::function<void(ID2D1DeviceContext*)> painter) {
        auto surface=draw->Surface(width,height,dpi,painter);
        scene.content.Children().RemoveAll();
        scene.content.Children().InsertAtTop(draw->Visual(scene.compositor,surface,width,height));
    }
    bool Hit(double x,double y)const {return visible&&RoundedHit(rect,x,y,radius*dpi);}
    int Index(float y)const{
        float at=5;for(int i=0;i<int(items.size());i++){
            float h=items[i].separator?9:28;
            if(y>=at&&y<at+h)return i;at+=h;
        }return -1;
    }
    float ItemY(int i)const {float y=5;for(int n=0;n<i&&n<int(items.size());n++)y+=items[n].separator?9:28;return y;}
    void Hover(POINT p) {
        if(menu){
            int next=Hit(p.x,p.y)?Index(float(p.y-rect.y)/dpi):-1;
            if(next!=hover){hover=next;PaintMenu();if(hovering)hovering(hover);}
        }
        bool pass=GetCapture()!=window&&!Hit(p.x,p.y);
        if(pass!=transparent){
            transparent=pass;auto ex=GetWindowLongPtrW(window,GWL_EXSTYLE);
            SetWindowLongPtrW(window,GWL_EXSTYLE,pass?ex|WS_EX_TRANSPARENT:ex&~WS_EX_TRANSPARENT);
        }
    }
    void Show(bool detail=false) {
        visible=true;scene.FloatShow(Animations(),detail);
        POINT p{};GetCursorPos(&p);Hover(p);
        ShowWithoutActivation(window);
        if(menu)SetForegroundWindow(window);
    }
    void Hide() {visible=false;hover=-1;
        if(pointerCancel)pointerCancel();if(window&&GetCapture()==window)ReleaseCapture();
        if(window)ShowWindow(window,SW_HIDE);}
    void PaintMenu() {
        Paint([&](ID2D1DeviceContext* dc){
            auto& c=draw->colors;float y=5;
            for(int i=0;i<int(items.size());i++){
                auto& item=items[i];
                if(item.separator){draw->Line(dc,11,y+4,width-11,y+4,c.hair);y+=9;continue;}
                if(i==hover&&!item.disabled)draw->Round(dc,{5,y,width-10,28},6,c.hover);
                draw->Text(dc,item.text,{15,y,width-30-(item.submenu?15:0),28},13,
                    item.disabled?c.secondary:item.danger?c.danger:c.text);
                if(item.submenu)draw->Text(dc,L"›",{width-28,y,13,28},13,c.secondary,false,true);
                y+=28;
            }
        });
    }
    void Destroy() {Hide();scene.Close();if(window){DestroyWindow(window);window=nullptr;}}
    ~Popup(){Destroy();}
};
inline float MenuHeight(std::vector<MenuItem> const& items){
    float h=10;for(auto const& item:items)h+=item.separator?9:28;return h;
}
}
