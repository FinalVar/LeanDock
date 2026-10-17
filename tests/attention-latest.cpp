#include <windows.h>
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include "../include/attention-policy-latest.h"

std::atomic<bool> g_eventStopping{false};
using Unhide = void(WINAPI*)(void*,int,int);
Unhide TrayUI_Unhide_Original;
Unhide CSecondaryTray__Unhide_Original;
#include "../src/hide-attention-latest.inc"

int nativeFlashing = TRUE, flashingReads = 0, primaryCalls = 0, secondaryCalls = 0;
void* lastSelf{}; int lastFlags{}, lastRequest{};
bool nativeMenuOpen = false, nativePointerOver = false, nativeDrag = false;
BOOL WINAPI Flashing(void*) { ++flashingReads; return nativeFlashing; }
BOOL WINAPI Permit(void* self) {
    return !nativeMenuOpen && !nativePointerOver && !nativeDrag &&
        !CTaskBtnGroup_IsFlashing_Hook(self);
}
void WINAPI Primary(void* self, int flags, int request) {
    ++primaryCalls; lastSelf=self; lastFlags=flags; lastRequest=request;
}
void WINAPI Secondary(void* self, int flags, int request) {
    ++secondaryCalls; lastSelf=self; lastFlags=flags; lastRequest=request;
}
BOOL WINAPI Nested(void* self) {
    assert(g_attentionPermissionQuery);
    CTaskListWnd_PermitAutoHide_Original=Permit;
    assert(CTaskListWnd_PermitAutoHide_Hook(self));
    assert(g_attentionPermissionQuery);
    return TRUE;
}
BOOL WINAPI Throwing(void*) { throw 42; }
int main() {
    CTaskBtnGroup_IsFlashing_Original=Flashing;
    CTaskListWnd_PermitAutoHide_Original=Permit;
    TrayUI_Unhide_Original=Primary;
    CSecondaryTray__Unhide_Original=Secondary;
    void* self=&primaryCalls;
    // Actual hook bodies: hide may proceed while the visual query stays true.
    assert(CTaskBtnGroup_IsFlashing_Hook(self)==TRUE);
    assert(CTaskListWnd_PermitAutoHide_Hook(self)==TRUE);
    assert(!g_attentionPermissionQuery);
    assert(CTaskBtnGroup_IsFlashing_Hook(self)==TRUE);
    assert(flashingReads==3);
    nativeMenuOpen=true; assert(!CTaskListWnd_PermitAutoHide_Hook(self)); nativeMenuOpen=false;
    nativePointerOver=true; assert(!CTaskListWnd_PermitAutoHide_Hook(self)); nativePointerOver=false;
    nativeDrag=true; assert(!CTaskListWnd_PermitAutoHide_Hook(self)); nativeDrag=false;
    // A nested query and an unwinding native call restore the scope correctly.
    CTaskListWnd_PermitAutoHide_Original=Nested;
    assert(CTaskListWnd_PermitAutoHide_Hook(self)); assert(!g_attentionPermissionQuery);
    CTaskListWnd_PermitAutoHide_Original=Throwing;
    try { CTaskListWnd_PermitAutoHide_Hook(self); assert(false); } catch(int e) { assert(e==42); }
    assert(!g_attentionPermissionQuery);
    CTaskListWnd_PermitAutoHide_Original=Permit;
    // The visual query on a different thread must never inherit hide scope.
    { LeanDockAttention::PermissionScope scope(g_attentionPermissionQuery,true);
      std::thread other([] { assert(!g_attentionPermissionQuery); }); other.join(); }
    // Keep unknown/future requests and all flags verbatim; only request 17 drops.
    for(int request=-1;request<=64;++request) {
        const auto p=primaryCalls,s=secondaryCalls;
        TrayUI_Unhide_Hook(self,0x1234,request);
        CSecondaryTray__Unhide_Hook(self,0x5678,request);
        if(request==17) {assert(primaryCalls==p && secondaryCalls==s);}
        else {assert(primaryCalls==p+1 && secondaryCalls==s+1);
              assert(lastSelf==self && lastFlags==0x5678 && lastRequest==request);}
    }
    // Stop and late callbacks pass through, including an already-entered scope.
    g_eventStopping=true;
    assert(!CTaskListWnd_PermitAutoHide_Hook(self));
    { LeanDockAttention::PermissionScope scope(g_attentionPermissionQuery,true);
      assert(CTaskBtnGroup_IsFlashing_Hook(self)==TRUE); }
    const auto p=primaryCalls,s=secondaryCalls;
    TrayUI_Unhide_Hook(self,0,17); CSecondaryTray__Unhide_Hook(self,1,17);
    assert(primaryCalls==p+1 && secondaryCalls==s+1);
    g_eventStopping=false; nativeFlashing=FALSE;
    assert(CTaskListWnd_PermitAutoHide_Hook(self));
    assert(!CTaskBtnGroup_IsFlashing_Hook(self));
    assert(g_attentionBlockedReveals==2);
    std::cout<<"Attention hooks: visual state, native inhibitors, primary/secondary reveal requests, nested/thread scopes and Stop passed.\n";
}
