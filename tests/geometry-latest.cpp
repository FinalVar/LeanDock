#include "../include/dock-geometry-latest.h"
#include <cstdio>
int main() {
    using namespace LeanDockGeometry;
    unsigned checks=0,failed=0;
    auto check=[&](bool ok){++checks;if(!ok)++failed;};
    for(LONG origin:{-2560L,0L,2560L})for(int dpi:{96,120,144,192}) {
        RECT mon{origin,-200,origin+2560,1400};
        RECT dock{origin+600,1400-MulDiv(64,dpi,96),origin+1960,1400};
        check(!InSpan(dock,dock.left-1));check(InSpan(dock,dock.left));
        check(InSpan(dock,dock.right-1));check(!InSpan(dock,dock.right));
        auto p=HostPosition(dock,mon,800,600,dpi,false);
        check(p.x==origin+880 && p.y+600-MulDiv(12,dpi,96)==dock.top-MulDiv(8,dpi,96));
        p=HostPosition(dock,mon,576,600,dpi,true);
        check(p.x+576-MulDiv(12,dpi,96)==dock.right);
        p=HostPosition(dock,mon,2560,1400,dpi,false);
        check(p.x==mon.left);
    }
    check(!ValidSpan(0,0,64,2560));check(!ValidSpan(-1,1200,64,2560));
    check(!ValidSpan(600,2561,64,2560));check(!ValidSpan(600,1900,1000,2560));
    check(ValidSpan(600,1900,96,2560));
    printf("%u geometry checks, %u failures\n",checks,failed);return failed?1:0;
}
