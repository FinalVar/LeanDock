#pragma once
#include "composition-latest.h"
#include "../include/icon-optics-latest.h"
#include <d3d11.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <functional>
#include <map>

// Public Windows SDK interop ABIs. No compositor injection or private offsets.
struct CapsuleCompositorInterop : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateCompositionSurfaceForHandle(HANDLE,void**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateCompositionSurfaceForSwapChain(IUnknown*,void**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateGraphicsDevice(IUnknown*,void**)=0;
};
struct CapsuleDrawingInterop : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE BeginDraw(const RECT*,REFIID,void**,POINT*)=0;
    virtual HRESULT STDMETHODCALLTYPE EndDraw()=0;
    virtual HRESULT STDMETHODCALLTYPE Resize(SIZE)=0;
    virtual HRESULT STDMETHODCALLTYPE Scroll(const RECT*,const RECT*,int,int)=0;
    virtual HRESULT STDMETHODCALLTYPE ResumeDraw()=0;
    virtual HRESULT STDMETHODCALLTYPE SuspendDraw()=0;
};
template<> inline constexpr winrt::guid winrt::impl::guid_v<CapsuleCompositorInterop>{
    0x25297D5C,0x3AD4,0x4C9C,{0xB5,0xCF,0xE3,0x6A,0x38,0x51,0x23,0x30}};
template<> inline constexpr winrt::guid winrt::impl::guid_v<CapsuleDrawingInterop>{
    0xFD04E6E3,0xFE0C,0x4C3C,{0xAB,0x19,0xA0,0x76,0x01,0xA5,0x76,0xEE}};

