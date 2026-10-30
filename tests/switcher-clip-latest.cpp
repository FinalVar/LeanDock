#include "../include/switcher-clip-policy-latest.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace NativeSwitcherClip;
static unsigned checks;
static void check(bool ok,const char* label){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}}
int main(){
 State s;
 const Size old{1081.33,816},wide{1532,528};
 check(s.Observe({0,0,old.width,old.height},old,old)==Action::Synchronized,"real three-item viewport seeds identity");
 check(s.Observe({0,0,old.width,old.height},wide,wide)==Action::RepairPreviousSize,"captured three-to-two width clipping");
 check(s.Observe({0,0,wide.width,wide.height},wide,wide)==Action::Synchronized,"repair readback commits new viewport");
 check(s.Observe({0,0,wide.width,wide.height},wide,wide)==Action::Synchronized,"repeated layout does not repair again");
 const Size tall{1450,768};
 check(s.Observe({0,0,wide.width,wide.height},tall,tall)==Action::RepairPreviousSize,"height growth catches lower-edge clipping");
 check(s.Observe({0,0,tall.width,tall.height},tall,tall)==Action::Synchronized,"height readback");
 check(s.Observe({0,0,wide.width,wide.height},tall,tall)==Action::Ignore,"not any historical viewport");
 check(s.Observe({0,0,200,100},wide,wide)==Action::Ignore,"unrecognized deliberate clip retained");
 check(s.Observe({8,0,tall.width,tall.height},wide,wide)==Action::Ignore,"nonzero clip origin retained");
 check(s.Observe({0,8,tall.width,tall.height},wide,wide)==Action::Ignore,"nonzero clip y retained");
 check(s.Observe({0,0,tall.width,tall.height},wide,tall)==Action::Ignore,"unfinished viewport pass not repaired");
 check(s.Observe({0,0,tall.width,tall.height},{0,0},{0,0})==Action::Ignore,"unloaded size ignored");
 check(s.Observe({0,0,tall.width,tall.height},{-1,528},{-1,528})==Action::Ignore,"negative size ignored");
 check(s.Observe({0,0,tall.width,tall.height},{std::numeric_limits<double>::infinity(),528},wide)==Action::Ignore,"infinite size ignored");
 check(s.Observe({0,0,std::numeric_limits<double>::quiet_NaN(),528},wide,wide)==Action::Ignore,"NaN clip ignored");
 State fresh;
 check(fresh.Observe({0,0,old.width,old.height},wide,wide)==Action::Ignore,"new presenter cannot claim unknown clip");
 check(fresh.Observe({0,0,wide.width-.33,wide.height+.33},wide,wide)==Action::Synchronized,"fractional DPI rounding tolerated");
 check(fresh.Observe({0,0,wide.width,wide.height},old,old)==Action::RepairPreviousSize,"shrinking viewport restores boundary too");
 for(int i=1;i<=128;++i){
   Size next{800.+i*3.25,400.+i*1.5};
   State cycle;cycle.Observe({0,0,old.width,old.height},old,old);
   check(cycle.Observe({0,0,old.width,old.height},next,next)==Action::RepairPreviousSize,"changing viewport admits one correction");
   check(cycle.Observe({0,0,next.width,next.height},next,next)==Action::Synchronized,"corrected viewport stops work");
 }
 std::printf("switcher clip policy: %u checks passed\n",checks);
}
