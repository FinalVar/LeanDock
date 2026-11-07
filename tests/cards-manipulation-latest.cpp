#define LEANDOCK_CARDS_MANIPULATION_TEST
#include "../include/cards-manipulation-latest.h"
#include <cstdio>
#include <limits>
using namespace LeanDockCards;
namespace LeanDockCards {
// Invoke the production callbacks on our never-shown COM viewport. No pointer
// events are injected and no fixture window is activated.
struct CardManipulationTestPeer {
    struct Content:winrt::implements<Content,IDirectManipulationContent> {
        float matrix[6]{1,0,0,1,0,0};
        HRESULT __stdcall GetContentRect(RECT*) noexcept override{return E_NOTIMPL;}
        HRESULT __stdcall SetContentRect(const RECT*) noexcept override{return E_NOTIMPL;}
        HRESULT __stdcall GetViewport(REFIID,void**) noexcept override{return E_NOTIMPL;}
        HRESULT __stdcall GetTag(REFIID,void**,UINT32*) noexcept override{return E_NOTIMPL;}
        HRESULT __stdcall SetTag(IUnknown*,UINT32) noexcept override{return E_NOTIMPL;}
        HRESULT __stdcall GetOutputTransform(float*,DWORD) noexcept override{return E_NOTIMPL;}
        HRESULT __stdcall GetContentTransform(float* out,DWORD count) noexcept override{
            if(count!=6)return E_INVALIDARG;std::copy_n(matrix,6,out);return S_OK;
        }
        HRESULT __stdcall SyncContentTransform(const float*,DWORD) noexcept override{return E_NOTIMPL;}
    };
    static void Arm(CardManipulation& a){a.state->pan.Reset();a.state->contact=true;}
    static void Status(CardManipulation& a,DIRECTMANIPULATION_STATUS next){
        const auto previous=a.state->status;
        a.state->handler->OnViewportStatusChanged(a.state->viewport.get(),next,previous);
    }
    static void Frame(CardManipulation& a,float x,float y){
        auto content=winrt::make_self<Content>();content->matrix[4]=x;content->matrix[5]=y;
        a.state->handler->OnContentUpdated(a.state->viewport.get(),content.get());
    }
    static void End(CardManipulation& a){
        a.state->handler.as<IDirectManipulationInteractionEventHandler>()->OnInteraction(nullptr,DIRECTMANIPULATION_INTERACTION_END);
    }
    static void InteractionBegin(CardManipulation& a){
        a.state->handler.as<IDirectManipulationInteractionEventHandler>()->OnInteraction(nullptr,DIRECTMANIPULATION_INTERACTION_BEGIN);
    }
    static bool Identity(CardManipulation& a){
        winrt::com_ptr<IDirectManipulationContent> content;
        if(FAILED(a.state->viewport->GetPrimaryContent(__uuidof(IDirectManipulationContent),content.put_void())))return false;
        float m[6]{};if(FAILED(content->GetContentTransform(m,6)))return false;
        return std::fabs(m[0]-1.f)<.001f&&std::fabs(m[4])<.001f&&std::fabs(m[5])<.001f;
    }
    static bool ZoomIdentity(CardManipulation& a,bool useWindowOrigin){
        const auto r=a.state->rect;
        const auto hr=a.state->viewport->ZoomToRect(useWindowOrigin?float(r.left):0.f,useWindowOrigin?float(r.top):0.f,
            useWindowOrigin?float(r.right):float(r.right-r.left),useWindowOrigin?float(r.bottom):float(r.bottom-r.top),FALSE);
        winrt::com_ptr<IDirectManipulationContent> content;
        if(FAILED(hr)||FAILED(a.state->viewport->GetPrimaryContent(__uuidof(IDirectManipulationContent),content.put_void())))return false;
        float m[6]{};if(FAILED(content->GetContentTransform(m,6)))return false;
        std::printf("Real ZoomToRect origin=%s: scale=%g x=%g y=%g\n",useWindowOrigin?"window":"content",m[0],m[4],m[5]);
        return std::fabs(m[0]-1.f)<.001f&&std::fabs(m[4])<.001f&&std::fabs(m[5])<.001f;
    }
};
}
int main(){
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
    CardPan pan;
    check(!pan.Step(1,0,0).delta,"stationary does not switch");
    check(!pan.Step(1,3,-2).delta&&!pan.axis,"small jitter does not lock an axis");
    check(!pan.Step(1,5,-10).delta&&pan.axis==1,"dominant vertical axis selected");
    auto c=pan.Step(1,20,-49);check(c.delta==-120&&!c.horizontal,"upward content motion advances vertical page");
    check(!pan.Step(1,400,-50).delta&&pan.axis==1,"cross-axis drift does not add a page");
    check(!pan.Step(1,400,-97).delta,"continued vertical movement cannot switch a second page");
    check(!pan.Step(1,400,97).delta,"reversing an accepted gesture cannot switch back");
    pan.Reset();c=pan.Step(1,49,5);check(c.delta==-120&&c.horizontal,"rightward content motion maps to horizontal previous");
    pan.Reset();c=pan.Step(1,-49,5);check(c.delta==120&&c.horizontal,"leftward content motion maps to horizontal next");
    pan.Reset();c=pan.Step(1,1,49);check(c.delta==120&&!c.horizontal,"downward content motion maps to vertical previous");
    pan.Reset();check(!pan.Step(1,47,0).delta,"sub-threshold pan accumulates");
    check(pan.Step(1,48,0).delta==-120,"threshold is inclusive");
    check(!pan.Step(1,48,0).delta,"same transform is not delivered twice");
    pan.Reset();check(!pan.Step(1.05f,200,0).delta&&pan.pinch,"pinch suppresses paging");
    check(!pan.Step(1,300,0).delta,"pinch classification lasts through sequence");
    pan.Reset();check(pan.Step(1,49,0).delta==-120,"new gesture clears pinch");
    pan.Reset();check(!pan.Step(0,100,0).delta,"invalid scale ignored");
    pan.Reset();check(!pan.Step(1,std::numeric_limits<float>::quiet_NaN(),0).delta,"invalid translation ignored");
    pan.Reset();check(pan.Step(1,10000,0).delta==-120,"large jump produces at most one page");
    check(!pan.Step(1,10000,0).delta,"discarded overshoot does not coast");
    check(!pan.Step(1,20000,0).delta,"more distance after a large jump remains latched");
    check(!pan.Step(1,-20000,0).delta,"large reversal after switching remains latched");
    pan.Reset();int pages=0;
    for(int i=0;i<300;++i){const float x=i<150?float(i):float(300-i);if(pan.Step(1,x,0).delta)++pages;}
    check(pages==1,"one continuous forward-and-back frame stream switches exactly one page");
    pan.Reset();check(pan.Step(1,49,0).delta==-120,"lifting and beginning a new gesture rearms paging");
    pan.Reset();pan.Step(1,45,0);check(!pan.Step(1,20,0).delta,"reversal cancels pending forward movement");
    check(pan.Step(1,-49,0).delta==120,"reversal can pass opposite threshold");

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"LeanDockHiddenDMFixture";
    check(RegisterClassW(&cls)!=0,"register fixture");
    HWND host=CreateWindowExW(0,cls.lpszClassName,L"",WS_POPUP,0,0,1000,1000,nullptr,nullptr,cls.hInstance,nullptr);
    HWND window=CreateWindowExW(0,cls.lpszClassName,L"",WS_CHILD,0,0,1000,1000,host,nullptr,cls.hInstance,nullptr);
    check(window&&!IsWindowVisible(window),"child input-window fixture never displayed");
    int frameStarts=0,frameStops=0,errors=0,updates=0;
    auto frames=[&](bool active){active?++frameStarts:++frameStops;};
    auto trace=[&](unsigned i,intptr_t value){if(i==11&&FAILED(HRESULT(value)))++errors;if(i==13)++updates;};
    CardManipulation a,b;RECT rect{10,10,114,54};
    const auto hr=a.Init(window,rect,[](int,bool){},frames,trace);
    std::printf("Actual Windows DirectManipulation Init HRESULT=0x%08lx\n",static_cast<unsigned long>(hr));
    check(SUCCEEDED(hr),"real COM manager and viewport initialize");
    check(SUCCEEDED(a.Init(window,rect,[](int,bool){},frames,trace)),"initialization is idempotent");
    check(SUCCEEDED(b.Init(window,rect,[](int,bool){},frames,trace)),"independent manager can share target window");
    a.Update();b.Update();check(updates==2&&!errors,"real update API succeeds");
    check(frameStarts==0,"idle state requests no animation frames");
    a.Cancel();a.Stop();a.Stop();check(frameStops>=1,"stop and duplicate stop complete");
    b.Update();check(updates==3&&!errors,"second manager survives first shutdown");
    check(SUCCEEDED(a.Init(window,rect,[](int,bool){},frames,trace)),"manager can be recreated");
    a.Stop();int callbackPages=0;
    RECT offsetRect{600,100,704,144};
    check(SUCCEEDED(a.Init(window,offsetRect,[&](int,bool){++callbackPages;},frames,trace)),"offset viewport initializes for reset regression");
    using Peer=CardManipulationTestPeer;
    check(!Peer::ZoomIdentity(a,true),"window-origin ZoomToRect is not an identity content transform");
    check(Peer::ZoomIdentity(a,false),"content-origin ZoomToRect restores identity even for offset viewport");
    Peer::Arm(a);Peer::Status(a,DIRECTMANIPULATION_RUNNING);Peer::Frame(a,80,0);Peer::Frame(a,300,0);
    check(callbackPages==1,"production content callback latches one page during a long gesture");
    Peer::Status(a,DIRECTMANIPULATION_READY);
    check(Peer::Identity(a),"production READY reset returns content to the correct origin");
    const int startsBeforeReset=frameStarts;Peer::InteractionBegin(a);
    check(frameStarts==startsBeforeReset,"reset interaction cannot rearm idle frame subscription");
    // ZoomToRect may deliver its own RUNNING/content/READY after returning.
    Peer::Status(a,DIRECTMANIPULATION_RUNNING);Peer::Frame(a,-600,-100);Peer::Frame(a,0,0);
    Peer::Status(a,DIRECTMANIPULATION_READY);Peer::End(a);
    check(callbackPages==1,"asynchronous release/reset callbacks cannot produce a second page");
    Peer::Arm(a);Peer::Status(a,DIRECTMANIPULATION_RUNNING);Peer::Frame(a,0,60);
    check(callbackPages==2,"a new contact rearms the production callback after release");
    Peer::End(a);Peer::Frame(a,0,300);
    check(callbackPages==2,"interaction END closes paging before READY arrives");
    Peer::Status(a,DIRECTMANIPULATION_READY);
    Peer::Arm(a);Peer::Status(a,DIRECTMANIPULATION_RUNNING);Peer::Frame(a,20,0);
    Peer::Status(a,DIRECTMANIPULATION_READY);Peer::Status(a,DIRECTMANIPULATION_RUNNING);Peer::Frame(a,-600,-100);
    Peer::Status(a,DIRECTMANIPULATION_READY);
    check(callbackPages==2,"sub-threshold gesture release cannot turn into a page");
    Peer::Arm(a);Peer::Status(a,DIRECTMANIPULATION_RUNNING);Peer::Frame(a,-60,0);
    a.Cancel();Peer::Status(a,DIRECTMANIPULATION_RUNNING);Peer::Frame(a,600,100);
    check(callbackPages==3,"cancel rejects late movement callbacks");
    a.Stop();b.Stop();DestroyWindow(host);UnregisterClassW(cls.lpszClassName,cls.hInstance);
    winrt::uninit_apartment();
    std::printf("%d checks, %d failures; real COM lifecycle on hidden window, no hardware gesture acceptance\n",checks,failures);
    return failures?1:0;
}
