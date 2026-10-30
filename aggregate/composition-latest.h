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
    // Floating panels: liquid-glass layers and the staggered content bands.
    comp::SpriteVisual glass{nullptr},lens{nullptr};
    comp::CompositionBrush shape{nullptr},ring{nullptr},band{nullptr};
    comp::CompositionLinearGradientBrush rimLight{nullptr},innerLight{nullptr};
    comp::CompositionRadialGradientBrush sheen{nullptr},glint{nullptr};
    comp::CompositionSurfaceBrush contentBrush{nullptr};
    std::vector<comp::SpriteVisual> bands;
    float full=400,base=120,scale=1,height=Height,radius=Radius;
    bool closed=false,floating=false,ownsCompositor=true,host=true;
    void Expression(comp::CompositionObject const& object,wchar_t const* property,
                    wchar_t const* expression) {
        auto a=compositor.CreateExpressionAnimation(expression);
        a.SetReferenceParameter(L"p",props);object.StartAnimation(property,a);
    }
    void SizeExpression(comp::CompositionObject const& object,wchar_t const* property=L"Size") {
        Expression(object,property,L"Vector2(p.Width, p.Height)");
    }
    // A rounded-rectangle outline inset into the animated panel bounds.
    comp::CompositionSpriteShape Outline(comp::ShapeVisual const& shape,float inset,float thickness) {
        auto geometry=compositor.CreateRoundedRectangleGeometry();
        geometry.Offset({inset,inset});geometry.CornerRadius({std::max(0.f,radius-inset),std::max(0.f,radius-inset)});
        Expression(geometry,L"Size",(L"Vector2(p.Width-"+std::to_wstring(2*inset)+L",p.Height-"+std::to_wstring(2*inset)+L")").c_str());
        auto line=compositor.CreateSpriteShape(geometry);line.StrokeThickness(thickness);
        shape.Shapes().Append(line);return line;
    }
    comp::CompositionSurfaceBrush ShapeMask(comp::ShapeVisual const& shape) {
        SizeExpression(shape);auto surface=compositor.CreateVisualSurface();surface.SourceVisual(shape);
        SizeExpression(surface,L"SourceSize");return compositor.CreateSurfaceBrush(surface);
    }
    comp::CompositionMaskBrush Masked(comp::CompositionBrush const& source,comp::CompositionBrush const& mask) {
        auto masked=compositor.CreateMaskBrush();masked.Source(source);masked.Mask(mask);return masked;
    }
    comp::SpriteVisual Layer(comp::CompositionBrush const& source,comp::CompositionBrush const& mask) {
        auto v=compositor.CreateSpriteVisual();SizeExpression(v);
        if(source)v.Brush(Masked(source,mask));
        body.Children().InsertAtTop(v);return v;
    }
    // Backdrop -> blur -> saturation, then out=k*c+o. A compressed, lifted
    // (light) or lowered (dark) range keeps text contrast on any wallpaper.
    comp::CompositionEffectBrush Glass(float amount,float saturation,float k,float o) {
        auto blur=winrt::make_self<GaussianBlurEffect>();
        blur->BlurAmount=amount;blur->BorderMode=D2D1_BORDER_MODE_HARD;
        blur->Source=comp::CompositionEffectSourceParameter(L"backdrop");
        auto tone=winrt::make_self<ColorMatrixEffect>();tone->Source=*blur;tone->ClampOutput=true;
        const float s=saturation,inv=1-s,r=.2126f,g=.7152f,b=.0722f;
        const float matrix[20]={(inv*r+s)*k,inv*r*k,inv*r*k,0,
                                inv*g*k,(inv*g+s)*k,inv*g*k,0,
                                inv*b*k,inv*b*k,(inv*b+s)*k,0,
                                0,0,0,1,o,o,o,0};
        std::copy(std::begin(matrix),std::end(matrix),tone->Matrix);
        auto effect=compositor.CreateEffectFactory(*tone).CreateBrush();
        // The probe has no window: it samples a fixture wallpaper behind the
        // panel in its own tree, through the same effect graph.
        effect.SetSourceParameter(L"backdrop",host?compositor.CreateHostBackdropBrush().as<comp::CompositionBrush>():
            compositor.CreateBackdropBrush().as<comp::CompositionBrush>());
        return effect;
    }
    static void Stops(comp::CompositionGradientBrush const& brush,comp::Compositor const& c,
                      std::initializer_list<std::pair<float,Color>> stops) {
        brush.ColorStops().Clear();
        for(auto [at,color]:stops)brush.ColorStops().Append(c.CreateColorGradientStop(at,color));
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
        root=compositor.CreateContainerVisual();
        if(hwnd){
            BOOL yes=TRUE;
            winrt::check_hresult(DwmSetWindowAttribute(hwnd,17,&yes,sizeof(yes)));
            winrt::check_hresult(compositor.as<CapsuleDesktopInterop>()->CreateDesktopWindowTarget(
                hwnd,TRUE,winrt::put_abi(target)));
            if(!target)winrt::throw_hresult(E_FAIL);
            target.Root(root);
        }
        scale=dpiScale;root.Scale({scale,scale,1});
        motion=compositor.CreateContainerVisual();root.Children().InsertAtTop(motion);
        props=compositor.CreatePropertySet();
        props.InsertScalar(L"Full",full);props.InsertScalar(L"Height",height);
        props.InsertScalar(L"X",32);
        props.InsertScalar(L"Width",base);props.InsertScalar(L"Y",HiddenOffset);
        props.InsertScalar(L"Zoom",.96f);props.InsertScalar(L"Alpha",0);
        // Specular light in panel DIPs; starts at the time key below.
        props.InsertVector2(L"Light",{full-36,height+18});
        Expression(motion,L"Offset",L"Vector3(p.X,32+p.Y,0)");
        // A floating panel grows out of the time key under its right end.
        Expression(motion,L"CenterPoint",floating?L"Vector3(p.Full-36,p.Height,0)":L"Vector3(p.Full,p.Height/2,0)");
        Expression(motion,L"Scale",L"Vector3(p.Zoom,p.Zoom,1)");
        Expression(motion,L"Opacity",L"p.Alpha");
        body=compositor.CreateContainerVisual();motion.Children().InsertAtTop(body);
        Expression(body,L"Offset",L"Vector3(p.Full-p.Width,0,0)");

        // Separate vector mask, sampled at the monitor's actual resolution.
        // No region clipping of the desktop window, so the external shadows
        // remain smooth. Hit testing uses the same animated rounded geometry.
        auto maskShape=compositor.CreateShapeVisual();
        auto round=compositor.CreateRoundedRectangleGeometry();
        round.CornerRadius({radius,radius});SizeExpression(round);
        auto maskFill=compositor.CreateSpriteShape(round);
        maskFill.FillBrush(compositor.CreateColorBrush(Color{255,255,255,255}));
        maskShape.Shapes().Append(maskFill);
        auto mask=ShapeMask(maskShape);shape=mask;

        auto shadowVisual=[&](float blur,float offset,comp::DropShadow& result) {
            auto v=compositor.CreateSpriteVisual();SizeExpression(v);
            result=compositor.CreateDropShadow();result.BlurRadius(blur);
            result.Offset({0,offset,0});result.Mask(mask);
            v.Shadow(result);body.Children().InsertAtTop(v);
        };
        // CSS box-shadow blur radius maps to approximately half that sigma.
        // Floating: stays inside the 32 DIP window margin below the panel.
        shadowVisual(floating?22:15,floating?9:10,soft);shadowVisual(1.5f,1,tight);

        glass=Layer(nullptr,nullptr);
        if(floating){
            // Edge lensing: near the rim the backdrop is sharper and more
            // vivid than the frosted centre, fading out over ~9 DIP. Built
            // from stepped vector outlines, so it follows every resize.
            auto falloff=compositor.CreateShapeVisual();
            for(int i=0;i<6;++i){
                const float a=std::pow(1-i/6.f,1.6f);
                Outline(falloff,.75f+1.5f*i,1.5f).StrokeBrush(compositor.CreateColorBrush(Color{uint8_t(255*a),255,255,255}));
            }
            band=ShapeMask(falloff);
            lens=Layer(nullptr,nullptr);
        }
        tint=compositor.CreateColorBrush();
        Layer(tint,mask);
        if(floating){
            // Pointer-following specular light: a broad sheen on the body and
            // a tight glint confined to the 1.5 DIP rim.
            sheen=compositor.CreateRadialGradientBrush();sheen.MappingMode(comp::CompositionMappingMode::Absolute);
            sheen.EllipseRadius({170,170});
            Expression(sheen,L"EllipseCenter",L"p.Light");
            Layer(sheen,mask);
            auto rim=compositor.CreateShapeVisual();SizeExpression(rim);
            rimLight=compositor.CreateLinearGradientBrush();rimLight.StartPoint({0,0});rimLight.EndPoint({1,1});
            Outline(rim,.5f,1).StrokeBrush(rimLight);
            innerLight=compositor.CreateLinearGradientBrush();innerLight.StartPoint({0,0});innerLight.EndPoint({0,1});
            Outline(rim,1.5f,1).StrokeBrush(innerLight);
            body.Children().InsertAtTop(rim);
            auto edge=compositor.CreateShapeVisual();
            Outline(edge,.75f,1.5f).StrokeBrush(compositor.CreateColorBrush(Color{255,255,255,255}));
            ring=ShapeMask(edge);
            glint=compositor.CreateRadialGradientBrush();glint.MappingMode(comp::CompositionMappingMode::Absolute);
            glint.EllipseRadius({120,120});
            Expression(glint,L"EllipseCenter",L"p.Light");
            Layer(glint,ring);
        }else{
            auto outline=compositor.CreateShapeVisual();SizeExpression(outline);
            stroke=compositor.CreateColorBrush();Outline(outline,.5f,1).StrokeBrush(stroke);
            body.Children().InsertAtTop(outline);
        }
        content=compositor.CreateContainerVisual();body.Children().InsertAtTop(content);
        Expression(content,L"Offset",L"Vector3(p.Width-p.Full,0,0)");
        auto clip=compositor.CreateInsetClip();content.Clip(clip);
        Expression(clip,L"LeftInset",L"Max(0,p.Full-p.Width)");
        Expression(content,L"Size",L"Vector2(p.Full,p.Height)");
        Theme();
    }
    // forced: 0 dark, 1 light (offscreen fixtures); -1 follows Windows.
    void Theme(int forced=-1) {
        DWORD light=1,clear=1,size=sizeof(DWORD);
        RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"SystemUsesLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);
        size=sizeof(DWORD);
        RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"EnableTransparency",RRF_RT_REG_DWORD,nullptr,&clear,&size);
        if(forced>=0){light=DWORD(forced);clear=1;}
        soft.Color(light?Color{255,15,20,30}:Color{255,0,0,0});
        tight.Color(light?Color{255,15,20,30}:Color{255,0,0,0});
        if(!floating){
            glass.Brush(Masked(Glass(22,1.6f,1,0),shape));
            tint.Color(light?Color{158,246,247,250}:Color{148,30,32,38});
            stroke.Color(light?Color{153,255,255,255}:Color{23,255,255,255});
            soft.Opacity(light?.20f:.45f);tight.Opacity(light?.12f:.35f);
            return;
        }
        // "Transparency effects" off in Settings: an opaque pane, no lensing.
        glass.IsVisible(clear);lens.IsVisible(clear);
        if(clear){
            glass.Brush(Masked(Glass(30,1.8f,.5f,light?.47f:0),shape));
            lens.Brush(Masked(Glass(5,2.1f,.56f,light?.42f:0),band));
        }
        tint.Color(!clear?(light?Color{255,243,244,247}:Color{255,32,34,40}):
                   light?Color{158,250,251,253}:Color{158,28,30,36});
        constexpr Color none{0,255,255,255};
        auto white=[](float a){return Color{uint8_t(std::lround(255*a)),255,255,255};};
        Stops(rimLight,compositor,{{0,white(light?.95f:.32f)},{.5f,white(light?.55f:.12f)},{1,white(light?.30f:.06f)}});
        Stops(innerLight,compositor,{{0,white(light?.55f:.12f)},{.35f,none},{1,none}});
        Stops(sheen,compositor,{{0,white(light?.30f:.075f)},{1,none}});
        Stops(glint,compositor,{{0,white(light?1:.6f)},{.6f,white(light?.25f:.12f)},{1,none}});
        soft.Opacity(light?.18f:.50f);tight.Opacity(light?.10f:.40f);
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
    // Content is one painted surface shown through horizontal bands, so a
    // reveal can cascade section by section without extra drawing.
    void Content(comp::ICompositionSurface const& surface,std::vector<float> const& cuts) {
        if(!contentBrush){contentBrush=compositor.CreateSurfaceBrush();contentBrush.Stretch(comp::CompositionStretch::Fill);}
        if(contentBrush.Surface()!=surface)contentBrush.Surface(surface);
        const size_t count=std::max<size_t>(1,cuts.size()>1?cuts.size()-1:1);
        while(bands.size()<count){
            auto v=compositor.CreateSpriteVisual();v.Brush(contentBrush);v.Clip(compositor.CreateInsetClip());
            content.Children().InsertAtTop(v);bands.push_back(v);
        }
        while(bands.size()>count){content.Children().Remove(bands.back());bands.pop_back();}
        for(size_t i=0;i<count;++i){
            const float top=cuts.size()>1?cuts[i]:0,bottom=cuts.size()>1?cuts[i+1]:height;
            bands[i].Size({full,height});auto clip=bands[i].Clip().as<comp::InsetClip>();
            clip.TopInset(i?top:0);clip.BottomInset(i+1<count?std::max(0.f,height-bottom):0);
        }
    }
    void Light(float x,float y){props.InsertVector2(L"Light",{x,y});}
    // Panel reveal and leave. One table drives the compositor animations
    // and RevealFrame/LeaveFrame, the static frames the offscreen probe uses
    // (a detached capture tree does not advance compositor animations).
    struct Timing {
        static constexpr float Rise=12,Zoom=.94f,BandRise=10,LeaveZoom=.97f,LeaveSink=6;
        static constexpr int RiseMs=380,ZoomMs=420,FadeMs=160,BandDelay=50,BandStep=28,BandMs=420,BandFadeMs=300;
        static constexpr int LeaveFadeMs=130,LeaveMs=150;
        static int BandAt(size_t i){return BandDelay+BandStep*int(i);}
    };
    void FloatShow(bool enabled,bool detail=false) {
        using T=Timing;
        props.InsertScalar(L"Width",full);
        props.InsertScalar(L"Y",enabled?(detail?T::Rise:6):0);
        props.InsertScalar(L"Zoom",enabled?(detail?T::Zoom:.98f):1);
        props.InsertScalar(L"Alpha",enabled?0:1);
        for(auto const& v:bands){v.StopAnimation(L"Offset");v.StopAnimation(L"Opacity");v.Offset({0,0,0});v.Opacity(1);}
        if(!enabled)return;
        Scalar(L"Y",0,detail?T::RiseMs:260,0,detail?Lift:Spring);
        Scalar(L"Zoom",1,detail?T::ZoomMs:260,0,detail?Lift:Spring);
        Scalar(L"Alpha",1,detail?T::FadeMs:150,0,Spring,true);
        if(!detail)return;
        // Sections follow the glass in, top to bottom, each settling upward.
        const auto settle=compositor.CreateCubicBezierEasingFunction({float(Settle.x1),float(Settle.y1)},{float(Settle.x2),float(Settle.y2)});
        for(size_t i=0;i<bands.size();++i){
            const auto delay=std::chrono::milliseconds(T::BandAt(i));
            auto offset=compositor.CreateVector3KeyFrameAnimation();
            offset.InsertKeyFrame(0,{0,T::BandRise,0});offset.InsertKeyFrame(1,{0,0,0},settle);
            offset.Duration(std::chrono::milliseconds(T::BandMs));offset.DelayTime(delay);
            offset.DelayBehavior(comp::AnimationDelayBehavior::SetInitialValueBeforeDelay);
            auto fade=compositor.CreateScalarKeyFrameAnimation();
            fade.InsertKeyFrame(0,0);fade.InsertKeyFrame(1,1,settle);
            fade.Duration(std::chrono::milliseconds(T::BandFadeMs));fade.DelayTime(delay);
            fade.DelayBehavior(comp::AnimationDelayBehavior::SetInitialValueBeforeDelay);
            bands[i].StartAnimation(L"Offset",offset);bands[i].StartAnimation(L"Opacity",fade);
        }
    }
    void RevealFrame(double ms) {
        using T=Timing;
        props.InsertScalar(L"Width",full);
        props.InsertScalar(L"Y",float(Mix(T::Rise,0,Progress(ms,T::RiseMs,Lift))));
        props.InsertScalar(L"Zoom",float(Mix(T::Zoom,1,Progress(ms,T::ZoomMs,Lift))));
        props.InsertScalar(L"Alpha",float(std::clamp(ms/T::FadeMs,0.,1.)));
        for(size_t i=0;i<bands.size();++i){
            const double local=ms-T::BandAt(i);
            bands[i].Offset({0,local<=0?T::BandRise:float(Mix(T::BandRise,0,Progress(local,T::BandMs,Settle))),0});
            bands[i].Opacity(local<=0?0:float(Progress(local,T::BandFadeMs,Settle)));
        }
    }
    // Leaving is quicker than arriving and accelerates away, sinking slightly
    // toward the key. Returns the time after which the window can be hidden.
    int FloatHide(bool enabled) {
        using T=Timing;
        if(!enabled)return 0;
        Scalar(L"Alpha",0,T::LeaveFadeMs,0,Sink,true);
        Scalar(L"Zoom",T::LeaveZoom,T::LeaveMs,0,Sink);
        Scalar(L"Y",T::LeaveSink,T::LeaveMs,0,Sink);
        return T::LeaveMs;
    }
    void LeaveFrame(double ms) {
        using T=Timing;
        props.InsertScalar(L"Alpha",float(1-std::clamp(ms/T::LeaveFadeMs,0.,1.)));
        props.InsertScalar(L"Zoom",float(Mix(1,T::LeaveZoom,Progress(ms,T::LeaveMs,Sink))));
        props.InsertScalar(L"Y",float(Mix(0,T::LeaveSink,Progress(ms,T::LeaveMs,Sink))));
    }
    void Scale(float dpiScale){scale=dpiScale;root.Scale({scale,scale,1});}
    void Close() noexcept {
        if(closed)return;closed=true;
        try{
            if(target){target.Root(nullptr);target.Close();}
            target=nullptr;motion=nullptr;content=nullptr;body=nullptr;root=nullptr;props=nullptr;
            tint=nullptr;stroke=nullptr;soft=nullptr;tight=nullptr;glass=nullptr;lens=nullptr;shape=nullptr;ring=nullptr;band=nullptr;
            rimLight=nullptr;innerLight=nullptr;sheen=nullptr;glint=nullptr;contentBrush=nullptr;bands.clear();
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
