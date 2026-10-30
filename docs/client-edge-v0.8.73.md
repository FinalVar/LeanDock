# GPT maximized window blocks Dock reveal

> 2026-10-12 更新：下文记录的候选已随 0.8.21 正常部署。旧的“尚未部署”表述是当时的历史边界；本次组件就绪不替代下文尚未完成的各项真实生命周期／界面验收。最新状态见 docs/verification-latest.json。


## Current result and boundary

On 2026-10-11 a one-shot public shell API correction was applied to the existing
ChatGPT HWND. The user confirmed both ordinary maximization and restoring then
maximizing again reveal Dock normally. This does **not** establish persistence
across recreating the application window, application restart or sign-in.

The deployed package remains 0.8.18, controller 0.3.6. The controller 0.3.7
candidate in the source tree compiled, but its test execution was blocked by
Windows Code Integrity (3077 at 18:17:40–41, Asia/Shanghai). The existing
maintenance signature preflight also refused the unsigned incoming runtime set
while Smart App Control was On. No new runtime was deployed, no existing host
was stopped, and the admission guard or security policy was not changed.

## Evidence and mechanism

The verified client is the app/ChatGPT.exe executable in package family
OpenAI.Codex_2p2nqsd0c76g0, observed version 26.1007.2314.0.
Its maximized window rectangle was (0,0)-(2560,1600), while the DWM visible frame
ended at y=1598. The hidden taskbar occupied (0,1598)-(2560,1694), clipped to
the Dock span, but did not have WS_EX_TOPMOST. WindowFromPoint in the clipped
bottom two pixels returned the client, above the taskbar in native z-order.

Setting NonRudeHWND=1, calling MarkFullscreenWindow(FALSE), and restoring the
native taskbar's topmost state with NOMOVE, NOSIZE, NOACTIVATE and NOOWNERZORDER
flags changed those hit tests to Shell_TrayWnd. Points outside the Dock span
still returned the client. Client size and IsZoomed remained unchanged.

The documented contract specifically prevents Shell's automatic full-screen
classification; MarkFullscreenWindow(FALSE) alone still permits that automatic
classification. The app symptom is independently tracked in OpenAI issue 45369.
The evidence supports this compatibility diagnosis; it does not prove the
client's internal source-level cause.

## Pending automatic implementation

- include/client-edge-policy-latest.h defines the geometry, package identity
  and property-ownership rules.
- include/client-edge-latest.h runs in the existing controller. Native
  foreground/show/location/state/destroy events coalesce into one posted
  recheck. There is no new process, polling timer or interception of mouse input.
- Only a resizable, natively maximized, visible client covering its monitor is
  eligible. The identity uses Windows package-family metadata plus executable
  suffix, not just the process name. No private Explorer interface is added.
- Only the taskbar publishing LeanDock's geometry on that monitor is promoted;
  the original Dock clipping and native reveal handling remain unchanged.
- Existing NonRudeHWND values are preserved. The adapter tags its own property,
  checks current HWND/PID/ownership on cleanup, closes event admission before
  unhooking, and rejects stale queued work after shutdown. COM reentrancy is
  bounded by an evaluation guard and a generation check.
- Restoring the window, hiding/minimizing it, losing resizability, turning off
  native auto-hide, or stopping the controller removes its owned mark. Other
  applications and real fullscreen policies are not globally overridden.

## Validation still required

The native policy/property fixture compiled, but **no fixture checks ran** due
to the application-control denial. The complete candidate controller also
compiled without executing it. The current-window GUI success must not be used
as acceptance of this event-driven candidate.

After Windows normally admits an appropriately trusted candidate and recovery
set, run tests/client-edge-latest.ps1, build through build/latest.ps1, package
and use the existing Update transaction. Do not bypass that transaction or
rename/move a denied executable to retry it. Confirm all four components recover
with protection On, then test client restart, repeat maximization, Dock-only
bottom span, other applications, true fullscreen, explorer recreation and stop
cleanup. Multi-monitor and secure-desktop behavior remain unverified.

The one-shot diagnostic added NonRudeHWND to the then-current HWND without the
new adapter's ownership token. Before future automatic acceptance, restore only
that recorded same-lifetime property (previous value zero), or use a naturally
recreated client window; never erase a preexisting foreign mark.

Evidence retained outside the published source: work/edge-reveal, including
read-only before/after hit tests, apply-current-latest.json, pending-build and
admission-latest.json. None of those diagnostic scripts is an installed startup
entry. The pending binary is a build artifact, not the deployed latest.exe.
