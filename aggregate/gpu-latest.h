#pragma once
#include "metrics-policy-latest.h"
#include <windows.h>
#include <dxgi.h>
#include <cstddef>
#include <map>
#include <string>
#include <vector>
#include <winrt/base.h>

// Public Windows SDK user-mode thunks (shared/d3dkmthk.h, exported by gdi32);
// the same kernel node counters behind Task Manager and the PDH "GPU Engine"
// set, without PDH's ~0.3 s query construction. MinGW ships no d3dkmthk.h, so
// only the fields read here are declared and the SDK 10.0.26100 layout is
// asserted. Read-only queries; no escapes, driver-private data or writes.
namespace Capsule::Kmt {
using Handle=UINT;
struct AdapterInfo {Handle adapter;LUID luid;ULONG sources;BOOL precisePresentRegions;};
struct EnumAdapters2 {ULONG count;AdapterInfo* adapters;};
struct QueryAdapterInfo {Handle adapter;int type;void* data;UINT size;};
struct CloseAdapter {Handle adapter;};
#pragma pack(push,1) // d3dkmdt.h declares node metadata inside pack(1).
struct NodeMetadata {UINT ordinalAndAdapter;int engineType;wchar_t name[32];UINT flags;BOOLEAN gpuMmu,ioMmu;};
#pragma pack(pop)
struct QueryStatistics {int type;LUID luid;uint64_t process;alignas(8) BYTE result[776];ULONG node;};
static_assert(sizeof(AdapterInfo)==20&&sizeof(EnumAdapters2)==16&&sizeof(QueryAdapterInfo)==24&&sizeof(CloseAdapter)==4);
static_assert(sizeof(NodeMetadata)==78&&offsetof(NodeMetadata,engineType)==4);
static_assert(sizeof(QueryStatistics)==808&&offsetof(QueryStatistics,result)==24&&offsetof(QueryStatistics,node)==800);
inline constexpr int AdapterTypeInfo=15,NodeMetadataInfo=25; // KMTQAITYPE_ADAPTERTYPE, _NODEMETADATA
inline constexpr int AdapterStatistics=0,NodeStatistics=5;   // D3DKMT_QUERYSTATISTICS_ADAPTER, _NODE
inline constexpr int Engine3D=1;                              // DXGK_ENGINE_TYPE_3D
struct Api {
    using Enum=LONG(APIENTRY*)(EnumAdapters2*);
    using Info=LONG(APIENTRY*)(QueryAdapterInfo const*);
    using Close=LONG(APIENTRY*)(CloseAdapter const*);
    using Statistics=LONG(APIENTRY*)(QueryStatistics const*);
    Enum enumerate{};Info info{};Close close{};Statistics statistics{};
    Api(){
        if(auto gdi=LoadLibraryExW(L"gdi32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)){
            enumerate=reinterpret_cast<Enum>(GetProcAddress(gdi,"D3DKMTEnumAdapters2"));
            info=reinterpret_cast<Info>(GetProcAddress(gdi,"D3DKMTQueryAdapterInfo"));
            close=reinterpret_cast<Close>(GetProcAddress(gdi,"D3DKMTCloseAdapter"));
            statistics=reinterpret_cast<Statistics>(GetProcAddress(gdi,"D3DKMTQueryStatistics"));
        }
    }
    bool Ready()const{return enumerate&&info&&close&&statistics;}
};
inline Api const& Thunks(){static const Api api;return api;}
inline uint64_t Key(LUID luid){return (uint64_t(uint32_t(luid.HighPart))<<32)|luid.LowPart;}
struct Adapter {uint64_t luid{};std::wstring name;std::vector<ULONG> nodes;};
// DXGI supplies names and drops software/remote adapters; the kernel adapter
// type then drops display-only/indirect aliases (no RenderSupported bit) that
// mirror another adapter's engines. Only nodes the driver labels 3D count.
inline std::vector<Adapter> RenderAdapters(){
    std::vector<Adapter> result;auto const& api=Thunks();if(!api.Ready())return result;
    winrt::com_ptr<IDXGIFactory1> factory;
    if(FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),factory.put_void())))return result;
    std::vector<std::pair<uint64_t,std::wstring>> named;
    for(UINT i=0;i<16;++i){
        winrt::com_ptr<IDXGIAdapter1> adapter;if(FAILED(factory->EnumAdapters1(i,adapter.put())))break;
        DXGI_ADAPTER_DESC1 desc{};
        if(FAILED(adapter->GetDesc1(&desc))||(desc.Flags&(DXGI_ADAPTER_FLAG_SOFTWARE|DXGI_ADAPTER_FLAG_REMOTE)))continue;
        named.emplace_back(Key(desc.AdapterLuid),desc.Description);
    }
    EnumAdapters2 list{};if(api.enumerate(&list)||!list.count||list.count>64)return result;
    std::vector<AdapterInfo> infos(list.count);list.adapters=infos.data();
    if(api.enumerate(&list))return result;
    infos.resize(std::min<size_t>(infos.size(),list.count));
    std::map<uint64_t,std::vector<ULONG>> engines;
    for(auto const& info:infos){
        UINT type=0;QueryAdapterInfo query{info.adapter,AdapterTypeInfo,&type,sizeof(type)};
        QueryStatistics adapter{};adapter.type=AdapterStatistics;adapter.luid=info.luid;
        if(!api.info(&query)&&GpuRenders(type)&&!api.statistics(&adapter)){
            ULONG count=0;memcpy(&count,adapter.result+4,sizeof(count));
            std::vector<ULONG> nodes;
            for(ULONG n=0;n<count&&n<64;++n){
                NodeMetadata meta{};meta.ordinalAndAdapter=n;
                QueryAdapterInfo q{info.adapter,NodeMetadataInfo,&meta,sizeof(meta)};
                if(!api.info(&q)&&meta.engineType==Engine3D)nodes.push_back(n);
            }
            if(!nodes.empty())engines[Key(info.luid)]=std::move(nodes);
        }
        CloseAdapter close{info.adapter};api.close(&close);
    }
    for(auto& [luid,name]:named)if(auto e=engines.find(luid);e!=engines.end())result.push_back({luid,std::move(name),e->second});
    return result;
}
inline bool NodeRunning(uint64_t luid,ULONG node,int64_t& running){
    auto const& api=Thunks();if(!api.Ready())return false;
    QueryStatistics q{};q.type=NodeStatistics;q.luid.LowPart=DWORD(luid);q.luid.HighPart=LONG(luid>>32);q.node=node;
    if(api.statistics(&q))return false;
    memcpy(&running,q.result,sizeof(running)); // GlobalInformation.RunningTime, 100 ns
    return true;
}
}
