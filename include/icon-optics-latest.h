#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

// Alpha bounds, not executable names or replacement artwork. The longest
// visible edge determines scale; aspect ratio and all original pixels remain.
namespace LeanDockIcons {
struct Fit {
    bool valid{};
    double scale{1}, x{}, y{}; // translation as a fraction of the square icon box
    int left{}, top{}, right{}, bottom{};
    // Enlarged past ordinary padding: cut to the artwork, as fractions of the
    // box, so what else the canvas holds does not land on neighbouring icons.
    bool clip{};
    double clipLeft{}, clipTop{}, clipRight{1}, clipBottom{1};
};
inline Fit Measure(std::span<const uint8_t> bgra,int width,int height) noexcept {
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
    if(count<std::max(4,width*height/400))return fit;
    // A rectangular source is uniformly fitted in the native square icon
    // box, so both axes use its longest edge, not two independent scales.
    const double canvas=std::max(width,height);
    const double extent=std::max(right-left,bottom-top)/canvas;
    if(extent<0.08)return fit; // a speck, not artwork
    // Windows can hand over a small icon centred on a large canvas with a
    // faint frame (MinerU, 2026-11-07: about 72 px in 256), and
    // apps can pad as they like: either way the artwork fills the box. Only a
    // solid core in a large translucent body stays bounded, since the body is
    // the icon there. The canvas's outer ring is where such frames lie.
    int bodyLeft=left,bodyTop=top,bodyRight=right,bodyBottom=bottom;
    const int ring=std::max(1,int(canvas*0.03));
    const unsigned visible=std::min(threshold,32u);
    for(int y=ring;y<height-ring;++y)for(int x=ring;x<width-ring;++x)
        if(bgra[(size_t(y)*width+x)*4+3]>=visible){
            bodyLeft=std::min(bodyLeft,x);bodyTop=std::min(bodyTop,y);
            bodyRight=std::max(bodyRight,x+1);bodyBottom=std::max(bodyBottom,y+1);
        }
    const double body=std::max(bodyRight-bodyLeft,bodyBottom-bodyTop)/canvas;
    const double limit=std::max(1.35,1.0/body);
    if(extent<0.40&&limit==1.35)return fit; // small core, large body: keep native
    fit.valid=true;fit.left=left;fit.top=top;fit.right=right;fit.bottom=bottom;
    fit.scale=std::clamp(1.0/extent,1.0,limit);
    if(fit.scale>1.35){
        // A little margin keeps the artwork's soft edge.
        const double padX=(canvas-width)/2.0,padY=(canvas-height)/2.0,margin=0.015;
        fit.clip=true;
        fit.clipLeft=std::max(0.0,(bodyLeft+padX)/canvas-margin);fit.clipTop=std::max(0.0,(bodyTop+padY)/canvas-margin);
        fit.clipRight=std::min(1.0,(bodyRight+padX)/canvas+margin);fit.clipBottom=std::min(1.0,(bodyBottom+padY)/canvas+margin);
    }
    fit.x=(width-double(left+right))/(2*canvas)*fit.scale;
    fit.y=(height-double(top+bottom))/(2*canvas)*fit.scale;
    // Suppress subpixel sampling noise on already full-sized artwork.
    if(fit.scale<1.02&&std::abs(fit.x)<0.01&&std::abs(fit.y)<0.01)
        fit.scale=1,fit.x=fit.y=0;
    return fit;
}
}
