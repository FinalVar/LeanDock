"""Build-only adapter for the retained native companions.

The upstream-derived runtime stays in src/include. All adaptations are generated
inside capsule/build, so the verified 0.8 branch and others' edits remain intact.
Every literal edit is anchored and input hashes are retained for drift detection.
"""
from pathlib import Path
import hashlib
import json
import re
import shutil

CAP = Path(__file__).resolve().parent
ROOT = CAP.parent
OUT = CAP / "build" / "companion"
SOURCE = OUT / "source"
inputs = {}
for folder in ("src", "include", "hotcorners", "vendor/minhook"):
    for p in (ROOT / folder).rglob("*"):
        if p.is_file() and p.suffix in (".h", ".inc", ".cpp", ".c"):
            rel = p.relative_to(ROOT)
            inputs[str(rel).replace("\\", "/")] = hashlib.sha256(p.read_bytes()).hexdigest().upper()
            dest = SOURCE / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(p, dest)
for name in ("companion-host-latest.h", "companion-shell-latest.h"):
    shutil.copyfile(CAP / name, SOURCE / "include" / name)
shutil.copyfile(CAP / "companion-stubs-latest.cpp", SOURCE / "src" / "companion-stubs-latest.cpp")

def load(rel):
    return (SOURCE / rel).read_text(encoding="utf-8-sig")

def save(rel, text):
    (SOURCE / rel).write_text(text, encoding="utf-8", newline="\n")

