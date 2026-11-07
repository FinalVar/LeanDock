#include "icon-optics-latest.h"
#include "icon-resolution-latest.h"
#include <cstdio>
#include <vector>
#include <cstdlib>
int checks=0,failures=0;
void Check(bool ok,const char* why){++checks;if(!ok){++failures;std::printf("FAIL %s\n",why);}}
bool Near(double a,double b){return std::abs(a-b)<1e-8;}
std::vector<uint8_t> Box(int width,int height,int left,int top,int right,int bottom,uint8_t alpha=255){
    std::vector<uint8_t> p(width*height*4);
    for(int y=top;y<bottom;++y)for(int x=left;x<right;++x)p[(y*width+x)*4+3]=alpha;
    return p;
}
int main(){
    using LeanDockIcons::Measure;
    auto full=Measure(Box(64,64,0,0,64,64),64,64);
    Check(full.valid,"full artwork accepted");Check(Near(full.scale,1),"full icon never enlarged");
    Check(Near(full.x,0)&&Near(full.y,0),"full icon remains centered");
    // Bounds from actual local app icon resources, also used as generic fixtures.
    auto pi=Measure(Box(64,64,5,4,59,58),64,64);
    Check(pi.valid&&Near(pi.scale,64.0/54),"padded 54px artwork fills 64px box");
    Check(Near(pi.x,0)&&pi.y>0,"asymmetric top padding centered");
    auto ag=Measure(Box(256,256,23,23,233,233),256,256);
    Check(ag.valid&&Near(ag.scale,256.0/210),"higher resolution same policy");
    auto dsh=Measure(Box(64,64,2,2,62,62),64,64);
    Check(Near(dsh.scale,64.0/60),"small padding compensated");
    auto wide=Measure(Box(64,64,4,22,60,42),64,64);
    Check(Near(wide.scale,64.0/56),"wide icon longest side controls scale");
    Check(Near(20*wide.scale/64,20.0/56),"wide icon retains aspect");
    auto tall=Measure(Box(64,64,22,4,42,60),64,64);
    Check(Near(tall.scale,wide.scale),"portrait and landscape symmetrical");
    auto rect=Measure(Box(32,64,0,6,32,58),32,64);
    Check(Near(rect.scale,64.0/52),"rectangular source uses its visible longest edge");
    Check(Near(rect.x,0)&&Near(rect.y,0),"rectangular source remains uniformly centered");
    auto cap=Measure(Box(64,64,12,12,52,52),64,64);
    Check(cap.valid&&Near(cap.scale,1.35),"bounded enlargement");
    Check(!Measure(Box(64,64,25,25,39,39),64,64).valid,"tiny placeholders retain native");
    Check(!Measure(Box(64,64,0,0,0,0),64,64).valid,"transparent capture ignored");
    Check(!Measure(Box(64,64,0,0,64,64,16),64,64).valid,"faded unsupported capture ignored");
    auto p=Box(64,64,5,5,59,59);p[3]=8;
    Check(Measure(p,64,64).left==5,"faint shadow does not define size");
    p[3]=96;Check(Measure(p,64,64).left==5,"resampling fringe below half coverage ignored");
    Check(!Measure(p,63,64).valid,"truncated or mismatched buffer rejected");
    Check(!Measure({},0,64).valid,"zero dimensions rejected");
    Check(!Measure({},2048,2048).valid,"unbounded allocations rejected");
    auto off=Measure(Box(64,64,0,3,48,51),64,64);
    Check(Near(0.375*off.scale+(1-off.scale)/2+off.x,0.5),"offset artwork visual center fixed");
    Check(Near(off.scale*48/64,1),"offset artwork longest edge fills target");
    using namespace LeanDockIconResolution;
    for(auto const& call:NativeIconSourceCalls)
        Check(SourceDips(40,40,false,SourceKind(call.returnRva))==80,"pinned cache and resource loaders have pixel headroom");
    Check(SourceDips(40,40,false,SourceKind(0x1526b8))==40,"actual layout caller keeps native dimensions");
    Check(SourceDips(40,40,false,SourceKind(0x288cd))==40,"nearby unverified call cannot opt into resolution change");
    Check(SourceDips(40,40,true,0)==40,"unload returns original source request");
    Check(SourceDips(0,0,false,0)==0&&SourceDips(-1,-1,false,0)==-1,"invalid native sizes unchanged");
    Check(SourceDips(40,20,false,0)==40,"non-square request unchanged");
    Check(SourceDips(129,129,false,0)==129,"unexpected size bounded without overflow");
    for(int dpi:{96,144,192,240})
        Check(SourceDips(40,40,false,0)*dpi/96>=40*1.35*dpi/96,"source covers optical enlargement across DPI scales");
    // MinerU: 64 px of artwork in a 256 px cell, centred or top-left, is undersized.
    Check(UndersizedArtwork(Box(256,256,96,96,160,160),256,256),"native picture centred in a large cell is undersized");
    Check(UndersizedArtwork(Box(256,256,4,4,68,68),256,256),"native picture in a large cell's corner is undersized");
    Check(!UndersizedArtwork(Box(256,256,15,15,241,241),256,256),"full artwork is not undersized");
    Check(!UndersizedArtwork(Box(256,256,64,64,192,192),256,256),"half the canvas is ordinary padding");
    Check(!UndersizedArtwork(Box(72,72,4,4,68,68),72,72),"a native size canvas never counts");
    Check(!UndersizedArtwork(std::vector<uint8_t>(256*256*4),256,256),"a blank bitmap says nothing");
    Check(!UndersizedArtwork({},256,256),"short data rejected");
    // An oversized source gets a copy at twice the display; an ordinary one does not.
    Check(LeanDockIcons::OversizedCopy(512,29.26)==59,"512 px shown at 29 px gets a copy");
    Check(LeanDockIcons::OversizedCopy(36,24)==0,"36 px shown at 24 px stays");
    Check(LeanDockIcons::OversizedCopy(24,24)==0,"a native size source stays");
    Check(LeanDockIcons::OversizedCopy(48,24)==0,"two to one stays, like the Dock");
    Check(LeanDockIcons::OversizedCopy(60,24)==48,"the sampling limit starts a copy");
    Check(LeanDockIcons::OversizedCopy(512,0)==0,"no display size, no copy");
    // Averaging keeps the art and smooths its edges.
    {   const auto solid=Box(64,64,0,0,64,64);
        const auto small=LeanDockIcons::Shrink(solid,64,64,16,16);
        Check(small.size()==16*16*4,"copy has the asked size");
        bool same=true;for(size_t i=0;i<small.size();++i)same=same&&small[i]==solid[i];
        Check(same,"a solid area keeps its exact value");
        // A disc: nearest sampling has only 0 and 255 alpha, an average has the steps between.
        std::vector<uint8_t> disc(512*512*4);
        for(int y=0;y<512;++y)for(int x=0;x<512;++x)if((x-256)*(x-256)+(y-256)*(y-256)<=200*200)disc[(size_t(y)*512+x)*4+3]=255;
        const auto smooth=LeanDockIcons::Shrink(disc,512,512,29,29);
        int partial=0;for(size_t i=3;i<smooth.size();i+=4)if(smooth[i]>0&&smooth[i]<255)++partial;
        Check(partial>=16,"rounded edges get partial coverage");
        // A fractional ratio still weighs every source pixel once.
        double before=0,after=0;
        for(size_t i=3;i<disc.size();i+=4)before+=disc[i];
        for(size_t i=3;i<smooth.size();i+=4)after+=smooth[i];
        Check(std::abs(before/(512*512)-after/(29*29))<1,"coverage is conserved");
        Check(LeanDockIcons::Shrink(disc,512,512,600,600).empty(),"never enlarges");
        Check(LeanDockIcons::Shrink(disc,512,512,0,29).empty(),"zero size rejected");
        Check(LeanDockIcons::Shrink({},512,512,29,29).empty(),"short data rejected");
    }
    // Frames of one tray icon: noise keeps the fit, a real change does not.
    auto frame=Measure(Box(64,64,5,5,59,59),64,64);
    auto nearly=Measure(Box(64,64,5,5,60,60),64,64);
    Check(LeanDockIcons::SameFit(frame,frame),"a frame matches itself");
    Check(LeanDockIcons::SameFit(frame,nearly),"one pixel of noise keeps the fit");
    Check(!LeanDockIcons::SameFit(frame,Measure(Box(64,64,12,12,52,52),64,64)),"a smaller glyph takes a new scale");
    Check(!LeanDockIcons::SameFit(frame,Measure(Box(64,64,0,0,54,54),64,64)),"an off-center glyph takes a new position");
    std::printf("icon-optics checks=%d failures=%d\n",checks,failures);
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
