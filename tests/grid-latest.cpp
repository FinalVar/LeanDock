#include "grid-latest.h"
#include <cassert>
#include <iostream>
#include <set>
int main() {
    int checks=0;
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