def one(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError(f"Companion anchor drifted ({text.count(old)}): {old[:120]}")
    return text.replace(old, new, 1)

for rel in ("src/controller-latest.cpp", "src/hotcorners-latest.cpp",
            "include/client-edge-latest.h", "include/switcher-visibility-latest.h"):
    text = load(rel)
    text, n = re.subn(r'FindWindowW\(L"Shell_TrayWnd",\s*nullptr\)', "LeanDockNativeShell()", text)
    if not n:
        raise RuntimeError(f"Missing real-shell adapter: {rel}")
    save(rel, '#include "companion-shell-latest.h"\n' + text)

text = load("src/styler-latest.cpp")
a = text.index("void ProcessAllStylesFromSettings() {")
b = text.index("    g_resourceVariables.clear();", a) + len("    g_resourceVariables.clear();")
text = text[:a] + "void ProcessAllStylesFromSettings() {\n    LeanDockSwitcher::AddRules();\n    g_resourceVariables.clear();" + text[b:]
for old in ("    LeanDockIconView::Attach(elementId, element);",
            "    EnsureTaskbarSurfaceSubclasses();", "    InstallLeanDockMenuHook();"):
    text = one(text, old, "    // Companion profile: original Dock surface path disabled.")
text = one(text, "if (g_initializedForThread && IsTaskbarTopLevelWindow(hWnd))",
           "if (false && g_initializedForThread && IsTaskbarTopLevelWindow(hWnd))")
text = one(text, "g_settings.clickThroughTaskbar = Wh_GetIntSetting(L\"clickThroughTaskbar\");",
           "g_settings.clickThroughTaskbar = false;")
save("src/styler-latest.cpp", text)

text = load("src/runtime-latest.cpp")
text = one(text, '\\"version\\":\\"0.5.63\\"', '\\"version\\":\\"0.6.0-capsule\\"')
text = one(text, "for(unsigned i: {2u,1u,3u,4u,0u})",
           "for(unsigned i: {3u,0u})")
text = one(text, "if(g_state==NativeActive)elements=Native_styler_count();",
           "if(g_state==NativeActive){Native_hide_after();elements=Native_styler_count();}")
save("src/runtime-latest.cpp", text)

text = load("src/hide-appbar-latest.inc")
text = one(text, "return pid==GetCurrentProcessId() && LeanDockGeometry::Read(window,dock);",
           "return pid==GetCurrentProcessId() && window==LeanDockNativeShell();")
text = one(text, "bool AppBarReady(){return g_appBarHookInstalled && !g_eventStopping && g_wasAutoHideProcessed && ReadyEdgeController();}",
           """bool AppBarReady(){
    HWND ui=FindWindowW(L"NativeTaskbarDock.Capsule.v1",nullptr);
    return g_appBarHookInstalled && !g_eventStopping && ReadyEdgeController() &&
        ui && GetPropW(ui,L"Capsule.Ready");
}""")
text = re.sub(r'FindWindowW\(L"Shell_TrayWnd",\s*nullptr\)', "LeanDockNativeShell()", text)
save("src/companion-appbar-generated.inc", text)

text = load("include/panel-position-latest.h")
text = one(text, "    for(auto cls:{L\"Shell_TrayWnd\",L\"Shell_SecondaryTrayWnd\"}){",
           """    HWND capsule=FindWindowW(L"NativeTaskbarDock.Capsule.v1",nullptr);
    if(capsule && GetPropW(capsule,L"Capsule.Ready") &&
       MonitorFromWindow(capsule,MONITOR_DEFAULTTONEAREST)==monitor &&
       LeanDockGeometry::Read(capsule,dock,&info))return true;
    for(auto cls:{L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"}){""")
save("include/panel-position-latest.h", text)

text = load("src/controller-latest.cpp")
text = '#include "companion-host-latest.h"\n' + text
text = one(text, "    LeanDockStart::Stop();", "    LeanDockSearchHost::Stop();\n    LeanDockStart::Stop();")
text = one(text, "    LeanDockStart::Start(g_directory);", "    LeanDockStart::Start(g_directory);\n    LeanDockSearchHost::Start(g_directory);")
text = one(text, "    const bool appBarReady=", "    size+=LeanDockSearchHost::Status(json+size,sizeof(json)-size);\n    const bool appBarReady=")
text = one(text, '\\"version\\":\\"0.3.43\\"', '\\"version\\":\\"0.4.0-capsule\\"')
text = one(text, "WaitForSingleObject(process,15000)", "WaitForSingleObject(process,30000)")
text = one(text, '    if(count==2&&!wcscmp(args[1],L"--cards"))',
           '    if(count==2&&!wcscmp(args[1],L"--capsule-token-probe")){LocalFree(args);return LeanDockCompanion::Probe(g_directory);}\n'
           '    if(count==2&&!wcscmp(args[1],L"--cards"))')
text = one(text, "    g_stopping=true;KillTimer(g_window,1);CancelHelper();",
           "    g_stopping=true;KillTimer(g_window,1);CancelHelper();\n    LeanDockCompanion::Stop(g_window);")
text = one(text, "if(TryConnect()){KillTimer(w,1);return 0;}",
           """if(TryConnect()){
            KillTimer(w,1);
            if(!LeanDockCompanion::attempted){
                SetPropW(w,L"LeanDock.Companion.v1",reinterpret_cast<HANDLE>(uintptr_t(GetCurrentProcessId())));
                if(!LeanDockCompanion::Launch(g_directory)){
                    _snwprintf_s(g_error,_TRUNCATE,L"Normal-user capsule launch failed (%lu)",LeanDockCompanion::lastError);
                    SaveStatus();g_exitCode=27;PostMessageW(w,WM_CLOSE,0,0);
                }else if(!LeanDockCompanion::Watch(w)){
                    wcscpy_s(g_error,L"Cannot observe capsule lifetime");g_exitCode=28;PostMessageW(w,WM_CLOSE,0,0);
                }else SetTimer(w,LeanDockCompanion::Timer,500,nullptr);
            }
            return 0;
        }""")
text = one(text, "    if(message==WM_TIMER && p==1) {",
           """    if(message==LeanDockCompanion::Exited && p==LeanDockCompanion::childPid){
        KillTimer(w,LeanDockCompanion::Timer);
        unsigned state=NativeIdle;if(HWND backend=Backend(g_explorerPid))QueryBackend(backend,&state);
        SaveStatus();return 0;
    }
    if(message==WM_TIMER && p==LeanDockCompanion::Timer){
        HWND ui=FindWindowW(L"NativeTaskbarDock.Capsule.v1",nullptr);
        if((ui && GetPropW(ui,L"Capsule.Ready")) ||
           (LeanDockCompanion::child && WaitForSingleObject(LeanDockCompanion::child,0)!=WAIT_TIMEOUT)){
            unsigned state=NativeIdle;if(HWND backend=Backend(g_explorerPid))QueryBackend(backend,&state);
            KillTimer(w,LeanDockCompanion::Timer);SaveStatus();
        }
        return 0;
    }
    if(message==WM_TIMER && p==1) {""")
text = one(text, '    char json[4096],error[1200];',
           '    char json[4096],error[1200];')
text = one(text, '    HWND taskbar=LeanDockNativeShell();\n',
           '    size+=snprintf(json+size,sizeof(json)-size,"\\"companionOnly\\":true,\\"capsuleGuardPid\\":%lu,\\"capsuleLaunchError\\":%lu,",LeanDockCompanion::childPid,LeanDockCompanion::lastError);\n'
           '    HWND taskbar=LeanDockNativeShell();\n')
save("src/controller-latest.cpp", text)

# Loading excluded original components would violate the handoff boundary.
if "Taskbar.TaskbarFrame" in load("src/styler-latest.cpp").split(
        "void ProcessAllStylesFromSettings() {", 1)[1].split("g_resourceVariables.clear();", 1)[0]:
    raise RuntimeError("Old Dock rules leaked into the companion profile")
if "for(unsigned i: {3u,0u})" not in load("src/runtime-latest.cpp"):
    raise RuntimeError("Companion startup contains original surface modules")

for p in CAP.glob("companion-*"):
    if p.is_file():
        inputs["capsule/" + p.name] = hashlib.sha256(p.read_bytes()).hexdigest().upper()
(OUT / "inputs-latest.json").write_text(json.dumps(inputs, indent=2), encoding="utf-8")
print(f"Prepared companion snapshot: {len(inputs)} inputs; old surface modules excluded")
