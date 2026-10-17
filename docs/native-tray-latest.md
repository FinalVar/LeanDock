# Native status icons in the mixed tray

Version 0.8.17 extends the retained upstream notification-grid adaptation to
the native MainStack. It does not manufacture permission indicators or
monitor applications to infer privacy state. Windows still owns the icon,
view model, visibility binding, tooltip and input handlers.

## Scope

Native microphone/privacy, location, wireless-display and other independent
system indicators can share the existing two-row app tray when Windows supplies
them. Existing application notification icons (including USB, Bluetooth,
printing, sync or security applications when present) keep their existing host.
No duplicate icon is added. Camera/recording apps without a native tray icon do
not gain a custom one.

The network/volume/power quick-settings group remains hidden at its original
position; its existing battery-card input mapping is unchanged. Notification
center, extra clocks, input-language controls, touch keyboard and pen/input
panels remain excluded. Dock auto-hide behavior is unchanged.

## Implementation and restoration

The generator keeps the original five upstream components and lifecycle hooks.
tray-system-latest.inc collects native system presenters alongside app
presenters. Both groups use the same column-first geometry and share a column
when the preceding group has an odd count. A single final item is centered
vertically. An empty group contributes zero width. The crowded-card fallback
also accounts for the native system group.

Controls are neither reparented nor replaced. Input exclusions use native
control/model class names present in the pinned SystemTray binary, not localized
tooltip text. If a native presenter is reused with a different model, the
filter restores its saved visibility binding before collecting it. All modified
properties use the existing binding-aware restoration owner on Stop. Existing
Loaded/LayoutUpdated callbacks handle changes; no new hooks, timers, monitor
processes or status-detection APIs were added.

The layout diagnostic includes separate app/system counts and actual rectangles.
Its system type inventory contains class names only, not permission consumers,
app titles or tooltip text. Unknown system controls stay native and visible;
this avoids silently discarding new privacy indicators after template changes.

## Validation boundaries

The package builds with the independent toolchain. Compile-time checks cover
65 application counts by 13 system counts, group parity, final-item centering
and hit-target extents, using the same production layout header. Executing the
new grid test executable was blocked by Windows application control; the
security policy was not changed. This is recorded separately from successful
compile-time checks.

Live deployment, actual native icon arrival/removal, mouse input and current
configuration preservation are recorded in verification-latest.json. A build
or native-host ready state alone is not end-to-end permission-icon acceptance.
USB removal, Bluetooth, printing, sharing and actual microphone/location use
require their respective real scenarios; absent scenarios are not marked passed.

On 2026-10-11 the new backend loaded successfully and QuickPosition stayed
active. The unchanged StartLayout DLL was rejected by VerifiedAndReputableDesktop
(Code Integrity event 3077); the controller reports StartLayout unsupported and
inactive. Its SHA-256 is identical to the pre-update file. Main-backend readiness
does not establish companion readiness. The two runtime INI files were preserved.
At observation there were three application icons and no active MainStack icon;
the two-column geometry was unchanged. Further executable fixtures were stopped
after the policy rejection. No protection, trust or exclusion settings were changed.

Later on the same day, after the user manually cycled SAC Off and back On,
the current 0.8.18 files passed fresh loading with VerifiedAndReputableDesktop
actually enforced. All three native hosts were restarted at 17:34–17:35; Start
layout and Quick Settings recovered with ComponentHealth Ready. The agent did
not change protection, trust or exclusion settings. This supersedes the earlier
current-runtime failure state, not the incident record. It does not validate an
absent native permission icon or explain Windows reputation decisions. Detailed
hashes, host changes and separate input acceptance remain in verification-latest.json.

The stopped four-finger transparency investigation remains unresolved. This
package includes the already-built 0.8.16 removal of ineffective gesture hooks;
it does not resume that investigation or claim a gesture fix.
