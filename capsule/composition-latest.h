#pragma once
#include "effects-latest.h"
#include "policy-latest.h"
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <dwmapi.h>

// Public Windows SDK ABI (windows.ui.composition.interop.h); not an internal
// compositor hook. MinGW's header does not yet declare the desktop interface.
struct CapsuleDesktopInterop : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateDesktopWindowTarget(HWND,BOOL,void**)=0;
    virtual HRESULT STDMETHODCALLTYPE EnsureOnThread(DWORD)=0;
};
template<> inline constexpr winrt::guid winrt::impl::guid_v<CapsuleDesktopInterop>{
    0x29E691FA,0x4567,0x4DCA,{0xB3,0x19,0xD0,0xF2,0x07,0xEB,0x68,0x07}};

namespace Capsule {
namespace comp=winrt::Windows::UI::Composition;
namespace num=winrt::Windows::Foundation::Numerics;
using winrt::Windows::UI::Color;
struct Scene {
    winrt::Windows::System::DispatcherQueueController dispatcher{nullptr};
    comp::Compositor compositor{nullptr};
    comp::Desktop::DesktopWindowTarget target{nullptr};
    comp::ContainerVisual root{nullptr},motion{nullptr},body{nullptr},content{nullptr};
    comp::CompositionPropertySet props{nullptr};
    comp::CompositionColorBrush tint{nullptr},stroke{nullptr};
    comp::DropShadow soft{nullptr},tight{nullptr};
    float full=400,base=120,scale=1,height=Height,radius=Radius;
    bool closed=false,floating=false,ownsCompositor=true;
    void Expression(comp::CompositionObject const& object,wchar_t const* property,
                    wchar_t const* expression) {
        auto a=compositor.CreateExpressionAnimation(expression);
        a.SetReferenceParameter(L"p",props);object.StartAnimation(property,a);
    }
    void SizeExpression(comp::CompositionObject const& object,wchar_t const* property=L"Size") {
        Expression(object,property,L"Vector2(p.Width, p.Height)");
    }
    void Init(HWND hwnd,float dpiScale,Scene* shared=nullptr) {
        ownsCompositor=!shared;
        if(shared)compositor=shared->compositor;
        else {
        struct Options{DWORD size;int thread,apartment;};
        using CreateQueue=HRESULT(WINAPI*)(Options,void**);
        static HMODULE module=LoadLibraryW(L"CoreMessaging.dll");
        if(!module)winrt::throw_last_error();
        const auto create=reinterpret_cast<CreateQueue>(GetProcAddress(module,"CreateDispatcherQueueController"));
        if(!create)winrt::throw_last_error();
        Options options{sizeof(Options),2,2};
        winrt::check_hresult(create(options,winrt::put_abi(dispatcher)));
        compositor=comp::Compositor();
        }
        BOOL yes=TRUE;
        winrt::check_hresult(DwmSetWindowAttribute(hwnd,17,&yes,sizeof(yes)));
        winrt::check_hresult(compositor.as<CapsuleDesktopInterop>()->CreateDesktopWindowTarget(
            hwnd,TRUE,winrt::put_abi(target)));
        if(!target)winrt::throw_hresult(E_FAIL);
        root=compositor.CreateContainerVisual();target.Root(root);
        scale=dpiScale;root.Scale({scale,scale,1});
        motion=compositor.CreateContainerVisual();root.Children().InsertAtTop(motion);
        props=compositor.CreatePropertySet();
        props.InsertScalar(L"Full",full);props.InsertScalar(L"Height",height);
        props.InsertScalar(L"X",32);
        props.InsertScalar(L"Width",base);props.InsertScalar(L"Y",HiddenOffset);
        props.InsertScalar(L"Zoom",.96f);props.InsertScalar(L"Alpha",0);
        Expression(motion,L"Offset",L"Vector3(p.X,32+p.Y,0)");
        Expression(motion,L"CenterPoint",L"Vector3(p.Full,p.Height/2,0)");
        Expression(motion,L"Scale",L"Vector3(p.Zoom,p.Zoom,1)");
        Expression(motion,L"Opacity",L"p.Alpha");
        body=compositor.CreateContainerVisual();motion.Children().InsertAtTop(body);
        Expression(body,L"Offset",L"Vector3(p.Full-p.Width,0,0)");

        // Separate vector mask, sampled at the monitor's actual resolution.
        // No region clipping of the desktop window, so the external shadows
        // remain smooth. Hit testing uses the same animated rounded geometry.
        auto maskShape=compositor.CreateShapeVisual();SizeExpression(maskShape);
        auto round=compositor.CreateRoundedRectangleGeometry();
        round.CornerRadius({radius,radius});SizeExpression(round);
        auto maskFill=compositor.CreateSpriteShape(round);
        maskFill.FillBrush(compositor.CreateColorBrush(Color{255,255,255,255}));
        maskShape.Shapes().Append(maskFill);
        auto surface=compositor.CreateVisualSurface();surface.SourceVisual(maskShape);
        SizeExpression(surface,L"SourceSize");
        auto mask=compositor.CreateSurfaceBrush(surface);

        auto shadowVisual=[&](float blur,float offset,comp::DropShadow& result) {
            auto v=compositor.CreateSpriteVisual();SizeExpression(v);
            result=compositor.CreateDropShadow();result.BlurRadius(blur);
            result.Offset({0,offset,0});result.Mask(mask);
            v.Shadow(result);body.Children().InsertAtTop(v);
        };
        // CSS box-shadow blur radius maps to approximately half that sigma.
        shadowVisual(15,10,soft);shadowVisual(1.5f,1,tight);

        auto blur=winrt::make_self<GaussianBlurEffect>();
        blur->BlurAmount=floating?26:22;blur->BorderMode=D2D1_BORDER_MODE_HARD;
        blur->Source=comp::CompositionEffectSourceParameter(L"backdrop");
        auto saturate=winrt::make_self<ColorMatrixEffect>();
        saturate->Source=*blur;
        constexpr float s=1.6f,inv=1-s,r=.2126f,g=.7152f,b=.0722f;
        float matrix[20]={inv*r+s,inv*r,inv*r,0,
                          inv*g,inv*g+s,inv*g,0,
                          inv*b,inv*b,inv*b+s,0,
                          0,0,0,1,0,0,0,0};
        std::copy(std::begin(matrix),std::end(matrix),saturate->Matrix);
        auto factory=compositor.CreateEffectFactory(*saturate);
        auto effect=factory.CreateBrush();
        effect.SetSourceParameter(L"backdrop",compositor.CreateHostBackdropBrush());
        auto glassMask=compositor.CreateMaskBrush();glassMask.Source(effect);glassMask.Mask(mask);
        auto glass=compositor.CreateSpriteVisual();glass.Brush(glassMask);SizeExpression(glass);
        body.Children().InsertAtTop(glass);
        tint=compositor.CreateColorBrush();
        auto tintMask=compositor.CreateMaskBrush();tintMask.Source(tint);tintMask.Mask(mask);
        auto wash=compositor.CreateSpriteVisual();wash.Brush(tintMask);SizeExpression(wash);
        body.Children().InsertAtTop(wash);

        auto outline=compositor.CreateShapeVisual();SizeExpression(outline);
        auto outlineGeometry=compositor.CreateRoundedRectangleGeometry();
        outlineGeometry.Offset({.5f,.5f});outlineGeometry.CornerRadius({radius-.5f,radius-.5f});
        Expression(outlineGeometry,L"Size",L"Vector2(p.Width-1,p.Height-1)");
        auto line=compositor.CreateSpriteShape(outlineGeometry);
        stroke=compositor.CreateColorBrush();line.StrokeBrush(stroke);line.StrokeThickness(1);
        outline.Shapes().Append(line);body.Children().InsertAtTop(outline);
        content=compositor.CreateContainerVisual();body.Children().InsertAtTop(content);
        Expression(content,L"Offset",L"Vector3(p.Width-p.Full,0,0)");
        auto clip=compositor.CreateInsetClip();content.Clip(clip);
        Expression(clip,L"LeftInset",L"Max(0,p.Full-p.Width)");
        Expression(content,L"Size",L"Vector2(p.Full,p.Height)");
        Theme();
    }
    void Theme() {
        DWORD light=1,size=sizeof(light);
        RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"SystemUsesLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);
        tint.Color(floating?(light?Color{214,248,249,251}:Color{219,34,36,42}):
                            (light?Color{158,246,247,250}:Color{148,30,32,38}));
        stroke.Color(light?Color{153,255,255,255}:Color{23,255,255,255});
        soft.Color(light?Color{255,15,20,30}:Color{255,0,0,0});
        soft.Opacity(light?.20f:.45f);
        tight.Color(light?Color{255,15,20,30}:Color{255,0,0,0});
        tight.Opacity(light?.12f:.35f);
    }
    void Scalar(wchar_t const* name,float end,int ms,int delay,Curve curve,bool linear=false) {
        auto a=compositor.CreateScalarKeyFrameAnimation();
        a.InsertExpressionKeyFrame(0,L"this.StartingValue");
        comp::CompositionEasingFunction ease=linear?
            compositor.CreateLinearEasingFunction().as<comp::CompositionEasingFunction>():
            compositor.CreateCubicBezierEasingFunction(
                {float(curve.x1),float(curve.y1)},{float(curve.x2),float(curve.y2)}).as<comp::CompositionEasingFunction>();
        a.InsertKeyFrame(1,end,ease);a.Duration(std::chrono::milliseconds(ms));
        a.DelayTime(std::chrono::milliseconds(delay));
        a.DelayBehavior(comp::AnimationDelayBehavior::SetInitialValueBeforeDelay);
        props.StartAnimation(name,a);
    }
    void Animate(bool show,bool enabled,Motion::Frame const* frame=nullptr) {
        if(frame){
            for(auto name:{L"Y",L"Zoom",L"Alpha",L"Width"})props.StopAnimation(name);
            props.InsertScalar(L"Y",float(frame->y));props.InsertScalar(L"Zoom",float(frame->scale));
            props.InsertScalar(L"Alpha",float(frame->alpha));props.InsertScalar(L"Width",float(frame->width));
        }
        if(!enabled){
            props.InsertScalar(L"Width",show?full:base);props.InsertScalar(L"Y",show?0:HiddenOffset);
            props.InsertScalar(L"Zoom",show?1:.96f);props.InsertScalar(L"Alpha",show?1:0);
            return;
        }
        const auto c=show?Spring:Sink;
        Scalar(L"Y",show?0:HiddenOffset,show?420:260,0,c);
        Scalar(L"Zoom",show?1:.96f,show?420:260,0,c);
        Scalar(L"Alpha",show?1:0,show?140:180,show?0:80,c,true);
        Scalar(L"Width",show?full:base,show?460:260,show?40:0,c);
    }
    void Dimensions(float width,float baseWidth,float h=Height) {
        full=width;base=baseWidth;height=h;
        props.InsertScalar(L"Full",full);props.InsertScalar(L"Height",height);
    }
    void FloatShow(bool enabled,bool detail=false) {
        props.InsertScalar(L"Width",full);
        props.InsertScalar(L"Y",enabled?(detail?8:6):0);
        props.InsertScalar(L"Zoom",enabled?(detail?.97f:.98f):1);
        props.InsertScalar(L"Alpha",enabled?0:1);
        if(enabled){
            Scalar(L"Y",0,detail?320:260,0,Spring);
            Scalar(L"Zoom",1,detail?320:260,0,Spring);
            Scalar(L"Alpha",1,detail?200:150,0,Spring,true);
        }
    }
    void Scale(float dpiScale){scale=dpiScale;root.Scale({scale,scale,1});}
    void Close() noexcept {
        if(closed)return;closed=true;
        try{
            if(target){target.Root(nullptr);target.Close();}
            target=nullptr;motion=nullptr;content=nullptr;body=nullptr;root=nullptr;props=nullptr;
            tint=nullptr;stroke=nullptr;soft=nullptr;tight=nullptr;
            if(ownsCompositor&&compositor)compositor.Close();compositor=nullptr;
            if(dispatcher){
                auto action=dispatcher.ShutdownQueueAsync();
                // Its shutdown completes on this thread: pump messages, never
                // block the dispatcher by calling get() on its own thread.
                const auto end=GetTickCount64()+2000;
                while(action.Status()==wf::AsyncStatus::Started&&GetTickCount64()<end){
                    MsgWaitForMultipleObjectsEx(0,nullptr,20,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
                    MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE))
                        if(m.message!=WM_QUIT){TranslateMessage(&m);DispatchMessageW(&m);}
                }
                dispatcher=nullptr;
            }
        }catch(...){}
    }
    ~Scene(){Close();}
};
}
