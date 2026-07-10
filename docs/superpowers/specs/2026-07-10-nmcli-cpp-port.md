# Nmcli C++ Port — Design Spec

**Date:** 2026-07-10
**Goal:** Move `services/Nmcli.qml` (1627 lines of JS nmcli spawning + parsing on the QML main thread) into the existing C++ plugin (`plugin/src/Caelestia/Services/`), keeping the public QML API byte-identical so no UI page changes. Motivation: fluidity — this is the largest remaining JS/main-thread parser in the shell.

## Architecture

- **`NmcliCore`** — C++ `QML_ELEMENT` (NOT singleton) in `Caelestia.Services`, instantiated once inside the QML facade. Owns all QProcess orchestration, parsing, state.
- **`NmcliAccessPoint`** / **`NmcliEthernetDevice`** — `QObject` per-entry types (`QML_ELEMENT` + `QML_UNCREATABLE`), NOTIFY-able properties, following the existing `FileSystemModel`/`FileSystemEntry` pattern. Exposed as `QList<T*>` Q_PROPERTY so QML keeps using `.find()`/`.some()` (JS array semantics required — QAbstractListModel is NOT a drop-in).
- **`services/Nmcli.qml`** — becomes a thin `pragma Singleton` facade: instantiates `NmcliCore { id: core }` plus one-line alias properties and one-line forwarding functions for every public member. Consumers keep `Nmcli.networks`, `Nmcli.AccessPoint` etc. unchanged.
  - Type annotations `Nmcli.AccessPoint`/`Nmcli.EthernetDevice` in consumer files: keep working via inline `component AccessPoint: NmcliAccessPoint {}` re-exports in the facade (or switch the 6 annotation sites to the plugin types if that fails — only acceptable consumer change).

## Public API contract (from consumer analysis 2026-07-10)

Properties (exact names): `networks` (list<AccessPoint>), `ethernetDevices` (list<EthernetDevice>), `active` (derived: first active AP or null), `activeEthernet` (derived: first connected dev or null), `wifiEnabled` (bool), `scanning` (bool), `hasAvailableEthernet` (bool), `ethernetSpeed` (string), `ethernetDeviceDetails` (var map), `ethernetDataUsage` (string), `pendingConnection` (var|null).

Functions: `rescanWifi()`, `connectingSsid()`, `forgetNetwork(ssid, cb)`, `disconnectEthernet(name, cb)`, `connectEthernet(name, iface, cb)`, `hasSavedProfile(ssid)`, `getEthernetSpeed(iface)`, `getEthernetDeviceDetails(iface, cb)`, `enableWifi(enabled, cb)`, `toggleWifi(cb)`, `disconnectFromNetwork()`, `getEthernetInterfaces(cb)`, `getEthernetDataUsage(iface, cb)`, `getIpv4Config(name, cb)`, `setIpv4Config(name, config, cb)`, plus **keep** `connectToNetworkWithPasswordCheck`, `connectToNetwork` and signal `connectionFailed(ssid)` (wifi connect path; zero direct grep hits but likely reached via page-local wrappers — verify before dropping).

Callbacks: JS functions receiving `{success, output, error, exitCode, needsPassword?}` — C++ takes `QJSValue` and invokes with a `QVariantMap`.

Entry types: `AccessPoint{ssid,bssid,strength:int,frequency:int,active:bool,security,isSecure}`, `EthernetDevice{iface,type,state,connection,connected,ipAddress,gateway,dns,subnet,macAddress,speed}`.

Everything else in the current file (40+ functions, constants, dead properties) is internal — do NOT port as public API.

## Behavioral requirements

1. **Process env**: every nmcli QProcess with `LANG=C.UTF-8 LC_ALL=C.UTF-8` (parse stability).
2. **Terse parsing**: proper unescape of nmcli `-t`/`-g` escaping (`\:`, `\\`) — current QML uses a placeholder hack; do it correctly.
3. **Keyed in-place reconcile**: update existing entry objects' properties (NOTIFY), remove missing, append new. Keys: AP = `freq:ssid:bssid`, ethernet = iface. Preserves QML delegate identity.
4. **Refresh triggers**: `nmcli monitor` persistent process; any output line → debounced refresh (single QTimer ~300ms, coalesce bursts — improvement over current per-line refresh); monitor crash → restart after 2s. Rescan proc exit → refresh networks. `scanning` == rescan proc running.
5. **Pending-connection state machine** (port intent, simplify): on wifi connect, success detected by refresh showing SSID active; 4s timeout → `connectionFailed(ssid)` + callback `{error:"Connection timeout"}`; up to 2 retries; 500ms repeated checks (max 6). `callbackCalled` guard → in C++, invoke-once semantics per QJSValue.
6. **Password detection heuristics**: same stderr substrings ("Secrets were required", "No secrets provided", "802-11-wireless-security.psk", "password for"), gated on absence of "Connection activated"/"successfully" → result `needsPassword: true`.
7. **Anti-flicker**: keep last-known ethernet details on transient nmcli failure.
8. **Known QML bug not to replicate**: `Qt.callLater(fn, delay)` was used as if delayed — it isn't. Port the intent with real QTimer::singleShot delays (300–1000ms as annotated).
9. **Security note (parity, flag only)**: PSK passed as argv (visible in /proc during run). Do not regress; improving to stdin `--ask` is out of scope.
10. **Command inventory**: use the exact nmcli command lines documented in the 2026-07-10 analysis (device status terse, `-g ACTIVE,SIGNAL,FREQ,SSID,BSSID,SECURITY d w`, saved connections + sequential per-connection SSID query, `device show`, radio on/off, connection add/up/down/delete/modify ipv4.*, sysfs speed + rx/tx bytes — sysfs reads become direct QFile reads, no `sh -c`).

## Phases

1. **Types + runner**: `nmclitypes.{hpp,cpp}` (entry QObjects), `nmcli.{hpp,cpp}` skeleton: process runner (env, stdout/stderr capture, exit → QVariantMap, QJSValue invoke-once), CMakeLists registration.
2. **Read paths**: wifi list, device status, saved connections, device details, radio state, sysfs speed/usage; reconcile; derived props.
3. **Orchestration**: monitor + restart, rescan, debounced refresh, startup sequence.
4. **Mutations**: connect/disconnect/forget/radio/ipv4 + pending state machine + password heuristics.
5. **Facade**: rewrite `services/Nmcli.qml` as thin wrapper; delete dead code.
6. **Verify**: `nix build`, live test on host (wifi list, connect with password, ethernet details, monitor reaction to airplane toggle), reviewer subagent over diff, ship.

## Non-goals

- No Rust (user chose C++ into existing plugin).
- No behavioral improvements beyond debounce + real delays + correct unescaping.
- VPN.qml, Clipboard, Emojis: later, same pattern.
