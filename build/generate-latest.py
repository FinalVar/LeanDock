"""Reproducible NativeTaskbarDock source adaptation. Does not deploy or inject."""
from pathlib import Path
import hashlib
import json
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "vendor" / "windhawk-sdk"
NAMES = ["styler", "tray", "size", "hide", "show"]
IDS = [
    "windows-11-taskbar-styler",
    "local@taskbar-notification-icon-spacing-merged",
    "taskbar-icon-size",
    "taskbar-auto-hide-when-maximized",
    "taskbar-notification-icons-show-all",
]


def read(path):
    return path.read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def emit(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_text(encoding="utf-8") == text:
        return
    path.write_text(text, encoding="utf-8", newline="\n")


def wide(value):
    return "L" + json.dumps(str(value), ensure_ascii=True)


def replace_once(text, old, new):
    assert text.count(old) == 1, (old[:100], text.count(old))
    return text.replace(old, new)


def replace_function(text, signature, body):
    start = text.index(signature)
    opening = text.index("{", start)
    # The chosen adaptation points have no brace characters inside string literals.
    depth = 1
    end = opening + 1
    quote = None
    line_comment = block_comment = False
    while depth:
        c = text[end]
        pair = text[end:end + 2]
        if line_comment:
            line_comment = c != "\n"
        elif block_comment:
            if pair == "*/":
                block_comment = False
                end += 1
        elif quote:
            if c == "\\":
                end += 1
            elif c == quote:
                quote = None
        elif pair == "//":
            line_comment = True
            end += 1
        elif pair == "/*":
            block_comment = True
            end += 1
        elif c in "\"'":
            quote = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        end += 1
    return text[:opening] + "{\n" + body + "\n}" + text[end:]


def main():
    include = ROOT / "include"
    # Header-only API signatures are retained for reviewable upstream reuse.
    # All implementations belong to this project; no Windhawk engine is linked.
    for filename in ("windhawk_api.h", "windhawk_api_internal.h", "windhawk_utils.h"):
        text = read(SDK / filename)
        if filename == "windhawk_api_internal.h":
            text = replace_once(
                text, "inline void* InternalWhModPtr;",
                "static void* const InternalWhModPtr = reinterpret_cast<void*>(ND_COMPONENT);")
        if filename != "windhawk_utils.h":
            text = re.sub(r"(?m)^inline ", "static inline ", text)
        emit(include / filename, text)

    style = json.loads(read(ROOT / "baseline" / "styles-latest.json"))
    constants = {}
    for key, value in style.items():
        if key.startswith("styleConstants[") and value:
            name, val = value.split("=", 1)
            constants[name] = val
    rules = []
    targets = sorted(int(match.group(1)) for key in style
                     if (match := re.fullmatch(r"controlStyles\[(\d+)\]\.target", key)))
    for index in targets:
        target = style[f"controlStyles[{index}].target"]
        values = []
        for sub in range(100):
            key = f"controlStyles[{index}].styles[{sub}]"
            if key not in style:
                break
            value = style[key]
            for name, val in constants.items():
                value = value.replace("$" + name, val)
            values.append(value)
        rules.append("    AddElementCustomizationRules(" + wide(target) + ", {" +
                     ", ".join(wide(v) for v in values) + "});")

    sources = []
    for component, name in enumerate(NAMES, 1):
        source = read(ROOT / "upstream" / (name + ".cpp"))
        source = source[source.index("#include"):]
        if name == "styler":
            source = '#include "clip-ownership-latest.h"\n' + source
            source = source.replace("#include <winrt/Windows.UI.Xaml.h>", "#include <winrt/Windows.UI.Xaml.h>\n#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>\n#include <winrt/Windows.UI.Xaml.Input.h>\n#include <winrt/Windows.UI.Xaml.Data.h>", 1)
            a = source.index("const Theme g_themeTranslucentTaskbar")
            b = source.index("// An InstanceHandle", a)
            # Retain non-theme declarations immediately before the visual-tree section.
            tail = source[a:b]
            last_theme_end = tail.rfind("// clang-format on")
            assert last_theme_end >= 0
            source = source[:a] + tail[last_theme_end:] + source[b:]
            source = replace_function(
                source, "void ProcessAllStylesFromSettings()",
                "\n".join(rules) + "\n    g_resourceVariables.clear();")
            source = replace_function(source, "void PromptToRestartExplorer(HRESULT statusCode)",
                                      '    Native_ReportFailure(L"XAML diagnostics attachment", statusCode);')
            prompt_start = source.index("constexpr WCHAR kRestartExplorerPromptTitle[]")
            prompt_end = source.index("void PromptToRestartExplorer(", prompt_start)
            source = source[:prompt_start] + source[prompt_end:]
            source = source.replace("    StartStatsTimer();", "    // Offline dedicated build: no telemetry timer.")
            source = source.replace("    StopStatsTimer();", "")
            start = source.index("bool StartStatsTimer()")
            end = source.index("void LoadSettings()", start)
            source = source[:start] + source[end:]
            # Bound startup attachment attempts; the external controller handles readiness.
            source = source.replace("for (int i = 0; i < 10000; i++)",
                                    "for (int i = 0; i < 32; i++)")
            source = source.replace(
                "{ 0xc85d8cc7, 0x5463, 0x40e8, { 0xa4, 0x32, 0xf5, 0x91, 0x6b, 0x64, 0x27, 0xe5 } }",
                "{ 0x7e54f66a, 0x1a8e, 0x4b90, { 0x94, 0x2f, 0x37, 0x45, 0x8b, 0x35, 0xe2, 0x01 } }")
            source = '#include "dock-geometry-latest.h"\n#include "cards-latest.h"\n#include "cards-preferences-latest.h"\n#include "shell-actions-latest.h"\n#include "shell-menu-latest.h"\n#include "backdrop-resume-latest.h"\n' + source
            # Preserve the upstream effect graph; add only a bounded reconnect
            # of its backdrop source after native desktop/expansion transitions.
            source = replace_once(source, "    ~XamlBlurBrush();", """    ~XamlBlurBrush();
    void ScheduleBackdropResume(XamlRoot const& root,HWND taskbar,unsigned reason) noexcept;
    void PrepareBackdrop(XamlRoot const& root,HWND taskbar) noexcept;
    void StopBackdropResume() noexcept;""")
            source = replace_once(source, "    wuc::Compositor m_compositor;", """    winrt::weak_ref<FrameworkElement> m_backdropOwner;
    winrt::Windows::System::DispatcherQueueTimer m_backdropTimer{nullptr};
    winrt::event_token m_backdropTick{};
    LeanDockBackdrop::Plan m_backdropPlan;
    HWND m_backdropTaskbar{};
    bool RefreshBackdropSource(XamlRoot const& root,HWND taskbar,bool requireVisible);
    wuc::Compositor m_compositor;""")
            source = replace_once(source, "    auto fe = element.try_as<FrameworkElement>();\n\n    auto createProxy", "    auto fe = element.try_as<FrameworkElement>();\n    if(fe)m_backdropOwner=winrt::make_weak(fe);\n\n    auto createProxy")
            source = replace_once(source, "XamlBlurBrush::~XamlBlurBrush()\n{", "XamlBlurBrush::~XamlBlurBrush()\n{\n    StopBackdropResume();")
            source = replace_once(source, "void XamlBlurBrush::OnDisconnected()\n{", "void XamlBlurBrush::OnDisconnected()\n{\n    StopBackdropResume();")
            source = replace_once(source, "// Helper functions for tracking, caching and retrying remote image loads.", '#include "backdrop-brush-latest.inc"\n\n// Helper functions for tracking, caching and retrying remote image loads.')
            source = replace_once(source, "value = winrt::make<XamlBlurBrush>(", "value = MakeLeanDockBlurBrush(")
            source = replace_once(source, "struct ClickThroughTaskbarState {", """struct ClickThroughTaskbarState {
    winrt::weak_ref<Controls::Button> menuAnchor;
    winrt::weak_ref<Controls::Grid> menuAnchorParent;
    winrt::weak_ref<Controls::MenuFlyout> menuFocusFlyout;
    winrt::event_token menuFocusOpened{};
    winrt::weak_ref<Controls::Control> menuFocusItem;
    winrt::event_token menuFocusGot{};
    ULONGLONG menuPointerAt{};
    winrt::weak_ref<Controls::Grid> contextRoot;
    UIElement::ContextRequested_revoker contextRequested;
    winrt::weak_ref<Controls::Grid> contextTrayRoot;
    UIElement::ContextRequested_revoker contextTrayRequested;
    winrt::weak_ref<Shapes::Rectangle> divider;
    winrt::weak_ref<Controls::Grid> dividerParent;
    winrt::weak_ref<Controls::Grid> material;
    winrt::weak_ref<Controls::Canvas> materialHost;
    winrt::weak_ref<Shapes::Rectangle> materialSource;
    winrt::weak_ref<Controls::Grid> materialParent;
    bool materialFailed=false;
    std::shared_ptr<LeanDockCards::Host> cards;
    bool cardsClosing=false;""")
            source = replace_once(source, "void UpdateClickThroughRegion(ClickThroughTaskbarState& state) {",
                '#include "surface-latest.inc"\nvoid UpdateClickThroughRegion(ClickThroughTaskbarState& state) {')
            source = replace_once(source, """    RECT currentRgnBox;
    bool hasRgn = GetWindowRgnBox(topLevelWnd, &currentRgnBox) != ERROR;
    auto appliedRgnBoxIt = g_clickThroughAppliedRgnBoxes.find(topLevelWnd);
    bool rgnIsOurs = hasRgn &&
                     appliedRgnBoxIt != g_clickThroughAppliedRgnBoxes.end() &&
                     EqualRect(&currentRgnBox, &appliedRgnBoxIt->second);""", """    RECT currentRgnBox{};
    const int currentRgnKind = GetWindowRgnBox(topLevelWnd, &currentRgnBox);
    bool hasRgn = currentRgnKind != ERROR;
    auto appliedRgnBoxIt = g_clickThroughAppliedRgnBoxes.find(topLevelWnd);
    const bool rgnMatchesApplied = hasRgn &&
                     appliedRgnBoxIt != g_clickThroughAppliedRgnBoxes.end() &&
                     EqualRect(&currentRgnBox, &appliedRgnBoxIt->second);
    const bool rgnIsShifted = appliedRgnBoxIt != g_clickThroughAppliedRgnBoxes.end() &&
        LeanDockClipOwnership::ShiftedFullHeight(currentRgnKind,currentRgnBox,
            appliedRgnBoxIt->second,tlWidth,tlHeight);
    // A translated copy of our full-height clip is still ours to repair.
    // Genuine native fold clips and unknown owners retain the upstream guard.
    const bool rgnIsOurs = rgnMatchesApplied || rgnIsShifted;""")
            source = replace_once(source,
                "if (signature == state.lastRegionSignature && rgnIsOurs) {",
                "if (signature == state.lastRegionSignature && rgnMatchesApplied) {")
            source = replace_once(source, """        RECT currentRgnBox;
        if (GetWindowRgnBox(topLevelWnd, &currentRgnBox) == ERROR ||
            !EqualRect(&currentRgnBox, &appliedRgnBox)) {
            continue;
        }""", """        RECT currentRgnBox{},windowRect{};
        const int kind=GetWindowRgnBox(topLevelWnd,&currentRgnBox);
        if(kind==ERROR)continue;
        if(!EqualRect(&currentRgnBox,&appliedRgnBox) &&
           (!GetWindowRect(topLevelWnd,&windowRect) ||
            !LeanDockClipOwnership::ShiftedFullHeight(kind,currentRgnBox,appliedRgnBox,
                windowRect.right-windowRect.left,windowRect.bottom-windowRect.top))) {
            continue;
        }""")
            source = replace_once(source, 'BOOL Wh_ModInit() {', 'BOOL Wh_ModInit() {\n    InstallLeanDockMenuHook();')
            signature = "LRESULT CALLBACK ClickThroughTaskbarSubclassProc("
            start = source.index(signature)
            body = source.index("    switch (uMsg) {", start)
            source = source[:body] + "    if(uMsg==LeanDockMaterialProbeMessage())return SampleLeanDockMaterial(hWnd,wParam,lParam);\n    if(uMsg==LeanDockBackdrop::Message()){ResumeLeanDockBackdrop(hWnd,wParam,lParam);return 0;}\n    if(uMsg==WM_CONTEXTMENU && HandleLeanDockContext(hWnd,lParam))return 0;\n    if(uMsg==WM_TIMECHANGE)LeanDockTimeChanged(hWnd);\n    if(uMsg==LeanDockCards::PreferencesMessage())LeanDockPreferencesChanged(hWnd);\n" + source[body:]
            source = replace_once(source, "    // LayoutUpdated is frequent; only re-apply when the result actually",
                """    // Share only the projected, fully shown geometry. Both popup placement
    // and edge reveal track application/tray count and DPI changes automatically.
    RECT windowRect{}; POINT origin{};
    if(GetWindowRect(topLevelWnd,&windowRect) && ClientToScreen(state.islandHwnd,&origin)) {
        auto span=*taskbarFrameRect;
        if(systemTrayFrameRect) {
            const float right=std::max(span.X+span.Width,systemTrayFrameRect->X+systemTrayFrameRect->Width);
            span.X=std::min(span.X,systemTrayFrameRect->X);span.Width=right-span.X;
        }
        const int left=origin.x-windowRect.left+std::lround(span.X*scale);
        LeanDockGeometry::Publish(topLevelWnd,left,left+std::lround(span.Width*scale),std::lround(span.Height*scale));
        UpdateLeanDockDivider(state);
    }
    // LayoutUpdated is frequent; only re-apply when the result actually""")
            source = replace_once(source,"void ClearClickThroughRegions() {", """void ClearClickThroughRegions() {
    StopLeanDockBackdrops();
    for(auto& state:g_clickThroughTaskbarState) {
        ClearLeanDockDivider(state);
        if(state.islandHwnd)LeanDockGeometry::Clear(GetAncestor(state.islandHwnd,GA_ROOT));
    }""")
        if name == "hide":
            source = '#include "dock-geometry-latest.h"\n#include "event-loop-latest.h"\n#include "backdrop-resume-latest.h"\n#include "attention-policy-latest.h"\n#include <stdexcept>\n' + source
            source = replace_once(source, "CSecondaryTray__Unhide_t CSecondaryTray__Unhide_Original;",
                "CSecondaryTray__Unhide_t CSecondaryTray__Unhide_Original;\n\n" +
                read(ROOT / "src" / "hide-attention-latest.inc"))
            source = replace_once(source, "            &TrayUI_Unhide_Original,\n        },",
                "            &TrayUI_Unhide_Original,\n            TrayUI_Unhide_Hook,\n        },")
            source = replace_once(source, "            &CSecondaryTray__Unhide_Original,\n        },",
                "            &CSecondaryTray__Unhide_Original,\n            CSecondaryTray__Unhide_Hook,\n        },")
            source = replace_once(source, "    // Taskbar.dll, explorer.exe\n    WindhawkUtils::SYMBOL_HOOK symbolHooks[] = {", """    // Taskbar.dll, explorer.exe
    WindhawkUtils::SYMBOL_HOOK symbolHooks[] = {
        {{LR"(public: virtual int __cdecl CTaskListWnd::PermitAutoHide(void))"},
         &CTaskListWnd_PermitAutoHide_Original, CTaskListWnd_PermitAutoHide_Hook},
        {{LR"(public: virtual int __cdecl CTaskBtnGroup::IsFlashing(void))"},
         &CTaskBtnGroup_IsFlashing_Original, CTaskBtnGroup_IsFlashing_Hook},""")
            source = replace_once(source, "using ViewCoordinator_ShouldTaskbarBeExpanded_t =",
                read(ROOT / "src" / "hide-state-latest.inc") + "\nusing ViewCoordinator_ShouldTaskbarBeExpanded_t =")
            source = replace_once(source, "    g_taskbarToViewCoordinator[hMMTaskbarWnd] = pThis;", """    g_taskbarToViewCoordinator[hMMTaskbarWnd] = pThis;
    ResumeValue(hMMTaskbarWnd,L"ExpandedAtQuery",expanded);
    ResumeValue(hMMTaskbarWnd,L"QueryAt",GetTickCount64());""")
            source = replace_once(source, "            return true;\n        }\n    }\n\n    return ViewCoordinator_ShouldTaskbarBeExpanded_Original(", "            ResumeValue(hMMTaskbarWnd,L\"Decision\",1);\n            return true;\n        }\n    }\n\n    return ViewCoordinator_ShouldTaskbarBeExpanded_Original(")
            source = replace_once(source, """    return ViewCoordinator_ShouldTaskbarBeExpanded_Original(
        pThis, hMMTaskbarWnd, expanded);""", """    const bool result=ViewCoordinator_ShouldTaskbarBeExpanded_Original(pThis,hMMTaskbarWnd,expanded);
    ResumeValue(hMMTaskbarWnd,L"Decision",result);return result;""")
            source = replace_once(source, "            ViewCoordinator_UpdateIsExpanded_Original(\n                it->second, hWnd, kReasonIsPointerOverTaskbarFrameChanged);", """            ReadExpandedState(hWnd,L"BeforeSync");
            ViewCoordinator_UpdateIsExpanded_Original(
                it->second, hWnd, kReasonIsPointerOverTaskbarFrameChanged);
            const int after=ReadExpandedState(hWnd,L"AfterSync");
            // Unhide above may already have expanded the taskbar before this
            // query. The policy transition, not a before/after inequality,
            // owns the bounded refresh request.
            if(after==1)LeanDockBackdrop::Notify(hWnd,LeanDockBackdrop::Reason::Expansion);
            ResumeValue(hWnd,L"SyncAt",GetTickCount64());""")
            source = replace_once(source, "    // Taskbar.View.dll\n    WindhawkUtils::SYMBOL_HOOK symbolHooks[] = {", """    // Taskbar.View.dll
    WindhawkUtils::SYMBOL_HOOK symbolHooks[] = {
        {{LR"(public: bool __cdecl winrt::Taskbar::implementation::ViewCoordinator::IsExpanded(unsigned __int64))"},
         &ViewCoordinator_IsExpanded_Original, nullptr, true},""")
            for unhide_call in (
                "TrayUI_Unhide_Original(pTrayUI_ITrayComponentHost, 0, 0);",
                "CSecondaryTray__Unhide_Original(pThis, 0, 0);",
            ):
                source = replace_once(source, unhide_call,
                    "LeanDockBackdrop::Prepare(hWnd);\n                " + unhide_call)
            source = replace_once(source, "    GetTaskbarRectForMonitor(monitor, &taskbarRect);", """    GetTaskbarRectForMonitor(monitor, &taskbarRect);
    const bool precise=LeanDockGeometry::OcclusionRect(hTaskbarWnd,monitor,taskbarRect);
    SetPropW(hTaskbarWnd,L"LeanDock.Hide.Precise",reinterpret_cast<HANDLE>(uintptr_t(precise+1)));
    // Diagnostics are scalar and title-free, written only on a real decision.
    const LONG coordinates[]={taskbarRect.left,taskbarRect.top,taskbarRect.right,taskbarRect.bottom};
    const wchar_t* names[]={L"LeanDock.Hide.Left",L"LeanDock.Hide.Top",L"LeanDock.Hide.Right",L"LeanDock.Hide.Bottom"};
    for(unsigned i=0;i<4;++i)SetPropW(hTaskbarWnd,names[i],reinterpret_cast<HANDLE>(intptr_t(coordinates[i])+1));""")
            source = replace_once(source, """    DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &windowRect,
                          sizeof(windowRect));""", """    if(FAILED(DwmGetWindowAttribute(hWnd,DWMWA_EXTENDED_FRAME_BOUNDS,&windowRect,sizeof(windowRect))) &&
       !GetWindowRect(hWnd,&windowRect))return false;""")
            source = replace_once(source, "        if (IntersectRect(&intersectRect, &windowRect, taskbarRect)) {",
                                  "        if (LeanDockGeometry::Overlaps(windowRect, *taskbarRect)) {")
            source = source.replace("        bool keepShown = ShouldKeepTaskbarShown(hWnd, monitor);", """        bool keepShown = ShouldKeepTaskbarShown(hWnd, monitor);
        SetPropW(hWnd,L"LeanDock.Hide.KeepShown",reinterpret_cast<HANDLE>(uintptr_t(keepShown+1)));
        auto decisions=reinterpret_cast<uintptr_t>(GetPropW(hWnd,L"LeanDock.Hide.Decisions"));
        SetPropW(hWnd,L"LeanDock.Hide.Decisions",reinterpret_cast<HANDLE>(decisions+1));""")
            source = replace_once(source, "std::atomic<HANDLE> g_winEventHookThread;", """std::atomic<HANDLE> g_winEventHookThread;
HANDLE g_eventStop;
std::atomic<bool> g_eventStopping{true};
std::atomic<LPARAM> g_eventGeneration{0};""")
            source = replace_once(source, "DWORD WINAPI WinEventHookThread(LPVOID lpThreadParameter);", """DWORD WINAPI WinEventHookThread(LPVOID lpThreadParameter);
void StopEventThread() {
    std::lock_guard guard(g_winEventHookThreadMutex);
    if(g_winEventHookThread) {
        LeanDockEvents::RequestStop(g_eventStop,GetThreadId(g_winEventHookThread));
        // Keep handles and the pinned image intact if native work is stuck.
        // Never terminate an Explorer thread or wait forever during Stop.
        if(!LeanDockEvents::WaitForExit(g_winEventHookThread,5000))
            throw std::runtime_error("hide event thread did not exit");
        CloseHandle(g_winEventHookThread);g_winEventHookThread=nullptr;
    }
    if(g_eventStop){CloseHandle(g_eventStop);g_eventStop=nullptr;}
}""")
            source = replace_function(source, "void AdjustTaskbar(HWND hMMTaskbarWnd, bool clearPendingWhenDone = false)", """    if(g_eventStopping)return;
    if(g_settings.mode!=Mode::never && !g_winEventHookThread) {
        std::lock_guard guard(g_winEventHookThreadMutex);
        if(g_eventStopping)return;
        if(!g_winEventHookThread) {
            g_eventStop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            if(!g_eventStop)return;
            g_winEventHookThread=CreateThread(nullptr,0,WinEventHookThread,g_eventStop,0,nullptr);
            if(!g_winEventHookThread){CloseHandle(g_eventStop);g_eventStop=nullptr;return;}
        }
    }
    PostMessage(hMMTaskbarWnd,g_updateTaskbarStateRegisteredMsg,
                clearPendingWhenDone,g_eventGeneration.load());""")
            source = replace_once(source,"bool AdjustAllTaskbarsIfNotPending() {", "bool AdjustAllTaskbarsIfNotPending() {\n    if(g_eventStopping)return true;")
            # Drop old-generation taskbar work after Stop/resume. Guard native
            # hooks too while Stop restores the auto-hide preference.
            for signature, original, args in (
                ("LRESULT WINAPI TrayUI_WndProc_Hook(", "TrayUI_WndProc_Original", "pThis,hWnd,Msg,wParam,lParam,flag"),
                ("LRESULT WINAPI CSecondaryTray_v_WndProc_Hook(", "CSecondaryTray_v_WndProc_Original", "pThis,hWnd,Msg,wParam,lParam"),
            ):
                opening=source.index("{",source.index(signature))+1
                source=source[:opening]+f"""
    if(g_eventStopping)return {original}({args});
    if(Msg==g_updateTaskbarStateRegisteredMsg && lParam!=g_eventGeneration.load())return 0;
    if(Msg==LeanDockGeometry::ChangedMessage())AdjustTaskbar(hWnd);
"""+source[opening:]
            opening=source.index("{",source.index("void CALLBACK WinEventProc("))+1
            source=source[:opening]+"""
    if(g_eventStopping)return;
    if(event==EVENT_SYSTEM_FOREGROUND){
        ObserveBackdropForeground(hWnd);
        if(!g_settings.foregroundWindowOnly)return;
    }
"""+source[opening:]
            source = replace_once(source, "    if (g_settings.foregroundWindowOnly) {\n        winSystemEventHook1 =", "    {\n        winSystemEventHook1 =")
            source = replace_once(source, "DWORD WINAPI WinEventHookThread(LPVOID lpThreadParameter) {", "DWORD WINAPI WinEventHookThread(LPVOID lpThreadParameter) {\n    g_backdropWasDesktop=LeanDockBackdrop::IsDesktop(GetForegroundWindow());")
            start=source.index("    BOOL bRet;",source.index("DWORD WINAPI WinEventHookThread(LPVOID lpThreadParameter) {"))
            end=source.index("    if (winObjectEventHook1)",start)
            source=source[:start]+"""    const auto result=LeanDockEvents::Pump(static_cast<HANDLE>(lpThreadParameter));
    // A nonzero stale timer would suppress all scheduling on the next run.
    LeanDockEvents::ClearTimer(g_pendingEventsTimer);

"""+source[end:]
            source = replace_once(source,"void Wh_ModAfterInit() {", """void Wh_ModAfterInit() {
    ++g_eventGeneration;g_eventStopping=false;""")
            source = replace_function(source,"void Wh_ModUninit()", """    g_eventStopping=true;++g_eventGeneration;
    StopEventThread();
    EnumWindows([](HWND window,LPARAM)->BOOL {
        DWORD owner=0;GetWindowThreadProcessId(window,&owner);
        if(owner==GetCurrentProcessId() && IsTaskbarWindow(window)) {
            for(auto key:{L"LeanDock.Hide.Precise",L"LeanDock.Hide.KeepShown",L"LeanDock.Hide.Decisions",L"LeanDock.Hide.Left",L"LeanDock.Hide.Top",L"LeanDock.Hide.Right",L"LeanDock.Hide.Bottom"})RemovePropW(window,key);
            ClearResumeState(window);
        }
        return TRUE;
    },0);
    if(g_wasAutoHideDisabled) {
        HWND taskbar=FindCurrentProcessTaskbarWnd();
        if(taskbar)SendMessage(taskbar,kHandleTrayPrivateSettingMessage,kTrayPrivateSettingAutoHideSet,FALSE);
    }""")
            # Settings are compile-time constants in this dedicated build, but
            # keep the retained upstream entry safe if ever invoked internally.
            source = replace_function(source,"BOOL Wh_ModSettingsChanged(BOOL* bReload)", """    *bReload=TRUE;return TRUE;""")
            # Clip handles normal hit-testing. Filter only the native mouse
            # hover timer, leaving keyboard, accessibility and menu Unhide
            # requests untouched even if the pointer is in an outside corner.
            source = source.replace("    if (Msg == WM_NCCREATE) {", """    if(Msg==WM_TIMER && wParam==kTrayUITimerUnhide && LeanDockGeometry::RejectPointerReveal(hWnd)) {
        KillTimer(hWnd,kTrayUITimerUnhide);return 0;
    }
    if (Msg == WM_NCCREATE) {""")
        if name == "size":
            source = replace_function(source, "HMODULE GetSearchUxUiModuleHandle()",
                                      "    return nullptr; // Search is disabled in the dedicated Dock profile.")
        if name == "tray":
            start = source.index("// Column-first ordering, shared by application icons")
            end = source.index("// Local extension to the upstream tray grid.", start)
            source = source[:start] + '#include "tray-grid-latest.h"\n\n' + source[end:]
            source = replace_once(
                source,
                "    if (!result.panel) return result;\n    EnumChildElements(result.panel,[&](FrameworkElement child) {",
                """    if (!result.panel) return result;
    if (!apps) {
        // Hide only the native IME content; its outer wrapper still supplies
        // the battery's required layout width on 26H2. Count no IME slots.
        // Preserve parents/visibility and restore both properties on Stop.
        DockSet(result.panel,UIElement::OpacityProperty(),0.0);
        DockSet(result.panel,UIElement::IsHitTestVisibleProperty(),false);
        return result;
    }
    EnumChildElements(result.panel,[&](FrameworkElement child) {""")
            source = replace_once(
                source,
                "        DockTranslate(battery,batteryX,row*(DockGrid::Row(appCount,0)-0.5));",
                """        // Center a lone final item in the full two-row height. Move the
        // entire native button, so glyph, hover and click bounds stay aligned.
        const double batteryY=(count%2 && appCount==count-1)
            ? 0.0 : row*(DockGrid::Row(appCount,0)-0.5);
        DockTranslate(battery,batteryX,batteryY);""")
            source = replace_once(source, "void UpdateUnifiedDockGrid() {", "void UpdateLegacyUnifiedDockGrid() {")
            source = replace_once(source, "void UpdateLegacyUnifiedDockGrid() {",
                read(ROOT / "src" / "tray-system-latest.inc") + "\nvoid UpdateLegacyUnifiedDockGrid() {")
            source = replace_once(source, "        auto input=DockCollect(root,false);",
                "        auto input=DockCollect(root,false);\n        auto system=DockCollectSystem(root);")
            source = replace_once(source, "        const int appCount=int(apps.icons.size());",
                "        const int appCount=int(apps.icons.size())+int(system.icons.size());")
            source = replace_once(source, "        DockArrangeGroup(apps,0,1,cell,pitch,row);",
                "        DockArrangeGroup(apps,0,1,cell,pitch,row);\n        DockArrangeGroup(system,int(apps.icons.size()),2,cell,pitch,row);")
            source = replace_once(source, "DockSet(battery,Controls::Grid::ColumnProperty(),2);",
                "DockSet(battery,Controls::Grid::ColumnProperty(),3);")
            source = source.replace("DockArrangeGroup(input,appCount+1,3,cell,pitch,row",
                "DockArrangeGroup(input,appCount+1,4,cell,pitch,row")
            source = replace_once(source, 'std::to_wstring(appCount)+\n            L",\\\"battery\\\":1',
                'std::to_wstring(apps.icons.size())+\n            L",\\\"system\\\":"+std::to_wstring(system.icons.size())+\n            L",\\\"battery\\\":1')
            source = replace_once(source, '        if(!first)state+=L",";\n        state+=DockRect(batteryGrid,root);',
                '        for(auto& item:system.icons){if(!first)state+=L",";first=false;state+=DockRect(item,root);}\n        if(!first)state+=L",";\n        state+=DockRect(batteryGrid,root);')
            source = replace_once(source, "void StopUnifiedDockGrid() {", "void ClearNativePowerHit();\nvoid StopUnifiedDockGrid() {\n    ClearNativePowerHit();")
            source = replace_once(source, "bool TryUnifiedDockGrid(FrameworkElement element) try {",
                read(ROOT / "src" / "tray-cards-latest.inc") + "\nbool TryUnifiedDockGrid(FrameworkElement element) try {")
        # Every functional unit shares one loader, configuration reader and hook manager.
        # Separate namespaces prevent accidental ODR collisions in upstream globals.
        if name != "styler":
            includes = list(dict.fromkeys(re.findall(r"(?m)^#include .+$", source)))
            source = re.sub(r"(?m)^#include .+\n", "", source)
            source = "\n".join(includes) + "\nnamespace ND" + name.title() + " {\n" + source + "\n}\n"
        source = "#define ND_COMPONENT " + str(component) + "\n" + \
            "#define WindhawkUtils ND" + name.title() + "Utils\n" + \
            '#include "native-api-latest.h"\n' + source
        namespace = "" if name == "styler" else "ND" + name.title() + "::"
        for old, new in [
            ("Wh_ModInit", "ComponentInit"), ("Wh_ModAfterInit", "ComponentAfterInit"),
            ("Wh_ModBeforeUninit", "ComponentBeforeUninit"),
            ("Wh_ModUninit", "ComponentUninit"), ("Wh_ModSettingsChanged", "ComponentSettingsChanged")
        ]:
            source = re.sub(r"\b" + old + r"\b", name.title() + new, source)
        source += f'\nextern "C" BOOL Native_{name}_init() {{ return {namespace}{name.title()}ComponentInit(); }}\n'
        source += f'extern "C" void Native_{name}_after() {{ {namespace}{name.title()}ComponentAfterInit(); }}\n'
        source += f'extern "C" void Native_{name}_stop() {{ {namespace}{name.title()}ComponentUninit(); }}\n'
        if name in ("tray", "size"):
            source += f'extern "C" void Native_{name}_before() {{ {namespace}{name.title()}ComponentBeforeUninit(); }}\n'
            source += f'extern "C" void Native_{name}_resume() {{ {namespace}g_unloading=false; {namespace}{name.title()}ComponentAfterInit(); }}\n'
        if name == "styler":
            source += '\nextern "C" unsigned Native_styler_count() { unsigned count=0; HWND w=GetTaskbarUiWnd(); if(w) RunFromWindowThread(w, [](void* p){ *static_cast<unsigned*>(p)=static_cast<unsigned>(g_elementsCustomizationState.size()); }, &count); return count; }\n'
        if name == "tray":
            source += '\nextern "C" void Native_tray_refresh_cards() { NDTray::UpdateUnifiedDockGrid(); }\n'
        if name == "hide":
            source += '\nextern "C" int Native_hide_expanded(HWND window) { return NDHide::ReadExpandedState(window,L"AtInput"); }\n'
            source += '''\nextern "C" unsigned long long Native_hide_attention(unsigned key) {
    switch(key) {
        case 0:return !NDHide::g_eventStopping.load();
        case 1:return NDHide::g_attentionBlockedReveals.load();
        case 2:return NDHide::g_attentionIgnoredFlashing.load();
        case 3:return NDHide::g_attentionPassedReveals.load();
        default:return 0;
    }
}\n'''
        # Full origin/license information remains in upstream and NOTICE.
        source = "// Derived from the preserved GPL-3.0 upstream; see NOTICE.md.\n" + source
        emit(ROOT / "src" / (name + "-latest.cpp"), source)
        sources.append({"component": name, "sourceLines": len(source.splitlines()),
                        "upstreamSha256": hashlib.sha256((ROOT / "upstream" / (name + ".cpp")).read_bytes()).hexdigest()})

    settings = json.loads(read(ROOT / "baseline" / "settings-latest.json"))
    defaults = []
    ini = ["; NativeTaskbarDock 0.1.0. Validated appearance and behavior are compiled in.", "[runtime]", "Logging=0", "Diagnostics=0", ""]
    for component, name in enumerate(NAMES, 1):
        values = next(x["Settings"] for x in settings if x["Id"] == IDS[component - 1])
        if name == "styler":
            values = {"clickThroughTaskbar": 1, "xamlDiagnosticsHandling": "block"}
        if name == "tray":
            values["debugLayout"] = 0
        for key, value in sorted(values.items()):
            defaults.append(f"    {{{component}, {wide(key)}, {wide(value)}}},")
    emit(include / "settings-generated.h",
         "#pragma once\nstruct NativeSetting { int component; const wchar_t* key; const wchar_t* value; };\n"
         "static const NativeSetting NativeDefaults[] = {\n" + "\n".join(defaults) + "\n};\n")
    if not (ROOT / "latest.ini").exists():
        emit(ROOT / "latest.ini", "\n".join(ini))

    modules = json.loads(read(ROOT / "baseline" / "modules-latest.json"))
    caches = json.loads(read(ROOT / "baseline" / "symbols-latest.json"))
    symbols = {}
    manifests = []
    for module in modules:
        data = Path(module["Path"]).read_bytes()
        assert hashlib.sha256(data).hexdigest().upper() == module["Sha256"]
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        timestamp = struct.unpack_from("<I", data, pe + 8)[0]
        image_size = struct.unpack_from("<I", data, pe + 24 + 56)[0]
        manifests.append("    {" + ", ".join([
            wide(module["Name"].lower()), wide(module["Path"]), wide(module["Sha256"]),
            str(timestamp) + "u", str(image_size) + "u"]) + "},")
    for cache in caches:
        parts = cache["Value"].split("#")
        assert parts[0] == "1" and len(parts) % 2 == 1, cache["Key"]
        module_name = parts[1].lower()
        stamp, image_size = map(int, parts[2].split("-"))
        module = next(x for x in modules if x["Name"].lower() == module_name)
        image = Path(module["Path"]).read_bytes()
        pe = struct.unpack_from("<I", image, 0x3C)[0]
        assert struct.unpack_from("<I", image, pe + 8)[0] == stamp
        assert module["ImageBytes"] == image_size
        for i in range(3, len(parts), 2):
            if not parts[i + 1]:
                continue
            rva = int(parts[i + 1])
            assert 0 < rva < image_size
            key = (module_name, parts[i])
            assert key not in symbols or symbols[key] == rva
            symbols[key] = rva
    lines = [f"    {{{wide(module)}, {wide(symbol)}, {rva}u}}," for (module, symbol), rva in sorted(symbols.items())]
    emit(include / "symbols-generated.h",
         "#pragma once\nstruct NativeModulePin { const wchar_t* name; const wchar_t* path; const wchar_t* sha256; unsigned timestamp; unsigned imageSize; };\n"
         "static const NativeModulePin NativeModules[] = {\n" + "\n".join(manifests) + "\n};\n"
         "struct NativeSymbol { const wchar_t* module; const wchar_t* name; unsigned rva; };\n"
         "static const NativeSymbol NativeSymbols[] = {\n" + "\n".join(lines) + "\n};\n")
    emit(ROOT / "build" / "sources-latest.json",
         json.dumps({"components": sources, "symbolCount": len(symbols), "pinnedModules": len(modules)}, indent=2))
    # Optional card adapter: an unknown input build must not disable core Dock.
    input_module = json.loads(read(ROOT / "baseline" / "cards-input-latest.json"))
    data = Path(input_module["Path"]).read_bytes()
    assert hashlib.sha256(data).hexdigest().upper() == input_module["Sha256"]
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    timestamp = struct.unpack_from("<I", data, pe + 8)[0]
    image_size = struct.unpack_from("<I", data, pe + 24 + 56)[0]
    assert image_size == input_module["ImageBytes"]
    emit(include / "cards-input-pin-generated.h",
         '#pragma once\n#include "symbols-generated.h"\n'
         'static const NativeModulePin NativeCardsInputModule = {' + ', '.join([
             wide(input_module["Name"].lower()), wide(input_module["Path"]), wide(input_module["Sha256"]),
             str(timestamp) + 'u', str(image_size) + 'u']) + '};\n')
    print(json.dumps({"components": len(sources), "symbols": len(symbols), "pinnedModules": len(modules)}))


if __name__ == "__main__":
    main()
