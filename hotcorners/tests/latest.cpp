#include "../core/latest.h"
#include "../actions/latest.h"
#include "../fullscreen/latest.h"
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
    Config dwellConfig; dwellConfig.actions[0]=LockScreen; dwellConfig.actions[2]=ShowDesktop;
    dwellConfig.cornerDelay[0]=500; e={};
    e.Observe(displays,3,{0,0},dwellConfig,10000,false);
    Check(e.due==10500,"left top uses its own 500ms dwell");
    Check(e.Expire(displays,3,{0,0},dwellConfig,10499,false)==None,"499ms cannot lock");
    e.Observe(displays,3,{1,1},dwellConfig,10499,false);
    Check(e.due==10500,"movement within trigger area preserves dwell start");
    Check(e.Expire(displays,3,{1,1},dwellConfig,10500,false)==LockScreen,"500ms locks");
    e.Observe(displays,3,{0,0},dwellConfig,11000,false);
    Check(e.waiting.monitor==-1,"dwell lock still requires spatial rearm");
    e={}; e.Observe(displays,3,{0,0},dwellConfig,12000,false);
    e.Observe(displays,3,{3,0},dwellConfig,12499,false);
    Check(e.waiting.monitor==-1&&e.due==0,"leaving trigger area before 500ms cancels");
    e.Observe(displays,3,{0,0},dwellConfig,12500,false);
    Check(e.due==13000,"returning starts a full new 500ms dwell");
    Check(e.Expire(displays,3,{0,0},dwellConfig,12999,false)==None,"separate short visits never accumulate");
    Check(e.Expire(displays,3,{0,0},dwellConfig,13000,true)==None,"modifier or fullscreen during dwell cancels lock");
    e={}; e.Observe(displays,3,{0,0},dwellConfig,14000,false);
    e.Observe(displays,3,{0,1079},dwellConfig,14100,false);
    Check(e.due==14100&&e.Expire(displays,3,{0,1079},dwellConfig,14100,false)==ShowDesktop,"left bottom remains immediate after leaving pending lock");
    e={}; e.Observe(displays,3,{1919,1079},dwellConfig,15000,false);
    Check(e.Expire(displays,3,{1919,1079},dwellConfig,15000,false)==TaskView,"right bottom remains immediate");
    e={}; e.Observe(displays,3,{-1280,-200},dwellConfig,16000,false);
    Check(e.due==16500,"left top dwell applies to another display with negative coordinates");
    Config inherited; inherited.delay=300;
    Check(inherited.Delay(0)==300&&inherited.Delay(2)==300,"missing per-corner settings inherit global delay");
    inherited.cornerDelay[0]=0;
    Check(inherited.Delay(0)==0&&inherited.Delay(2)==300,"explicit zero override remains immediate");
    Check(BuildActionInput(static_cast<Action>(999),input)==0,"invalid action never sends Win D");
    Check(BuildActionInput(None,input)==0,"disabled action produces no keyboard input");
    Check(BuildActionInput(StartMenu,input)==2,"Start action is one logo key press");
    Check(input[0].ki.wVk==VK_LWIN && input[0].ki.dwFlags==KEYEVENTF_EXTENDEDKEY,"Windows key down has extended marker");
    Check(input[1].ki.wVk==VK_LWIN && input[1].ki.dwFlags==(KEYEVENTF_EXTENDEDKEY|KEYEVENTF_KEYUP),"Windows key up has extended marker");
    Check(BuildActionInput(TaskView,input)==4,"Task View uses a complete Win Tab chord");
    Check(input[1].ki.wVk==VK_TAB && input[1].ki.dwFlags==0 && input[2].ki.wVk==VK_TAB && input[2].ki.dwFlags==KEYEVENTF_KEYUP,"Tab is paired and not an extended key");
    Check(input[3].ki.wVk==VK_LWIN && input[3].ki.dwFlags==(KEYEVENTF_EXTENDEDKEY|KEYEVENTF_KEYUP),"Task View releases extended Windows key last");
    Check(BuildActionInput(ShowDesktop,input)==4 && input[1].ki.wVk=='D',"optional Show Desktop uses Win D");
    const RECT monitor{0,0,2560,1600},full=monitor,gap{0,0,2560,1598};
    const LONG_PTR gptStyle=0x15070000; // live maximized frameless GPT window
    Check(!SuppressFullscreen(gptStyle,full,monitor,true,true),"verified GPT maximization remains usable when DWM reaches screen edge");
    Check(!SuppressFullscreen(gptStyle,gap,monitor,true,true),"two pixel DWM gap does not change GPT corner behavior");
    Check(SuppressFullscreen(gptStyle,full,monitor,true,false),"identity failure/unrelated maximized fullscreen retains suppression");
    Check(SuppressFullscreen(gptStyle,full,monitor,false,true),"GPT non-maximized fullscreen retains suppression");
    Check(SuppressFullscreen(gptStyle & ~WS_THICKFRAME,full,monitor,true,true),"fullscreen with stale maximized flag but no sizing frame is blocked");
    Check(SuppressFullscreen(WS_POPUP,full,monitor,false,false),"borderless fullscreen application remains blocked");
    Check(!SuppressFullscreen(WS_CAPTION|WS_THICKFRAME,full,monitor,true,false),"captioned application keeps previous exemption");
    Check(!SuppressFullscreen(gptStyle,gap,monitor,false,false),"unrelated application exposing monitor edge is not fullscreen");
    Check(!SuppressFullscreen(gptStyle,full,RECT{0,0,0,0},true,true),"invalid monitor cannot classify as fullscreen");
    const RECT negative{-2560,-1600,0,0};
    Check(!SuppressFullscreen(gptStyle,negative,negative,true,true),"verified client supports negative monitor coordinates");
    Check(SuppressFullscreen(WS_POPUP,negative,negative,false,false),"fullscreen protection supports negative coordinates");
    Config maxConfig;maxConfig.actions[2]=ShowDesktop;Engine maxEngine;
    const Rect maxMonitor[]{ {0,0,2560,1600} };
    for(int corner=2;corner<=3;++corner) {
        const Point cornerPoint{corner==2?0:2559,1599};
        maxEngine={};
        bool suppress=SuppressFullscreen(gptStyle,full,monitor,true,true);
        maxEngine.Observe(maxMonitor,1,cornerPoint,maxConfig,17000,suppress);
        Check(maxEngine.Expire(maxMonitor,1,cornerPoint,maxConfig,17000,suppress)==maxConfig.actions[corner],"screen-covering GPT corner fires immediately");
        maxEngine.Observe(maxMonitor,1,cornerPoint,maxConfig,17001,suppress);
        Check(maxEngine.Expire(maxMonitor,1,cornerPoint,maxConfig,17001,suppress)==None,"new exemption still requires spatial rearm");
        maxEngine.Observe(maxMonitor,1,{1280,800},maxConfig,17002,false);
        suppress=SuppressFullscreen(gptStyle,gap,monitor,true,true);
        maxEngine.Observe(maxMonitor,1,cornerPoint,maxConfig,17003,suppress);
        Check(maxEngine.Expire(maxMonitor,1,cornerPoint,maxConfig,17003,suppress)==maxConfig.actions[corner],"DWM gap transition keeps reentered corner immediate");
    }
    printf("%d checks, %d failures\n",checks,failed); return failed?1:0;
}
