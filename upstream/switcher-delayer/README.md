# Display gate reference

L3r0y, Alt+Tab window delayer 1.2.0, ramensoftware/windhawk-mods commit b07c7e0d65abbbf7d10547608004595ce07189e0. Original source retained unmodified in latest.cpp. The repository policy assigns MIT to mods without explicit license metadata; retained in license-policy.md.

LeanDock adapts the native DisplayAltTab scope and ShowWindow hook boundary only. DWM cloak ownership, exact binary gate, single-use request, native uncloak interception, thread-pool watchdog, Stop/resume cleanup and diagnostics are local additions. The original's global delay, legacy Win10/ARM64 branches, direct timer ShowWindow, and wrong void ViewLoaded signature are not copied. No runtime dependency on Windhawk is added.
