#pragma once
#include "time-key-latest.h"

namespace Capsule {
// Production painter -> private D3D texture -> PNG. No desktop capture,
// visible window, input, activation, tray registration or system changes.
inline void VerifyTimeKey(Drawing& draw) {
    const auto output=Directory()+L"\\time-key-probe";
    std::filesystem::create_directories(output);
    const auto geometryReport=output+L"\\geometry-latest.csv";
    FILE* geometryFile=_wfopen(geometryReport.c_str(),L"w");
    if(!geometryFile)winrt::throw_last_error();
    fprintf(geometryFile,"percent,length,expected,left,top,right,bottom\n");fclose(geometryFile);
    for(int percent:{1,18,50,72,97,99}){
        auto path=TimeRingPath(draw,percent);float length=0;D2D1_RECT_F bounds{};
        winrt::check_hresult(path->ComputeLength(nullptr,.01f,&length));
        // This is an open, hollow stroke, so validate its stroked bounds.
        // Fill-only GetBounds reports an empty rectangle for a short line.
        winrt::check_hresult(path->GetWidenedBounds(TimeRingStroke,nullptr,nullptr,.01f,&bounds));
        const double perimeter=2*(TimeWidth-TimeHeight)+2*RingPi*(TimeHeight/2-TimeRingInset);
        geometryFile=_wfopen(geometryReport.c_str(),L"a");
        if(!geometryFile)winrt::throw_last_error();
        fprintf(geometryFile,"%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",percent,length,
            perimeter*percent/100,bounds.left,bounds.top,bounds.right,bounds.bottom);fclose(geometryFile);
        if(!std::isfinite(length)||!std::isfinite(bounds.left)||!std::isfinite(bounds.top)||
           !std::isfinite(bounds.right)||!std::isfinite(bounds.bottom)||
           std::abs(length-perimeter*percent/100)>.15||bounds.left<0||bounds.top<0||
           bounds.right>TimeWidth||bounds.bottom>TimeHeight)winrt::throw_hresult(E_UNEXPECTED);
    }
    for(auto value:{L"00:00",L"11:11",L"23:59"}){
        auto layout=draw.Layout(value,18,TimeWidth,22,true,false,L"Segoe UI");
        DWRITE_TEXT_METRICS m{};winrt::check_hresult(layout->GetMetrics(&m));
        if(m.widthIncludingTrailingWhitespace>TimeWidth-12)winrt::throw_hresult(E_BOUNDS);
    }
    Power power;power.valid=true;power.value.BatteryFlag=0;
    for(auto percent:{0,100,101,255}){
        power.value.BatteryLifePercent=BYTE(percent);
        if(power.Present()!=(percent<=100))winrt::throw_hresult(E_UNEXPECTED);
    }
    power.value.BatteryLifePercent=50;power.value.BatteryFlag=128;
    if(power.Present())winrt::throw_hresult(E_UNEXPECTED);

    const auto original=draw.colors;
    unsigned renders=0;
    try{
        for(float scale:{1.f,1.25f,1.5f,1.75f,2.f}){
            winrt::com_ptr<ID2D1DeviceContext> dc;
            winrt::check_hresult(draw.device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,dc.put()));
            const UINT width=UINT(std::lround(600*scale)),height=UINT(std::lround(264*scale));
            D2D1_BITMAP_PROPERTIES1 props{};
            props.pixelFormat={DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED};
            props.dpiX=props.dpiY=96*scale;props.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            winrt::com_ptr<ID2D1Bitmap1> target;
            winrt::check_hresult(dc->CreateBitmap({width,height},nullptr,0,&props,target.put()));
            dc->SetTarget(target.get());dc->SetDpi(96*scale,96*scale);
            dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            dc->BeginDraw();dc->Clear(Ink(0xF4F5F3));
            for(int row=0;row<3;++row){
                draw.colors.light=row!=1;
                draw.colors.text=Ink(row==1?0xF3F5F9:0x1D252D);
                draw.colors.secondary=Ink(row==1?0xBBC4D1:0x58636F);
                winrt::com_ptr<ID2D1SolidColorBrush> fill;
                winrt::check_hresult(dc->CreateSolidColorBrush(Ink(row==0?0xE1E6F0:row==1?0x2D3340:0x9AAEE6),fill.put()));
                dc->FillRectangle({0,float(row*88),600,float((row+1)*88)},fill.get());
                int column=0;
                for(int percent:{-1,0,18,72,97,100}){
                    const float x=14+100.f*column++,y=32+88.f*row;
                    const auto label=percent<0?L"未知":std::to_wstring(percent)+L"%";
                    draw.Text(dc.get(),label,{x,y-25,TimeWidth,18},12,draw.colors.secondary);
                    dc->SetTransform(D2D1::Matrix3x2F::Translation(x,y));
                    PaintTimeKey(draw,dc.get(),L"23:59",L"10/22",percent);
                    dc->SetTransform(D2D1::Matrix3x2F::Identity());++renders;
                }
            }
            winrt::check_hresult(dc->EndDraw());dc->SetTarget(nullptr);
            props.bitmapOptions=D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            winrt::com_ptr<ID2D1Bitmap1> readback;
            winrt::check_hresult(dc->CreateBitmap({width,height},nullptr,0,&props,readback.put()));
            winrt::check_hresult(readback->CopyFromBitmap(nullptr,target.get(),nullptr));
            D2D1_MAPPED_RECT mapped{};
            winrt::check_hresult(readback->Map(D2D1_MAP_OPTIONS_READ,&mapped));
            std::vector<BYTE> pixels;
            try{pixels.resize(size_t(width)*height*4);
                for(UINT row=0;row<height;++row)memcpy(pixels.data()+size_t(row)*width*4,mapped.bits+size_t(row)*mapped.pitch,width*4);
            }catch(...){readback->Unmap();throw;}
            winrt::check_hresult(readback->Unmap());
            winrt::com_ptr<IWICBitmap> bitmap;
            winrt::check_hresult(draw.wic->CreateBitmapFromMemory(width,height,GUID_WICPixelFormat32bppPBGRA,
                width*4,UINT(pixels.size()),pixels.data(),bitmap.put()));
            winrt::com_ptr<IWICStream> stream;winrt::check_hresult(draw.wic->CreateStream(stream.put()));
            const auto name=output+L"\\dpi-"+std::to_wstring(int(std::lround(scale*100)))+L".png";
            winrt::check_hresult(stream->InitializeFromFilename(name.c_str(),GENERIC_WRITE));
            winrt::com_ptr<IWICBitmapEncoder> encoder;
            winrt::check_hresult(draw.wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.put()));
            winrt::check_hresult(encoder->Initialize(stream.get(),WICBitmapEncoderNoCache));
            winrt::com_ptr<IWICBitmapFrameEncode> frame;
            winrt::check_hresult(encoder->CreateNewFrame(frame.put(),nullptr));
            winrt::check_hresult(frame->Initialize(nullptr));
            winrt::check_hresult(frame->WriteSource(bitmap.get(),nullptr));
            winrt::check_hresult(frame->Commit());winrt::check_hresult(encoder->Commit());
        }
    }catch(...){draw.colors=original;throw;}
    draw.colors=original;
    FILE* report=_wfopen((output+L"\\latest.json").c_str(),L"w");
    if(!report)winrt::throw_last_error();
    fprintf(report,"{\"productionPainter\":true,\"desktopCapture\":false,\"renders\":%u,"
        "\"dpiPercent\":[100,125,150,175,200],\"states\":[-1,0,18,72,97,100],\"widthDip\":72,\"heightDip\":46}\n",renders);
    fclose(report);
}
}
