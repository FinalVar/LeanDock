#pragma once
// DirectManipulation setup/reset/interaction lifecycle adapted from Chromium.
// Copyright 2015, 2019 The Chromium Authors. BSD-3-Clause; see NOTICE.md and
// upstream/cards-input/LICENSE-Chromium-BSD.txt. Card paging is local policy.
#include <windows.h>
#include <directmanipulation.h>
#include <winrt/base.h>
#include <functional>
#include <memory>
#include "cards-pan-latest.h"

namespace LeanDockCards {
class CardManipulation {
#if defined(LEANDOCK_CARDS_MANIPULATION_TEST)
    friend struct CardManipulationTestPeer;
#endif
    struct State {
        winrt::com_ptr<IDirectManipulationManager> manager;
        winrt::com_ptr<IDirectManipulationUpdateManager> updates;
        winrt::com_ptr<IDirectManipulationViewport> viewport;
        winrt::com_ptr<IDirectManipulationViewportEventHandler> handler;
        HWND window{};DWORD thread=GetCurrentThreadId(),cookie{};RECT rect{};
        bool registered=false,activated=false,closed=false,resetting=false,contact=false;
        DIRECTMANIPULATION_STATUS status=DIRECTMANIPULATION_BUILDING;
        CardPan pan;float dpi=1;
        std::function<void(int,bool)> scroll;
        std::function<void(bool)> frames;
        std::function<void(unsigned,intptr_t)> trace;
        void Close() noexcept {
            if(closed)return;closed=true;
            if(frames)frames(false);
            if(viewport){
                viewport->ReleaseAllContacts();viewport->Stop();
                if(registered)viewport->RemoveEventHandler(cookie);
                viewport->Disable();viewport->Abandon();
            }
            if(manager&&activated)manager->Deactivate(window);
            handler=nullptr;viewport=nullptr;updates=nullptr;manager=nullptr;
        }
        ~State(){Close();}
    };
    struct Handler:winrt::implements<Handler,IDirectManipulationViewportEventHandler,IDirectManipulationInteractionEventHandler> {
        std::weak_ptr<State> weak;
        explicit Handler(std::shared_ptr<State> const& s):weak(s){}
        HRESULT __stdcall OnViewportStatusChanged(IDirectManipulationViewport* viewport,DIRECTMANIPULATION_STATUS current,DIRECTMANIPULATION_STATUS previous) noexcept override {
            auto keepAlive=get_strong();
            auto s=weak.lock();if(!s||GetCurrentThreadId()!=s->thread||s->closed)return S_OK;
            s->status=current;if(s->trace)s->trace(10,current);
            if(s->resetting)return S_OK;
            if(current==DIRECTMANIPULATION_READY&&previous!=current){
                const bool displaced=s->pan.lastX!=0||s->pan.lastY!=0||s->pan.pinch;
                // Close input before ZoomToRect. Its own RUNNING/content/READY
                // callbacks can arrive after it returns, outside resetting.
                // Only a newly admitted SetContact can reopen the paging gate.
                s->contact=false;s->pan.Reset();
                if(displaced){
                    s->resetting=true;
                    // The target uses primary-content coordinates, not the
                    // viewport's offset inside the taskbar input window.
                    const auto r=s->rect;auto hr=viewport->ZoomToRect(0,0,float(r.right-r.left),float(r.bottom-r.top),FALSE);
                    s->resetting=false;if(FAILED(hr)&&s->trace)s->trace(11,hr);
                }
                if(s->frames)s->frames(false);
            }
            return S_OK;
        }
        HRESULT __stdcall OnViewportUpdated(IDirectManipulationViewport*) noexcept override {return S_OK;}
        HRESULT __stdcall OnContentUpdated(IDirectManipulationViewport*,IDirectManipulationContent* content) noexcept override {
            auto keepAlive=get_strong();
            auto s=weak.lock();if(!s||GetCurrentThreadId()!=s->thread||s->closed||s->resetting)return S_OK;
            float transform[6]{};auto hr=content->GetContentTransform(transform,6);
            if(FAILED(hr)){if(s->trace)s->trace(11,hr);return hr;}
            if(s->trace)s->trace(9,1);
            if(!s->contact||s->status!=DIRECTMANIPULATION_RUNNING)return S_OK;
            const auto command=s->pan.Step(transform[0],transform[4]/s->dpi,transform[5]/s->dpi);
            if(command.delta&&s->scroll)s->scroll(command.delta,command.horizontal);
            return S_OK;
        }
        HRESULT __stdcall OnInteraction(IDirectManipulationViewport2*,DIRECTMANIPULATION_INTERACTION_TYPE interaction) noexcept override {
            auto keepAlive=get_strong();
            auto s=weak.lock();if(!s||GetCurrentThreadId()!=s->thread||s->closed)return S_OK;
            if(interaction==DIRECTMANIPULATION_INTERACTION_BEGIN){if(s->contact&&s->frames)s->frames(true);}
            else if(interaction==DIRECTMANIPULATION_INTERACTION_END){s->contact=false;if(s->frames)s->frames(false);}
            return S_OK;
        }
    };
    std::shared_ptr<State> state;
public:
    using Scroll=std::function<void(int,bool)>;using Frames=std::function<void(bool)>;
    using Trace=std::function<void(unsigned,intptr_t)>;
    ~CardManipulation(){Stop();}
    HRESULT Init(HWND window,RECT rect,Scroll scroll,Frames frames,Trace trace) noexcept {
        if(state&&state->window==window&&!state->closed)return S_OK;
        Stop();
        try {
            auto s=std::make_shared<State>();s->window=window;s->rect=rect;
            s->scroll=std::move(scroll);s->frames=std::move(frames);s->trace=std::move(trace);
            auto ok=[](HRESULT hr){winrt::check_hresult(hr);};
            ok(CoCreateInstance(CLSID_DirectManipulationManager,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IDirectManipulationManager),s->manager.put_void()));
            ok(s->manager->GetUpdateManager(__uuidof(IDirectManipulationUpdateManager),s->updates.put_void()));
            ok(s->manager->CreateViewport(nullptr,window,__uuidof(IDirectManipulationViewport),s->viewport.put_void()));
            const auto configuration=DIRECTMANIPULATION_CONFIGURATION_INTERACTION|
                DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X|DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y|
                DIRECTMANIPULATION_CONFIGURATION_RAILS_X|DIRECTMANIPULATION_CONFIGURATION_RAILS_Y|
                DIRECTMANIPULATION_CONFIGURATION_SCALING;
            ok(s->viewport->ActivateConfiguration(configuration));
            ok(s->viewport->SetViewportOptions(DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE));
            ok(s->viewport->SetViewportRect(&rect));
            s->handler=winrt::make_self<Handler>(s).as<IDirectManipulationViewportEventHandler>();
            ok(s->viewport->AddEventHandler(window,s->handler.get(),&s->cookie));s->registered=true;
            ok(s->manager->Activate(window));s->activated=true;
            ok(s->viewport->Enable());ok(s->updates->Update(nullptr));
            state=std::move(s);return S_OK;
        }catch(...){return winrt::to_hresult();}
    }
    HRESULT Begin(UINT32 pointer,RECT rect,float dpi) noexcept {
        auto s=state;if(!s||s->closed)return E_UNEXPECTED;
        if(dpi<=0)return E_INVALIDARG;
        if(!EqualRect(&rect,&s->rect)){
            s->contact=false;s->pan.Reset();
            auto hr=s->viewport->Stop();if(FAILED(hr))return hr;
            hr=s->viewport->SetViewportRect(&rect);if(FAILED(hr))return hr;
            s->rect=rect;s->pan.Reset();s->contact=false;
        }
        s->dpi=dpi;if(!s->contact)s->pan.Reset();
        const auto hr=s->viewport->SetContact(pointer);
        if(SUCCEEDED(hr)){s->contact=true;if(s->frames)s->frames(true);}
        return hr;
    }
    void Update() noexcept {
        auto s=state;if(!s||s->closed)return;
        auto updates=s->updates;const auto hr=updates->Update(nullptr);
        if(s->trace)s->trace(13,1);
        if(FAILED(hr)){if(s->trace)s->trace(11,hr);s->Close();}
    }
    void Cancel() noexcept {
        auto s=state;if(!s||s->closed)return;
        s->contact=false;s->pan.Reset();
        s->viewport->ReleaseAllContacts();s->viewport->Stop();
        if(s->frames)s->frames(false);
    }
    void Stop() noexcept {auto old=std::exchange(state,nullptr);if(old)old->Close();}
};
}
