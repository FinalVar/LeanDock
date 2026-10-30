// Included inside NativeCorners after LaunchAction. A real corner entry is
// the only caller. Never run an opening chord through an active Start popup.
static constexpr UINT_PTR kStartDismiss=0x434D;
static nhc::StartGate startGate;
static HWND startDismissWindow;
static unsigned startDismissCount;
static void CancelStartDismiss(){
    if(window)KillTimer(window,kStartDismiss);
    startGate.Clear();startDismissWindow=nullptr;startDismissCount=0;
}
static void CompleteCorner(nhc::Action action,int corner){
    if(LaunchAction(action)){++activations[corner];Log(ActionName(action));}
    else{++failures;Log("action-failed",lastErrorCode);}
}
static bool SendStartEscape(HWND start){
    INPUT keys[nhc::ActionInputCapacity];
    const UINT count=nhc::BuildActionInput(nhc::AppSwitcher,keys,true); // Escape pair.
    if(ModifierOrButton()||nhc::AnyActionKeyHeld(keys,count,[](WORD key){return GetAsyncKeyState(key)<0;})){
        lastErrorCode=ERROR_BUSY;++failures;Log("start-dismiss-input-held",lastErrorCode);return false;
    }
    SetLastError(ERROR_SUCCESS);const UINT sent=SendInput(count,keys,sizeof(INPUT));
    if(sent!=count){
        lastErrorCode=GetLastError();if(!lastErrorCode)lastErrorCode=ERROR_GEN_FAILURE;
        INPUT cleanup[nhc::ActionInputCapacity];UINT n=nhc::BuildReleaseInput(keys,count,sent,cleanup);
        if(n)SendInput(n,cleanup,sizeof(INPUT));++failures;Log("start-dismiss-failed",lastErrorCode);return false;
    }
    startDismissWindow=start;++startDismissCount;
    Log("start-dismiss-request",unsigned(LeanDockStartState::Identify(start)));
    return true;
}
static void DispatchCorner(nhc::Action action,int corner){
    CancelStartDismiss();
    const HWND start=(action==nhc::ShowDesktop||action==nhc::AppSwitcher||action==nhc::TaskView)?
        LeanDockStartState::Foreground():nullptr;
    if(!start){CompleteCorner(action,corner);return;}
    if(!SendStartEscape(start)){CancelStartDismiss();return;}
    startGate.Arm(action,corner,GetTickCount64());
    if(!SetTimer(window,kStartDismiss,15,nullptr)){
        lastErrorCode=GetLastError();CancelStartDismiss();++failures;Log("start-dismiss-timer-failed",lastErrorCode);return;
    }
}
static void ContinueAfterStart(){
    const bool interrupted=paused||locked||ModifierOrButton()||!DefaultInputDesktop()||
        (IsWindow(startDismissWindow)&&!LeanDockStartState::Host(startDismissWindow));
    // Escape from Search may reveal its parent Start panel. Dismiss that
    // bounded second surface without renewing the original 700 ms deadline.
    const HWND next=LeanDockStartState::Foreground();
    if(!interrupted&&GetTickCount64()<startGate.deadline&&next&&next!=startDismissWindow){
        if(startDismissCount>=2||!SendStartEscape(next)){CancelStartDismiss();return;}
        return;
    }
    auto state=startGate.Poll(GetTickCount64(),LeanDockStartState::Shown(startDismissWindow),
        GetForegroundWindow()==startDismissWindow,interrupted);
    if(state==nhc::StartGateResult::Wait)return;
    const auto action=startGate.action;const int corner=startGate.corner;CancelStartDismiss();
    if(state==nhc::StartGateResult::Execute){Log("start-dismiss-complete");CompleteCorner(action,corner);}
    else if(state==nhc::StartGateResult::Cancel){lastErrorCode=ERROR_CANCELLED;++failures;Log("start-dismiss-cancelled",lastErrorCode);}
}
