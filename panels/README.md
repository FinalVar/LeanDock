# Quick Settings position companion

`latest.dll` is built from `src/quick-position-latest.cpp`; it is a local build
artifact, not checked into Git. Build with `build/panels-latest.ps1` or a full /
controller build. It uses only Windows system DLLs and is not a standalone app.

The existing controller verifies the system hosts and loads it into the exact
ControlCenterWindow thread. Its only export is the temporary Windows hook
entrypoint; its fixed maintenance messages enable, disable and query this one
placement feature. There is no key, shell command or arbitrary action endpoint.

See `docs/cards-latest.md` for placement, lifecycle and recovery. Normal Stop
removes its subclass and owner wait. Executable callbacks stay mapped until
ShellHost exits, so do not overwrite the production DLL while that host has it
loaded. Updating the controller alone does not require exiting ShellHost.