namespace Capsule {
struct AppArtwork {
    winrt::com_ptr<IWICBitmap> image;
    UINT width{},height{};
    LeanDockIcons::Fit fit;
};
inline D2D1_COLOR_F Ink(unsigned hex,float alpha=1) {
    return {float((hex>>16)&255)/255,float((hex>>8)&255)/255,float(hex&255)/255,alpha};
}
struct Palette {
    bool light=true;
    D2D1_COLOR_F text{},secondary{},hair{},hover{},warn{},danger{},accent{};
    Palette() {Refresh();}
    void Refresh() {
        DWORD v=1,n=sizeof(v);
        RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"SystemUsesLightTheme",RRF_RT_REG_DWORD,nullptr,&v,&n);light=v;
        text=Ink(light?0x15171B:0xECEEF2);secondary=Ink(light?0x5E6571:0x9EA5B1);
        hair=Ink(light?0x141820:0xFFFFFF,.10f);hover=Ink(light?0x141820:0xFFFFFF,light?.07f:.08f);
        warn=Ink(light?0xB97708:0xE6A23A);danger=Ink(light?0xD23C33:0xF06A60);
        accent=Ink(light?0x0A7470:0x3CC2B7);
    }
};
struct Drawing {
    winrt::com_ptr<ID3D11Device> d3d;
    winrt::com_ptr<ID2D1Factory1> factory;
    winrt::com_ptr<ID2D1Device> device;
    winrt::com_ptr<IDWriteFactory> write;
    winrt::com_ptr<IWICImagingFactory> wic;
    comp::CompositionGraphicsDevice graphics{nullptr};
    std::wstring font=L"Segoe UI";
    Palette colors;
    void Init(comp::Compositor const& compositor) {
        winrt::check_hresult(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,d3d.put(),nullptr,nullptr));
        D2D1_FACTORY_OPTIONS options{};
        winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),
            &options,factory.put_void()));
        auto dxgi=d3d.as<IDXGIDevice>();
        winrt::check_hresult(factory->CreateDevice(dxgi.get(),device.put()));
        winrt::check_hresult(compositor.as<CapsuleCompositorInterop>()->CreateGraphicsDevice(
            device.get(),winrt::put_abi(graphics)));
        winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write.put())));
        winrt::check_hresult(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,
            __uuidof(IWICImagingFactory),wic.put_void()));
        winrt::com_ptr<IDWriteFontCollection> collection;
        winrt::check_hresult(write->GetSystemFontCollection(collection.put()));
        for(auto name:{L"MiSans",L"HarmonyOS Sans SC",L"Microsoft YaHei UI",L"Segoe UI"}){
            UINT32 index;BOOL exists;
            if(SUCCEEDED(collection->FindFamilyName(name,&index,&exists))&&exists){font=name;break;}
        }
    }
    winrt::com_ptr<IDWriteTextLayout> Layout(std::wstring const& s,float size,float width,float height,
                                           bool semi=false,bool right=false,const wchar_t* face=nullptr) {
        winrt::com_ptr<IDWriteTextFormat> format;
        winrt::check_hresult(write->CreateTextFormat(face?face:font.c_str(),nullptr,semi?DWRITE_FONT_WEIGHT_SEMI_BOLD:
            DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"zh-CN",format.put()));
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        format->SetTextAlignment(right?DWRITE_TEXT_ALIGNMENT_TRAILING:DWRITE_TEXT_ALIGNMENT_LEADING);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        winrt::com_ptr<IDWriteInlineObject> ellipsis;
        write->CreateEllipsisTrimmingSign(format.get(),ellipsis.put());
        DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};
        format->SetTrimming(&trimming,ellipsis.get());
        winrt::com_ptr<IDWriteTextLayout> layout;
        winrt::check_hresult(write->CreateTextLayout(s.c_str(),UINT32(s.size()),format.get(),width,height,layout.put()));
        winrt::com_ptr<IDWriteTypography> type;
        if(SUCCEEDED(write->CreateTypography(type.put()))){
            type->AddFontFeature({DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES,1});
            layout->SetTypography(type.get(),{0,UINT32(s.size())});
        }return layout;
    }
    float Measure(std::wstring const& s,float size,bool semi=false) {
        auto layout=Layout(s,size,4096,64,semi);DWRITE_TEXT_METRICS m{};layout->GetMetrics(&m);
        return std::ceil(m.widthIncludingTrailingWhitespace);
    }
    comp::CompositionDrawingSurface Surface(float width,float height,float dpi,
        std::function<void(ID2D1DeviceContext*)> paint) {
        using namespace winrt::Windows::Graphics::DirectX;
        auto surface=graphics.CreateDrawingSurface({std::ceil(width*dpi),std::ceil(height*dpi)},
            DirectXPixelFormat::B8G8R8A8UIntNormalized,DirectXAlphaMode::Premultiplied);
        auto interop=surface.as<CapsuleDrawingInterop>();
        winrt::com_ptr<ID2D1DeviceContext> dc;POINT offset{};
        winrt::check_hresult(interop->BeginDraw(nullptr,__uuidof(ID2D1DeviceContext),dc.put_void(),&offset));
        try {
            dc->SetDpi(96*dpi,96*dpi);
            dc->SetTransform(D2D1::Matrix3x2F::Translation(float(offset.x)/dpi,float(offset.y)/dpi));
            dc->Clear({0,0,0,0});
            // Grayscale AA is correct for transparent composition surfaces.
            dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            paint(dc.get());
        }catch(...){interop->EndDraw();throw;}
        winrt::check_hresult(interop->EndDraw());return surface;
    }
    void Text(ID2D1DeviceContext* dc,std::wstring const& s,Rect r,float size,D2D1_COLOR_F color,
              bool semi=false,bool right=false) {
        auto layout=Layout(s,size,float(r.w),float(r.h),semi,right);
        winrt::com_ptr<ID2D1SolidColorBrush> b;winrt::check_hresult(dc->CreateSolidColorBrush(color,b.put()));
        dc->DrawTextLayout({float(r.x),float(r.y)},layout.get(),b.get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    void Round(ID2D1DeviceContext* dc,Rect r,float radius,D2D1_COLOR_F color) {
        winrt::com_ptr<ID2D1SolidColorBrush> b;winrt::check_hresult(dc->CreateSolidColorBrush(color,b.put()));
        dc->FillRoundedRectangle({{float(r.x),float(r.y),float(r.x+r.w),float(r.y+r.h)},radius,radius},b.get());
    }
    void Line(ID2D1DeviceContext* dc,float x,float y,float xx,float yy,D2D1_COLOR_F color,float width=1) {
        winrt::com_ptr<ID2D1SolidColorBrush> b;winrt::check_hresult(dc->CreateSolidColorBrush(color,b.put()));
        dc->DrawLine({x,y},{xx,yy},b.get(),width);
    }
    winrt::com_ptr<IWICBitmap> Bitmap(HBITMAP image) {
        // Shell's ICONONLY image contains straight BGRA (verified against the
        // embedded PNG). Declaring it PBGRA makes e.g. RGB=254,A=1 emit almost
        // white. Keep the alpha, then let RasterArtwork premultiply once.
        winrt::com_ptr<IWICBitmap> bitmap;if(image)wic->CreateBitmapFromHBITMAP(image,nullptr,WICBitmapUseAlpha,bitmap.put());
        return bitmap;
    }
    winrt::com_ptr<IWICBitmap> Icon(HICON icon) {
        winrt::com_ptr<IWICBitmap> bitmap;if(icon)wic->CreateBitmapFromHICON(icon,bitmap.put());return bitmap;
    }
    AppArtwork Artwork(winrt::com_ptr<IWICBitmap> image) {
        AppArtwork a{image};if(!image)return a;
        if(FAILED(image->GetSize(&a.width,&a.height))||a.width<2||a.height<2||
           a.width>1024||a.height>1024)return a;
        winrt::com_ptr<IWICFormatConverter> converter;
        if(FAILED(wic->CreateFormatConverter(converter.put()))||FAILED(converter->Initialize(
            image.get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return a;
        std::vector<uint8_t> pixels(size_t(a.width)*a.height*4);
        if(SUCCEEDED(converter->CopyPixels(nullptr,a.width*4,UINT(pixels.size()),pixels.data())))
            a.fit=LeanDockIcons::Measure(pixels,int(a.width),int(a.height));
        return a;
    }
    comp::ContainerVisual AppIcon(comp::Compositor const& c,AppArtwork const& a,float size,float dpi) {
        const float factor=a.fit.valid?float(a.fit.scale):1;
        const float tx=a.fit.valid?float(a.fit.x)*size:0,ty=a.fit.valid?float(a.fit.y)*size:0;
        const float longest=float(std::max<UINT>(1,std::max(a.width,a.height)));
        const float w=size*factor*a.width/longest,h=size*factor*a.height/longest;
        auto icon=c.CreateContainerVisual();icon.Size({size,size});icon.CenterPoint({size/2,size/2,0});
        if(!a.image||!a.width||!a.height)return icon;
        // Downsample once with area filtering at the FINAL optical size in
        // physical pixels, then upload 1:1. The old size-rounded DIP surface
        // was stretched again by Composition and softened static icon edges.
        const UINT pw=std::max(1u,UINT(std::lround(w*dpi))),ph=std::max(1u,UINT(std::lround(h*dpi)));
        auto raster=RasterArtwork(a,pw,ph);
        auto surface=Surface(float(pw),float(ph),1,[&](ID2D1DeviceContext* dc){
            Image(dc,raster.get(),{0,0,double(pw),double(ph)});
        });
        auto art=Visual(c,surface,pw/dpi,ph/dpi);
        art.Offset({std::round(((size-pw/dpi)/2+tx)*dpi)/dpi,std::round(((size-ph/dpi)/2+ty)*dpi)/dpi,0});
        icon.Children().InsertAtTop(art);return icon;
    }
    winrt::com_ptr<IWICBitmap> RasterArtwork(AppArtwork const& a,UINT width,UINT height) {
        winrt::com_ptr<IWICFormatConverter> converter;
        winrt::check_hresult(wic->CreateFormatConverter(converter.put()));
        winrt::check_hresult(converter->Initialize(a.image.get(),GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
        winrt::com_ptr<IWICBitmapScaler> scaler;
        winrt::check_hresult(wic->CreateBitmapScaler(scaler.put()));
        winrt::check_hresult(scaler->Initialize(converter.get(),width,height,WICBitmapInterpolationModeFant));
        winrt::com_ptr<IWICBitmap> bitmap;
        winrt::check_hresult(wic->CreateBitmapFromSource(scaler.get(),WICBitmapCacheOnLoad,bitmap.put()));
        winrt::check_hresult(bitmap->SetResolution(96,96));return bitmap;
    }
    void Image(ID2D1DeviceContext* dc,IWICBitmapSource* image,Rect r,float radius=0) {
        if(!image)return;
        winrt::com_ptr<ID2D1Bitmap> bitmap;
        if(SUCCEEDED(dc->CreateBitmapFromWicBitmap(image,nullptr,bitmap.put()))){
            winrt::com_ptr<ID2D1RoundedRectangleGeometry> clip;
            if(radius>0){
                winrt::check_hresult(factory->CreateRoundedRectangleGeometry(
                    {{float(r.x),float(r.y),float(r.x+r.w),float(r.y+r.h)},radius,radius},clip.put()));
                dc->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),clip.get()),nullptr);
            }
            dc->DrawBitmap(bitmap.get(),{float(r.x),float(r.y),float(r.x+r.w),float(r.y+r.h)},
                1,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,nullptr);
            if(clip)dc->PopLayer();
        }
    }
    comp::SpriteVisual Visual(comp::Compositor const& c,comp::CompositionDrawingSurface const& s,
                              float width,float height) {
        auto v=c.CreateSpriteVisual();v.Size({width,height});
        auto brush=c.CreateSurfaceBrush(s);brush.Stretch(comp::CompositionStretch::Fill);
        v.Brush(brush);return v;
    }
};
}
