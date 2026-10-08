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
`-ValidateOnly` verifies paths, all payload hashes, task ownership and the native
compatibility check without altering the installed copy. Existing latest.ini
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

A successful live readiness check commits the transaction. Failures restore
the previous exact files and running state. A hidden, temporary watchdog can
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

Actual update, rollback and resource scenario results belong in
verification-latest.json. First install on a separate clean machine, battery
power, multi-monitor and abrupt machine power loss must not be claimed from
these tests. A checksum is integrity evidence, not a publisher signature.
