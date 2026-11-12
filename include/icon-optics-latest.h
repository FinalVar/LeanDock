#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

// Alpha bounds, not executable names or replacement artwork. The longest
// visible edge determines scale; aspect ratio and all original pixels remain.
namespace LeanDockIcons {
struct Fit {
    bool valid{};
    double scale{1}, x{}, y{}; // translation as a fraction of the square icon box
    int left{}, top{}, right{}, bottom{};
    int fill{100}; // percent of the bounds the picture covers, weighted by alpha
};
// A notification icon whose picture covers at least this much of its bounds sits
// on a plate (a filled rounded square) and is fitted to the box like the others.
// Below it the picture is a glyph (dots, outlines, a figure): its app already drew
// it at tray size with tray padding, so it keeps that size and is only centred.
constexpr int PlateFill=75;
// tray: a notification icon may draw part of its picture faint on purpose
// (Tailscale's unlit dots). Faint content reaching well past the solid part
// moves that side out; a one pixel resampling fringe still does not. A tray
// glyph (fill below PlateFill) is not enlarged.
inline Fit Measure(std::span<const uint8_t> bgra,int width,int height,bool tray=false) noexcept {
    Fit fit;
    if(width<2||height<2||width>1024||height>1024||
       bgra.size()!=size_t(width)*size_t(height)*4)return fit;
    unsigned maximum=0;
    for(size_t i=3;i<bgra.size();i+=4)maximum=std::max(maximum,unsigned(bgra[i]));
    if(maximum<32)return fit; // empty/fading/unsupported capture: keep native
    // Half coverage keeps resampled edge fringes/shadows from expanding the
    // contour; relative coverage also supports intentionally translucent art.
    const unsigned threshold=std::max(16u,maximum/2);
    int left=width,top=height,right=0,bottom=0,count=0;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
        if(bgra[(size_t(y)*width+x)*4+3]>=threshold){
            left=std::min(left,x);top=std::min(top,y);
            right=std::max(right,x+1);bottom=std::max(bottom,y+1);++count;
        }
    if(count<std::max(4,width*height/100))return fit;
    // A rectangular source is uniformly fitted in the native square icon
    // box, so both axes use its longest edge, not two independent scales.
    const double canvas=std::max(width,height);
    if(tray){
        const unsigned faint=std::max(16u,maximum/8);
        int fl=width,ft=height,fr=0,fb=0;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x)
            if(bgra[(size_t(y)*width+x)*4+3]>=faint){
                fl=std::min(fl,x);ft=std::min(ft,y);fr=std::max(fr,x+1);fb=std::max(fb,y+1);
            }
        const int fringe=std::max(1,int(canvas/16));
        if(left-fl>fringe)left=fl;
        if(top-ft>fringe)top=ft;
        if(fr-right>fringe)right=fr;
        if(fb-bottom>fringe)bottom=fb;
    }
    const double extent=std::max(right-left,bottom-top)/canvas;
    if(extent<0.40)return fit; // tiny glyph/placeholder: don't invent its scale
    fit.valid=true;fit.left=left;fit.top=top;fit.right=right;fit.bottom=bottom;
    fit.scale=std::clamp(1.0/extent,1.0,1.35);
    if(tray){
        double ink=0;
        for(int y=top;y<bottom;++y)for(int x=left;x<right;++x)ink+=bgra[(size_t(y)*width+x)*4+3];
        fit.fill=int(std::lround(100*ink/(double(maximum)*(right-left)*(bottom-top))));
        if(fit.fill<PlateFill)fit.scale=1;
    }
    fit.x=(width-double(left+right))/(2*canvas)*fit.scale;
    fit.y=(height-double(top+bottom))/(2*canvas)*fit.scale;
    // Suppress subpixel sampling noise on already full-sized artwork.
    if(fit.scale<1.02&&std::abs(fit.x)<0.01&&std::abs(fit.y)<0.01)
        fit.scale=1,fit.x=fit.y=0;
    return fit;
}
// Longest edge for a copy of an oversized source, or 0 when it is fine as it is.
// Past about 2.5 source pixels per display pixel, composition's bilinear sampling
// skips source pixels and rounded corners turn to stairs. Dock icons get two source
// pixels per display pixel, so the copy does too.
inline int OversizedCopy(int source,double display) noexcept {
    if(!(display>=1)||source<2.5*display)return 0;
    return std::min(source,int(std::ceil(display*2)));
}
// Area average, so every source pixel counts. XAML bitmaps are premultiplied, so
// the channels average directly. Empty on invalid input.
inline std::vector<uint8_t> Shrink(std::span<const uint8_t> bgra,int width,int height,int toWidth,int toHeight) {
    std::vector<uint8_t> out;
    if(width<1||height<1||toWidth<1||toHeight<1||toWidth>width||toHeight>height||
       bgra.size()!=size_t(width)*size_t(height)*4)return out;
    out.resize(size_t(toWidth)*size_t(toHeight)*4);
    const double sx=double(width)/toWidth,sy=double(height)/toHeight;
    for(int y=0;y<toHeight;++y){
        const double y0=y*sy,y1=(y+1)*sy;
        const int vLast=std::min(height,int(std::ceil(y1)));
        for(int x=0;x<toWidth;++x){
            const double x0=x*sx,x1=(x+1)*sx;
            const int uLast=std::min(width,int(std::ceil(x1)));
            double sum[4]{},area=0;
            for(int v=int(y0);v<vLast;++v){
                const double wy=std::min(y1,v+1.0)-std::max(y0,double(v));
                for(int u=int(x0);u<uLast;++u){
                    const double w=wy*(std::min(x1,u+1.0)-std::max(x0,double(u)));
                    const uint8_t* p=&bgra[(size_t(v)*width+u)*4];
                    for(int c=0;c<4;++c)sum[c]+=p[c]*w;
                    area+=w;
                }
            }
            uint8_t* o=&out[(size_t(y)*toWidth+x)*4];
            for(int c=0;c<4;++c)o[c]=uint8_t(std::clamp(std::lround(sum[c]/area),0L,255L));
        }
    }
    return out;
}
// Fingerprint of pixel data, only ever compared with an earlier one of the same
// bitmap to notice a frame drawn into it.
inline uint64_t Fingerprint(std::span<const uint8_t> data) noexcept {
    uint64_t h=0x9E3779B97F4A7C15ull^data.size();size_t i=0;
    for(;i+8<=data.size();i+=8){uint64_t w;std::memcpy(&w,data.data()+i,8);h=(h^w)*0x100000001B3ull;h^=h>>29;}
    for(;i<data.size();++i)h=(h^data[i])*0x100000001B3ull;
    return h;
}
// Frames of one tray icon (a status dot, a spinner) measure a little differently.
// Within this much the current fit stays, so the icon does not tremble.
inline bool SameFit(const Fit& a,const Fit& b) noexcept {
    return std::abs(a.scale-b.scale)<0.03&&std::abs(a.x-b.x)<0.02&&std::abs(a.y-b.y)<0.02;
}
}
