# 0.7.0 Maintenance and resource baselines

The same management entry supports Status, Start, Stop, Resources, Update and
Rollback. Install with a PackagePath installs a complete package into a new,
empty Destination. The legacy Install without PackagePath only registers the
task in an already populated deployment. No installer, measurement script or
watchdog remains running during normal use.

## Package and update

After a successful build, run build/package-latest.ps1 with a new output folder.
The folder contains the four runtime binaries, source, licenses, build/test and
maintenance tools, project AGENTS.md, default configurations, and a SHA-256 manifest. The compiler,
Windows DLLs, credentials and old runtime status files are not bundled. This
remains a machine/component-pinned package, not universal Windows support.

Build/test entry points resolve the standalone compiler through
build/toolchain-latest.ps1. CompilerRoot overrides the process environment, then
the user-level LEANDOCK_COMPILER_ROOT is used when neither is set. Windhawk is
not a fallback install location. See toolchain-latest.md for provenance,
independent-build evidence and the separation of runtime, build and rollback
materials.

Use `latest.ps1 -Action Update -PackagePath <package> -Destination <installed>`.
`-ValidateOnly` verifies paths, all payload hashes and task ownership without
altering the installed copy. For a runtime change it also checks the full
incoming/recovery runtime sets before running the native compatibility helper.
Source-only updates do not execute that helper. Existing latest.ini
and hotcorners/latest.ini are always preserved. No third-party service is
silently disabled. A different existing NativeTaskbarDock task is a hard error.

Updates take an exclusive current-user maintenance mutex, snapshot every
changed file, then stop the exact controller normally. Runtime updates cleanly
reload the taskbar-owning Explorer because diagnostics hot reconnect is not
proven reliable. Other hosts restart only for changed DLLs actually loaded
from this deployment, with PID, process
creation time, full image path and session checked. The native taskbar owner is
selected through Shell_TrayWnd, not every explorer.exe process. Source-only
updates do not restart a running Dock.

A successful check of the previous component readiness baseline commits the
transaction. Live alone covers only Dock and hot corners. Start support,
attachment/layout and Quick Settings support, attachment/theme are checked
separately when they were present before the update, with a bounded 15-second
wait for delayed attachment. Failed restoration must name the missing component;
it cannot report success from core readiness alone. New installs additionally
require supported Start and Quick Settings components. Hosts that have not yet
opened remain explicitly pending, not visually accepted.

Failures restore the previous exact files and running state when Windows still
permits the recovery set to load. A hidden, temporary watchdog can
recover if the maintenance parent exits before commit; it never races a still
running parent. A parent that remains stuck for five minutes produces a deferred
recovery record and requires inspection instead of being forcibly killed.

`latest.ps1 -Action Rollback` uses the most recent receipt backup; BackupPath
selects a specific transaction under this deployment's backups directory.
Introduced files are removed only if still byte-identical to this transaction.
Edited new files block rollback rather than being deleted. Config files and
unrelated data are preserved. All rollback files are checked before restoration.
Older pre-0.7.0 backups retain their own restore scripts; the unified rollback
entry requires a 0.7.0 transaction manifest.

## Application control and companion preservation

The shared controller currently stops all companions during a runtime update,
even for a change only to latest.dll. This is not component-isolated reloading.
On 2026-10-11 it unloaded a previously functioning Start companion whose bytes
were unchanged; Windows then rejected it under VerifiedAndReputableDesktop.
The former core-only readiness check incorrectly accepted that update.

Maintenance now reads Smart App Control state and checks the signatures of all
four runtime files, including unchanged Start and Quick Settings dependencies,
and of the recovery set, before executing the compatibility helper or stopping
anything. Under enforced SAC it conservatively refuses an unsigned/invalid
runtime set. Unknown policy state also refuses a runtime restart. This is a
maintenance safety gate, not a reproduction of Windows reputation assessment:
an unsigned file might previously have been accepted, and a valid signature
alone does not replace actual Windows loading and component readiness checks.
Off and Evaluation are distinguished from On. No code changes security policy,
trust stores, exclusions or application identities.

Rollback applies the same admission gate before stopping current components.
If policy changes during an update, recovery may require attention; the tool
does not bypass the denial to manufacture a successful rollback. Old transaction
manifests without a component baseline retain the legacy core-only check and
cannot establish historical companion readiness.

Status and receipts now include ComponentHealth (Ready, WaitingForHost,
Degraded or Stopped) with missing and pending components. Applied describes file
transaction completion; Live describes core runtime readiness. Neither overrides
ComponentHealth or the separate mouse, layout and animation acceptance record.
Updating only scripts/docs preserves existing runtime processes and an existing
Degraded state; it does not imply the blocked Start companion was restored.

