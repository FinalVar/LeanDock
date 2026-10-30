#pragma once
namespace Capsule {
inline void VerifyIconAlpha(Drawing& draw){
    // Real straight-alpha HBITMAP -> WIC -> final premultiplied raster. The
    // almost transparent white sample reproduces the observed Qiye fringe.
    BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),4,-1,1,32,BI_RGB,0,0,0,0,0};
    void* data{};HBITMAP raw=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&data,nullptr,0);
    if(!raw||!data)winrt::throw_last_error();
    const BYTE source[]={254,254,254,1, 128,64,32,128, 255,255,255,0, 16,32,64,255};
    memcpy(data,source,sizeof(source));
    auto bitmap=draw.Bitmap(raw);DeleteObject(raw);
    winrt::com_ptr<IWICFormatConverter> converted;
    winrt::check_hresult(draw.wic->CreateFormatConverter(converted.put()));
    winrt::check_hresult(converted->Initialize(bitmap.get(),GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    BYTE pixels[16]{};winrt::check_hresult(converted->CopyPixels(nullptr,16,16,pixels));
    const BYTE expected[]={1,1,1,1, 64,32,16,128, 0,0,0,0, 16,32,64,255};
    for(unsigned i=0;i<16;++i)if(std::abs(int(pixels[i])-expected[i])>1)winrt::throw_hresult(E_UNEXPECTED);
    Log(L"icon-alpha-fixture-passed",4);
}
inline bool ValidPremultiplied(Drawing& draw,IWICBitmapSource* image){
    UINT width{},height{};winrt::check_hresult(image->GetSize(&width,&height));
    winrt::com_ptr<IWICFormatConverter> converted;
    winrt::check_hresult(draw.wic->CreateFormatConverter(converted.put()));
    winrt::check_hresult(converted->Initialize(image,GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    std::vector<BYTE> pixels(size_t(width)*height*4);
    winrt::check_hresult(converted->CopyPixels(nullptr,width*4,UINT(pixels.size()),pixels.data()));
    for(size_t i=0;i<pixels.size();i+=4)
        if(std::max({pixels[i],pixels[i+1],pixels[i+2]})>pixels[i+3])return false;
    return true;
}
}
