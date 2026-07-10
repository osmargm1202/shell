# VPN Service C++ Port — Design Spec

**Date:** 2026-07-10
**Goal:** Port `services/VPN.qml` (484 lines, multi-provider VPN status/connect orchestration in JS) to a C++ `VpnCore` in the plugin, same recipe as the Nmcli port. Thin facade, API identical.

## Public API contract

Sole consumer today: `modules/utilities/cards/Toggles.qml` using `connected`, `status.state`, `connecting`, `toggle()`. Facade keeps the fuller minimal surface for parity: `connected` (bool), `status` (var map `{connected, state, reason, authUrl}`), `connecting` (bool), `enabled` (bool), `connect()`, `disconnect()`, `toggle()`, `checkStatus()`.

`status.state` values: `disconnected | connecting | connected | needs-auth | error`.

## Architecture

- `VpnCore` — `QML_ELEMENT` non-singleton, instantiated in the `services/VPN.qml` facade.
- **UI-agnostic core**: no Toaster, no GlobalConfig reads in C++. Facade passes provider config in (property bound from `GlobalConfig.utilities.vpn.provider`) and listens to a `statusChanged`-driven signal to emit toasts (keep the exact toast-on-state-transition logic + `GlobalConfig.utilities.toasts.vpnChanged` gate in the facade QML).
- **Share the nmcli monitor**: do NOT spawn a second `nmcli monitor`. Add a public signal to `NmcliCore` (e.g. `connectionEvent()`) emitted from its existing debounced refresh; expose the core instance from the `Nmcli.qml` facade (`readonly property NmcliCore core: core` or similar); `VPN.qml` facade wires `Connections { target: Nmcli.core }` → `VpnCore.scheduleStatusCheck()`. Inherits the crash-restart fix for free and drops one persistent process.

## Behavioral requirements (from 2026-07-10 analysis)

1. **Provider chain**: config list objects `{enabled, name, interface, connectCmd, disconnectCmd, displayName}`; first enabled provider wins; built-in defaults for wireguard (pkexec wg-quick up/down <iface>), tailscale (tailscale up/down), netbird (netbird up --no-browser/down), warp (warp-cli connect/disconnect); unknown name → `[name, "up"/"down"]`; custom fields override per-field.
2. **Status commands + parsers** (all with `LANG/LC_ALL=C.UTF-8` — and apply it to connect/disconnect procs too, deviation from QML which only set it on status): tailscale `status --json` (BackendState mapping, NeedsLogin/NeedsMachineAuth → needs-auth + AuthURL, "Logged out"/"Stopped"/"not running" pre-checks), netbird `status --json` (management.connected && signal.connected; auth|login in management.error → needs-auth), warp `warp-cli status` plain text, wireguard/default `ip link show` contains `<iface>:`.
3. **Warp matching**: use word-boundary matching in C++ but KEEP the original precedence (Connected > Connecting > registration keywords > Disconnected > error). Correction 2026-07-10 review: the initially-claimed "Disconnected contains Connected" bug never existed (JS `includes` is case-sensitive; the embedded substring is "connected"), and checking Disconnected first regresses auto-registration — warp prints "Status update: Disconnected. Reason: Registration Missing." and the registration keywords must win.
4. **stderr heuristics**: service-not-running patterns → `Service not running (run: sudo systemctl start <unit>)` with unit map (tailscaled/netbird/warp-svc/<name>d); connect stderr: "Access denied" → operator hint reason; "Unknown device type"/"Protocol not supported" → modprobe hint; auth URL regex `(https?://\S+)` on connect stdout lines → immediate needs-auth with authUrl; "already exists" → treat as connected but normalize through the status-update path (fix the QML desync where `connected` and `status.connected` diverged).
5. **Debounce**: single-shot 500ms status-check timer; triggers: startup (if enabled), shared monitor event, connect proc exit 0 (queued invocation so pending stdout auth-URL lines process first — QMetaObject::invokeMethod QueuedConnection), disconnect exit, warp-register exit 0, provider change (also reset status to disconnected).
6. **Warp auto-register**: only on state TRANSITION into needs-auth with "registration" in reason, and never while the register proc runs (QML re-spawned per poll — fix).
7. **authUrl stickiness**: preserve previous authUrl when new needs-auth status lacks one.
8. **Toast logic** (facade): toast only on `state` transition; "error" toast only if previous state was connected/connecting/needs-auth; gated on `GlobalConfig.utilities.toasts.vpnChanged`.
9. **Security**: keep `pkexec` exactly (interactive polkit agent); no sudo substitution. No secrets in argv here.
10. Logging category `caelestia.services.vpn`.

## Non-goals

- No new providers, no UI changes, no provider config schema changes.

## Phases

1. NmcliCore: add `connectionEvent()` signal + facade exposure.
2. VpnCore C++ + CMakeLists.
3. Facade VPN.qml (config binding + toasts + monitor wiring).
4. Build clean; combined review with Clipboard/Emojis port; ship.
