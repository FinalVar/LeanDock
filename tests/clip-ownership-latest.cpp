#include "../include/clip-ownership-latest.h"
#include <initializer_list>
using LeanDockClipOwnership::ShiftedFullHeight;
constexpr RECT original{972,0,1588,96};
constexpr RECT stale{972,-7,1588,89};
static_assert(ShiftedFullHeight(SIMPLEREGION,stale,original,2560,96));
static_assert(!ShiftedFullHeight(SIMPLEREGION,original,original,2560,96));
// Native fold, empty and externally owned complex clips remain untouched.
static_assert(!ShiftedFullHeight(SIMPLEREGION,{972,89,1588,96},original,2560,96));
static_assert(!ShiftedFullHeight(SIMPLEREGION,{972,0,1588,89},original,2560,96));
static_assert(!ShiftedFullHeight(SIMPLEREGION,{0,89,2560,96},original,2560,96));
static_assert(!ShiftedFullHeight(NULLREGION,stale,original,2560,96));
static_assert(!ShiftedFullHeight(ERROR,stale,original,2560,96));
static_assert(!ShiftedFullHeight(COMPLEXREGION,stale,original,2560,96));
// Do not claim a new owner's changed width or a previous monitor/DPI height.
static_assert(!ShiftedFullHeight(SIMPLEREGION,{973,-7,1588,89},original,2560,96));
static_assert(!ShiftedFullHeight(SIMPLEREGION,{972,-7,1589,89},original,2560,96));
static_assert(!ShiftedFullHeight(SIMPLEREGION,stale,original,2560,64));
static_assert(!ShiftedFullHeight(SIMPLEREGION,stale,original,1500,96));
static_assert(!ShiftedFullHeight(SIMPLEREGION,stale,{972,1,1588,97},2560,96));
static_assert(!ShiftedFullHeight(SIMPLEREGION,stale,{1588,0,972,96},2560,96));
static_assert(!ShiftedFullHeight(SIMPLEREGION,{972,-96,1588,0},original,2560,96));
static_assert(!ShiftedFullHeight(SIMPLEREGION,{972,7,1588,103},original,2560,96));
static_assert(!ShiftedFullHeight(SIMPLEREGION,stale,original,0,96));
static_assert(!ShiftedFullHeight(SIMPLEREGION,stale,original,2560,0));
// Exercise every overlapping negative shift at common physical taskbar heights.
constexpr bool AllDpiShifts() {
    for(int height:{64,80,96,128}) {
        const RECT applied{300,0,900,height};
        for(int dy=-height+1;dy<0;++dy) {
            if(!ShiftedFullHeight(SIMPLEREGION,{300,dy,900,height+dy},applied,1920,height))return false;
            if(ShiftedFullHeight(SIMPLEREGION,{300,dy+1,900,height+dy},applied,1920,height))return false;
        }
    }
    return true;
}
static_assert(AllDpiShifts());