The user explicitly set the final acceptance condition on 2026-10-11: Smart App
Control must be On again, its VerifiedAndReputableDesktop policy must actually
be enforced, and the component must still load afresh and work in its native
UI. Success observed only with protection Off or in a previously loaded process
does not satisfy this condition. A temporary Off/On diagnostic does not confer
trust, authorize permanent disablement, or establish a completed repair.

At 2026-10-11 16:56 (Asia/Shanghai), after the user manually turned SAC Off,
the existing Quiesce/Start lifecycle restored all unchanged components.
ComponentHealth was Ready; the Start host actually loaded the pinned Start
DLL. All four runtime hashes were unchanged and Defender real-time protection
remained enabled. This is a temporary recovery record, not SAC-On acceptance.
No trust-store, application-control policy or exclusion was changed by the
recovery script. The enforced-policy/fresh-load/UI checks remain mandatory.

At 2026-10-11 17:29 the user had re-enabled SAC. Readback confirmed On,
VerifiedAndReputableDesktop enforced, and Defender real-time protection enabled.
An ordinary `LoadLibraryExW` in a fresh process accepted the unchanged Start DLL.
At 17:34–17:35 a separately authorized, no-file-change diagnostic first checked
the four pinned file hashes, the owned task and all three DLLs with the ordinary
loader, then reused Quiesce/Start to cold-restart Explorer, StartMenuExperienceHost
and ShellHost. Their PIDs changed, the controller restarted, and all companions
recovered to ComponentHealth Ready. Enforcement and exact hashes were checked
again afterward. No project-matching Code Integrity denial occurred between
17:34:41 and 17:36:05. The Start layout reported 45 items, 8 columns, 6 rows, one
locked selector and zero failures. At 17:38 the user confirmed the compact Start
layout, launching an application and opening Quick Settings through the battery
card were all normal. Readback also confirmed the Quick Settings theme tile was
installed and ready, with zero failures; theme switching was not repeated.

This diagnostic does not relax the update/rollback signature gate, grant trust,
or generalize admission to future builds. It used the existing deployed paths
and normal Windows loader; no file identity, trust-store, security setting,
policy or exclusion was changed. The underlying reason for the earlier denial
and current admission is not established. Current exact-hash admission and
fresh-host readiness are separate from native UI acceptance.

Microsoft documents that SAC has no per-app allow exception and advises valid
trusted signing for app developers:
https://support.microsoft.com/en-us/windows/security/threat-malware-protection/smart-app-control-frequently-asked-questions
https://learn.microsoft.com/en-us/windows/apps/develop/smart-app-control/code-signing-for-smart-app-control

## Resources

`latest.ps1 -Action Resources -Scenario Visible -DurationSeconds 30` records a
bounded sample and exits. Scenario supports Visible, Hidden, Media, Battery,
Observe and Native. Each run includes actual card visibility/page and AC state;
an unmet scenario is reported as unobserved, not passed. Native requires the
controller to be stopped. No artificial unplugging or fake playback is used.

CPU is reported as both total-machine percentage and one-core percentage.
Working set and private committed bytes are separate and must not be added.
Controller, Explorer, Start and Quick Settings have separate rows. Host totals
include Windows and other shell work; they do not directly measure injected
LeanDock overhead. A native-host baseline and a settled attached-host sample
provide an observed difference with workload/caching limitations, not proof of
exclusive attribution.

Thread context switches use Windows raw performance counters, matched by
process/thread IDs across the sampling interval. Exited/new threads are not
silently treated as zero, and the result is a lower bound. These are scheduling
counts, **not hardware wakeups**. True wakeup and power attribution remain
unavailable without a separate ETW power investigation. Battery-life claims are
not inferred from CPU counters. Battery runs require real battery power.

Pass BaselinePath to compare matching scenario, OS build, processor count, AC
state and card page. Deltas are evidence, not automatic pass/fail thresholds.
Keep the desktop workload quiet; compiler/build work invalidates a clean idle
comparison. The observer itself adds overhead. Raw samples allow later review.

## Validation and limits

maintenance-latest.ps1 tests malformed/escaping paths, junction rejection,
manifest completeness, payload tampering, duplicate entries, exact restoration,
preserved config and refusal to delete modified introduced files. They mutate
only test fixtures, not Windows tasks or the real desktop.

The maintenance regression also covers unchanged unsigned companions, unknown
policy state, exact recovery-file inventory, a lost Start layout despite Live,
lost Quick Settings/theme, blocked-host stale flags and delayed companion
readiness. It does not launch native test programs or modify Windows protection.

Actual update, rollback and resource scenario results belong in
verification-latest.json. First install on a separate clean machine, battery
power, multi-monitor and abrupt machine power loss must not be claimed from
these tests. A checksum is integrity evidence, not a publisher signature.
