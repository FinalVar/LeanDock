#pragma once
// Locally written: the D2D gamma transfer effect for composition effect
// brushes, in the same IGraphicsEffectD2D1Interop pattern as effects-latest.h
// (left unchanged, it is extracted upstream code). out=amplitude*c^exponent+
// offset per colour channel lifts a dark backdrop more than a bright one, so
// the glass keeps the desktop's colour while text stays legible.
#include "effects-latest.h"

struct GammaTransferEffect : winrt::implements<GammaTransferEffect,wge::IGraphicsEffect,wge::IGraphicsEffectSource,awge::IGraphicsEffectD2D1Interop>
{
    wge::IGraphicsEffectSource Source{nullptr};
    float Amplitude=1,Exponent=1,Offset=0; // the same curve on red, green and blue; alpha untouched
    winrt::hstring name=L"Gamma";

    HRESULT STDMETHODCALLTYPE GetEffectId(GUID* id) noexcept override{
        if(!id)return E_INVALIDARG;*id=CLSID_D2D1GammaTransfer;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetNamedPropertyMapping(LPCWSTR,UINT*,awge::GRAPHICS_EFFECT_PROPERTY_MAPPING*) noexcept override{
        return E_INVALIDARG; // nothing is animated by name
    }
    HRESULT STDMETHODCALLTYPE GetPropertyCount(UINT* count) noexcept override{
        if(!count)return E_INVALIDARG;*count=D2D1_GAMMATRANSFER_PROP_CLAMP_OUTPUT+1;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetProperty(UINT index,winrt::impl::abi_t<wf::IPropertyValue>** value) noexcept override try{
        if(!value)return E_INVALIDARG;
        if(index>D2D1_GAMMATRANSFER_PROP_CLAMP_OUTPUT)return E_BOUNDS;
        wf::IInspectable v{nullptr};
        if(index==D2D1_GAMMATRANSFER_PROP_CLAMP_OUTPUT)v=wf::PropertyValue::CreateBoolean(true);
        else{
            // Four properties per channel: amplitude, exponent, offset, disable.
            const UINT channel=index/4,field=index%4;const bool alpha=channel==3;
            if(field==3)v=wf::PropertyValue::CreateBoolean(alpha);
            else v=wf::PropertyValue::CreateSingle(alpha?(field==0?1.f:field==1?1.f:0.f):(field==0?Amplitude:field==1?Exponent:Offset));
        }
        *value=v.as<winrt::impl::abi_t<wf::IPropertyValue>>().detach();return S_OK;
    }catch(...){return winrt::to_hresult();}
    HRESULT STDMETHODCALLTYPE GetSource(UINT index,awge::IGraphicsEffectSource** source) noexcept override{
        if(!source)return E_INVALIDARG;if(index||!Source)return E_BOUNDS;
        winrt::copy_to_abi(Source,*reinterpret_cast<void**>(source));return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetSourceCount(UINT* count) noexcept override{if(!count)return E_INVALIDARG;*count=1;return S_OK;}
    winrt::hstring Name(){return name;}
    void Name(winrt::hstring value){name=value;}
};
