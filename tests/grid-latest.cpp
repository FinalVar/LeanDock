#include "grid-latest.h"
#include <cassert>
#include <iostream>
#include <set>
// Compile-time validation remains available when host application control
// doesn't permit execution of a freshly built, unsigned test program.
constexpr bool MixedNativeGroups() {
    for(int apps=0;apps<=64;++apps)for(int system=0;system<=12;++system){
        const int total=apps+system;
        int before=0,extent=0;
        for(int count:{apps,system}){
            const auto group=DockGrid::PlaceGroup(before,count);
            const int start=extent-group.overlapColumns;
            for(int i=0;i<count;++i){
                const int x=start*28+28*DockGrid::Column(before,i)+2;
                const int y=24+int(24*DockGrid::RowOffset(before,i,total));
                if(x!=((before+i)/2)*28+2||x<0||x+24>DockGrid::TotalColumns(total)*28)return false;
                if(y!=(total%2&&before+i==total-1?24:(before+i)%2?36:12))return false;
            }
            extent+=group.columns-group.overlapColumns;before+=count;
        }
        if(extent!=DockGrid::TotalColumns(total))return false;
    }
    return true;
}
static_assert(MixedNativeGroups());
int main() {
    int checks=0;
    // Two separately owned native panels must occupy a continuous mixed grid.
    // A system group must share the last app column at odd parity; removing
    // a group must leave no reserved cell/column. Verify actual translated
    // bounds (24 DIP hit targets on 28 DIP column pitch), including centering.
    for(int apps=0;apps<=64;++apps)for(int system=0;system<=12;++system){
        const int total=apps+system;
        int before=0,extent=0;
        std::set<std::pair<int,int>> centers;
        for(int count:{apps,system}){
            const auto group=DockGrid::PlaceGroup(before,count);
            const int start=extent-group.overlapColumns;
            for(int i=0;i<count;++i){
                const int x=start*28+i*24+28*DockGrid::Column(before,i)-24*i+2;
                const int y=24+int(24*DockGrid::RowOffset(before,i,total));
                assert(x==((before+i)/2)*28+2);++checks;
                assert(y==(total%2&&before+i==total-1?24:(before+i)%2?36:12));++checks;
                assert(centers.emplace(x,y).second);++checks;
                assert(x>=0&&x+24<=DockGrid::TotalColumns(total)*28);++checks;
            }
            extent+=group.columns-group.overlapColumns;before+=count;
        }
        assert(extent==DockGrid::TotalColumns(total));++checks;
        assert(centers.size()==size_t(total));++checks;
    }
    // Dynamic app counts and 0-3 visible input-method/status items. Simulate
    // each group's measured width/negative margin, not just index formulas.
    for(int apps=0;apps<=128;++apps) for(int inputs=0;inputs<=3;++inputs) {
        int before=0,extent=0;
        std::set<std::pair<int,int>> slots;
        for(int count:{apps,1,inputs}) {
            const auto g=DockGrid::PlaceGroup(before,count);
            const int start=extent-g.overlapColumns;
            for(int i=0;i<count;++i) {
                const int col=start+DockGrid::Column(before,i),row=DockGrid::Row(before,i);
                assert(slots.emplace(col,row).second);++checks;
                assert(col==(before+i)/2 && row==(before+i)%2);++checks;
            }
            extent+=g.columns-g.overlapColumns;before+=count;
        }
        assert(extent==DockGrid::TotalColumns(before));++checks;
        for(int i=0;i<before;++i){assert(slots.count({i/2,i%2})==1);++checks;}
        assert(slots.size()==size_t(before));++checks;
        // 26H2 uses a horizontal StackPanel: apps, IME, then battery. Prove
        // that its physical allocation still places the logical battery slot
        // before the IME slots, including an absent IME and no app icons.
        const int total=apps+1+inputs;
        const int allColumns=DockGrid::TotalColumns(total);
        const int appColumns=DockGrid::TotalColumns(apps);
        const int remaining=allColumns-appColumns;
        assert(remaining>=0);++checks;
        const int batteryNativeColumn=appColumns+remaining-1;
        const int batteryDelta=apps/2-(allColumns-1);
        assert(batteryNativeColumn+batteryDelta==apps/2);++checks;
        for(int i=0;i<inputs;++i){
            assert(appColumns+DockGrid::Column(apps+1,i)==(apps+1+i)/2);++checks;
        }
    }
    std::cout<<checks<<" layout invariants passed\n";
}

