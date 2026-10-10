#include "cards-network-latest.h"
#include "cards-preferences-latest.h"
#include <cstdio>
#include <limits>
int wmain(int argc,wchar_t** argv){
    using namespace LeanDockCards;int count=0,failures=0;
    auto check=[&](bool ok,const char* name){++count;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
    PageOrder order;check(order.pages.front()==Page::Clock,"first use defaults to clock");
    check(order.Step(Page::Clock,1)==Page::Power&&order.Step(Page::Clock,-1)==Page::Network,"default order both directions");
    for(auto key:{L"",L"clock,",L",clock",L"clock,,media",L"clock,clock",L"CLOCK",L"metrics,nonsense",L"    "})
        check(!ParseOrder(key),"reject invalid order");
    check(!ParseOrder(std::wstring(129,L'x')),"reject oversized preference");
    auto spaced=ParseOrder(L" clock , metrics ");check(spaced&&spaced->Text()==L"clock,metrics","normalize supported whitespace");
    for(unsigned mask=1;mask<32;++mask){PageOrder subset;subset.pages.clear();
        for(unsigned i=0;i<5;++i)if(mask&(1u<<i))subset.pages.push_back(DefaultOrder[i]);
        auto parsed=ParseOrder(subset.Text());check(parsed&&parsed->pages==subset.pages,"all enabled subsets roundtrip");
        for(auto page:DefaultOrder){auto next=subset.Step(page,1);check(subset.Has(next),"disabled pages cannot be selected by wheel");
            check(subset.Has(subset.Select(page)),"removed last page falls back to enabled page");
            if(subset.Has(page))check(subset.Step(next,-1)==page,"backward reverses forward");}
    }
    NetworkSampler sample;
    check(!sample.Sample(NetworkSnapshot{{1,1000,2000}},1000)&&!sample.value,"first network sample is baseline");
    check(!sample.Sample(NetworkSnapshot{{1,1100,2100}},1100)&&!sample.value,"reject early tick");
    check(sample.Sample(NetworkSnapshot{{1,1400,2200}},1200)&&sample.value->received==2000&&sample.value->sent==1000,"real elapsed rate");
    check(sample.IntervalMs()==1000,"steady sampling one second");
    sample.Pause();check(!sample.baseline&&!sample.value,"hidden page clears rate and baseline");
    check(!sample.Sample(NetworkSnapshot{{1,999000,888000}},10000),"hidden traffic never mixed into rate");
    sample.Sample(NetworkSnapshot{{1,999500,888250}},10500);check(sample.value&&sample.value->received==1000&&sample.value->sent==500,"resume real delta");
    sample.Sample(NetworkSnapshot{{1,10,10}},11000);check(!sample.value,"counter reset cannot underflow");
    sample.Sample(NetworkSnapshot{{1,510,1010},{2,500000,400000}},11500);check(!sample.value,"new adapter requires baseline");
    sample.Sample(NetworkSnapshot{{1,1010,1510},{2,500500,400500}},12000);check(sample.value&&sample.value->received==2000&&sample.value->sent==2000,"sum distinct stable adapters");
    sample.Sample(NetworkSnapshot{},13000);check(sample.value&&sample.value->received==0&&!sample.connected,"disconnect clears last rate");
    sample.Sample(NetworkSnapshot{{7,1000000,1000000}},14000);check(!sample.value,"reconnect does not expose lifetime counter");
    sample.Sample(NetworkSnapshot{{7,1000100,1000100}},13900);check(!sample.value,"time rollback resets");
    sample.Sample(NetworkSnapshot{{7,9999999,9999999}},99999);check(!sample.value,"long suspend resets baseline");
    sample.Sample({},100999);check(!sample.value&&!sample.baseline,"failed query is unknown not zero");
    check(RateText({})==L"—"&&RateText(-1)==L"—","unknown speed formatting");
    check(RateText(std::numeric_limits<double>::infinity())==L"—","nonfinite speed rejected");
    check(RateText(0)==L"0 B/s"&&RateText(1250)==L"1.2 KB/s"&&RateText(12500000)==L"12.5 MB/s","decimal byte rates");
    check(RateText(1e30)==L">9999 GB/s","bounded display");
    const auto upload=FormatRate(129000),download=FormatRate(3300),unknown=FormatRate({});
    check(upload.number==L"129"&&upload.unit==L"KB"&&download.number==L"3.3"&&download.unit==L"KB","separate aligned values and compact units");
    check(unknown.number==L"—"&&unknown.unit.empty()&&unknown.PerSecondText()==L"—","unknown rate has no stale unit");
    check(FormatRate(0).unit==L"B"&&FormatRate(12500000).unit==L"MB"&&FormatRate(1e30).number==L">9999","compact unit transitions and bounds");
    check(FormatRate(500).CompactText()==L"500 B"&&FormatRate(1500).CompactText()==L"1.5 KB","number and variable-length unit use one space");
    check(FormatRate(12500000).CompactText()==L"12.5 MB"&&FormatRate(1e30).CompactText()==L">9999 GB","joined compact labels keep larger units and limits");
    check(FormatRate({}).CompactText()==L"—"&&FormatRate(0).CompactText()==L"0 B","unknown and zero compact labels have no padding");
    if(argc!=2)return 2;std::wstring root=argv[1];
    check(CreateDirectoryW(root.c_str(),nullptr)!=FALSE,"isolated nonexisting test root");
    Page selected=Page::Power;PageOrder saved;
    check(ReadPreferences(root,saved,selected)&&selected==Page::Clock,"missing file defaults without write");
    check(GetFileAttributesW(PreferencesPath(root).c_str())==INVALID_FILE_ATTRIBUTES,"reading defaults creates no file");
    saved=*ParseOrder(L"power,clock,network");check(SaveOrder(root,saved),"save order");
    check(SaveSelection(root,Page::Network),"remember selection");
    PageOrder reread;check(ReadPreferences(root,reread,selected)&&reread.Text()==saved.Text()&&selected==Page::Network,"read persisted order and page");
    check(SaveOrder(root,*ParseOrder(L"clock,power")),"disable selected page");
    check(ReadPreferences(root,reread,selected)&&selected==Page::Clock,"disabled saved page falls back");
    check(SaveSelection(root,Page::Power)&&ReadPreferences(root,reread,selected)&&reread.Text()==L"clock,power","selection preserves order");
    WritePrivateProfileStringW(L"cards",L"Order",L"clock,unknown",PreferencesPath(root).c_str());
    auto before=reread.Text();check(!ReadPreferences(root,reread,selected)&&reread.Text()==before&&selected==Page::Power,"invalid reload preserves last good state");
    PageOrder empty;empty.pages.clear();check(!SaveOrder(root,empty),"prevent all-disabled saved state");
    check(!SaveSelection(root,Page::Count),"invalid selected page cannot be written");
    LARGE_INTEGER f{},a{},b{};QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
    auto real=ReadNetwork();QueryPerformanceCounter(&b);
    check(bool(real),"current physical counter API succeeds");
    std::printf("Live network query: %zu active physical interfaces, %.3f ms (no traffic generated)\n",real?real->size():0,1000.0*(b.QuadPart-a.QuadPart)/f.QuadPart);
    std::printf("Card preferences/network: %d checks, %d failures\n",count,failures);return failures?1:0;
}
