#include "../core/latest.h"
#include "../actions/latest.h"
#include <stdio.h>
using namespace nhc;
static int checks, failed;
static void Check(bool ok, const char* name) { ++checks; if (!ok) { ++failed; printf("FAIL %s\n", name); } }
int main() {
    Rect displays[] = {{0,0,1920,1080}, {-1280,-200,0,824}, {1920,300,4480,1740}};
    Config cfg; cfg.delay=250; cfg.cooldown=500;
    for (auto& a : cfg.actions) a = TaskView;
    for (int m = 0; m < 3; ++m) {
        Rect r = displays[m];
        Point points[] = {{r.left,r.top},{r.right-1,r.top},{r.left,r.bottom-1},{r.right-1,r.bottom-1}};
        for (int c=0;c<4;++c) {
            Target t=Hit(displays,3,points[c],cfg); Check(t.monitor==m&&t.corner==c,"all physical corners / negative coordinates");
        }
    }
    Check(Hit(displays,3,{1920,0},cfg).monitor==-1,"gap between staggered monitors is not a corner");
    Check(Hit(displays,3,{-1281,-200},cfg).monitor==-1,"outside monitor");
    Check(InCorner(displays[0],{2,2},0,3),"inclusive last hot pixel");
    Check(!InCorner(displays[0],{3,2},0,3),"exclusive hot-area boundary");
    cfg.actions[1]=None;
    Check(Hit(displays,3,{1919,0},cfg).monitor==-1,"disabled corner");
    Engine e;
    e.Observe(displays,3,{0,0},cfg,1000,false);
    Check(e.due==1250,"dwell begins on entry");
    Check(e.Expire(displays,3,{0,0},cfg,1249,false)==None,"early timer cannot fire");
    e.Observe(displays,3,{1,1},cfg,1200,false);
    Check(e.due==1250,"corner movement does not postpone dwell");
    Check(e.Expire(displays,3,{0,0},cfg,1250,false)==TaskView,"fires at dwell deadline");
    e.Observe(displays,3,{0,0},cfg,2000,false);
    Check(e.waiting.monitor==-1,"no repeated action while staying in corner");
    e.Observe(displays,3,{10,10},cfg,2100,false);
    e.Observe(displays,3,{0,0},cfg,2200,false);
    Check(e.waiting.monitor==-1,"small corner jitter cannot rearm");
    e.Observe(displays,3,{100,100},cfg,2300,false);
    e.Observe(displays,3,{0,0},cfg,2400,false);
    Check(e.due==2650,"leave rearm zone permits fresh activation");
    e.Observe(displays,3,{100,100},cfg,2500,false);
    Check(e.Expire(displays,3,{100,100},cfg,2700,false)==None,"fast pass cancels");
    e.Observe(displays,3,{0,0},cfg,2800,true);
    Check(e.waiting.monitor==-1,"drag/modifier/fullscreen blocks entry");
    e.Observe(displays,3,{0,0},cfg,3200,false);
    Check(e.waiting.monitor==-1,"unblocking alone must not trigger");
    e.Observe(displays,3,{100,100},cfg,3300,false);
    e.Observe(displays,3,{0,0},cfg,3400,false);
    Check(e.Expire(displays,3,{0,0},cfg,3650,true)==None,"block arriving during dwell cancels");
    e.Reset();
    e.Observe(displays,3,{0,0},cfg,4000,false);
    Check(e.Expire(displays,3,{0,0},cfg,4250,false)==TaskView,"state resets recover correctly");
    e.Observe(displays,3,{100,100},cfg,4300,false);
    e.Observe(displays,3,{0,1079},cfg,4350,false);
    Check(e.waiting.monitor==-1,"cooldown across corners");
    e.Reset(); e.Observe(displays,3,{0,0},cfg,5000,false);
    Check(e.Expire(displays,0,{0,0},cfg,5300,false)==None,"monitor removed during dwell");
    cfg.actions[2]=ShowDesktop; e.Reset();
    e.Observe(displays,3,{0,1079},cfg,6000,false);
    Check(e.Expire(displays,3,{0,1079},cfg,6250,false)==ShowDesktop,"action mapping");
    Config defaults; e.Reset();
    Check(Hit(displays,3,{0,0},defaults).monitor==-1,"user configuration: top-left disabled");
    Check(Hit(displays,3,{1919,0},defaults).monitor==-1,"user configuration: top-right disabled");
    e.Observe(displays,3,{0,1079},defaults,7000,false);
    Check(e.Expire(displays,3,{0,1079},defaults,7000,false)==StartMenu,"bottom-left Windows key fires in same input event");
    e.Reset(); e.Observe(displays,3,{1919,1079},defaults,8000,false);
    Check(e.Expire(displays,3,{1919,1079},defaults,8000,false)==TaskView,"bottom-right Task View fires in same input event");
    Check(e.waiting.monitor==-1 && e.due==0,"immediate action leaves no dwell timer pending");
    e.Observe(displays,3,{1919,1079},defaults,8001,false);
    Check(e.Expire(displays,3,{1919,1079},defaults,8001,false)==None,"immediate mode does not repeat while staying inside");
    e.Observe(displays,3,{1800,900},defaults,8002,false);
    e.Observe(displays,3,{1919,1079},defaults,8003,false);
    Check(e.Expire(displays,3,{1919,1079},defaults,8003,false)==TaskView,"leave and reenter fires without cooldown");
    e.Reset(); e.Observe(displays,3,{0,1079},defaults,8100,true);
    Check(e.Expire(displays,3,{0,1079},defaults,8100,true)==None,"immediate mode retains drag and fullscreen suppression");
    INPUT input[4];
    Config lockConfig;lockConfig.actions[0]=LockScreen;e.Reset();
    e.Observe(displays,3,{0,0},lockConfig,9000,false);
    Check(e.Expire(displays,3,{0,0},lockConfig,9000,false)==LockScreen,"top-left immediate lock action");
    e.Observe(displays,3,{0,0},lockConfig,9001,false);
    Check(e.Expire(displays,3,{0,0},lockConfig,9001,false)==None,"lock requires leaving before rearming");
    Check(BuildActionInput(LockScreen,input)==0,"lock uses native API without synthetic keys");
    Check(BuildActionInput(static_cast<Action>(999),input)==0,"invalid action never sends Win D");
    Check(BuildActionInput(None,input)==0,"disabled action produces no keyboard input");
    Check(BuildActionInput(StartMenu,input)==2,"Start action is one logo key press");
    Check(input[0].ki.wVk==VK_LWIN && input[0].ki.dwFlags==KEYEVENTF_EXTENDEDKEY,"Windows key down has extended marker");
    Check(input[1].ki.wVk==VK_LWIN && input[1].ki.dwFlags==(KEYEVENTF_EXTENDEDKEY|KEYEVENTF_KEYUP),"Windows key up has extended marker");
    Check(BuildActionInput(TaskView,input)==4,"Task View uses a complete Win Tab chord");
    Check(input[1].ki.wVk==VK_TAB && input[1].ki.dwFlags==0 && input[2].ki.wVk==VK_TAB && input[2].ki.dwFlags==KEYEVENTF_KEYUP,"Tab is paired and not an extended key");
    Check(input[3].ki.wVk==VK_LWIN && input[3].ki.dwFlags==(KEYEVENTF_EXTENDEDKEY|KEYEVENTF_KEYUP),"Task View releases extended Windows key last");
    Check(BuildActionInput(ShowDesktop,input)==4 && input[1].ki.wVk=='D',"optional Show Desktop uses Win D");
    printf("%d checks, %d failures\n",checks,failed); return failed?1:0;
}
