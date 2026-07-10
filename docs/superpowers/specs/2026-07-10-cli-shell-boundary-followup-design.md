# CLI–Shell Boundary Follow-up — Design Spec

**Date:** 2026-07-10

## Goal

Finish the minimum shell-side work required by the Rust migration of `caelestia-cli` without duplicating UI or moving unrelated shell services.

## Scope

This change has two integration points:

1. Expose the shell launcher clipboard and emoji modes through stable IPC methods.
2. Replace `M3Variants.qml`'s direct import of the Python `caelestia` package with a headless CLI preview command.

Also update the AI assistant's advertised command list and pin the compatible CLI revision after both repositories implement the contract.

Out of scope:

- Moving animation management out of QML.
- Auditing every shell `Process` or external command.
- Reimplementing clipboard or emoji data handling.
- Adding a Fuzzel fallback.
- Changing visual launcher behavior.
- Changing visual screenshot/search picker behavior.

## Ownership Boundary

The shell owns resident UI and interactive state:

- Launcher visibility and active screen selection.
- Clipboard and emoji picker presentation.
- Clipboard/emoji filtering, favourites, previews, and item activation.
- Material 3 variant preview presentation.

The CLI owns headless, scriptable operations:

- Stable command aliases used by keybindings and external scripts.
- Material colour calculation and scheme serialization.
- Error reporting when the shell IPC endpoint is unavailable.

No clipboard/emoji picker backend is added to the CLI. No material-colour implementation is added to the shell.

## Launcher IPC Contract

`modules/Shortcuts.qml` will expose an `IpcHandler` with target `launcher` and these zero-argument methods:

```text
openClipboard()
openEmoji()
```

They are invoked externally as:

```bash
qs -c caelestia ipc call launcher openClipboard
qs -c caelestia ipc call launcher openEmoji
```

Each method must use the same internal launcher-opening helper as the existing `caelestia:clipboard` and `caelestia:emoji` shortcuts. The helper:

1. Returns without opening a launcher when the active context is fullscreen, preserving current behavior.
2. Sets `Visibilities.launcherInitialSearch` to the configured action prefix followed by `clipboard ` or `emoji `.
3. Opens the launcher for the active screen.

The existing shortcuts retain their names and behavior. IPC and shortcuts must not maintain separate copies of the opening logic.

The CLI may report IPC process failure to its caller. The shell does not add Fuzzel or another fallback.

## Scheme Preview Contract

The CLI will provide:

```bash
caelestia scheme preview --variant <variant>
```

Successful stdout is exactly one JSON object with these fields:

```json
{
  "name": "dynamic",
  "flavour": "default",
  "mode": "dark",
  "variant": "vibrant",
  "colours": {}
}
```

The concrete values come from the current scheme plus the requested variant. Preview execution must not:

- modify the persisted scheme;
- apply colours or themes;
- run hooks;
- send notifications.

Invalid variants or calculation failures produce non-zero exit status and diagnostic stderr. They must not emit a partial JSON object on stdout.

After that command is available, `modules/launcher/services/M3Variants.qml` will execute it directly instead of constructing a `python3 -c` program that imports `caelestia.utils.scheme`. Its existing JSON-to-preview flow remains unchanged.

This requirement removes the shell's runtime dependency on the Python package internals for Material 3 previews. It does not require removing unrelated Python scripts used elsewhere by the shell.

## AI Assistant Command Catalogue

`modules/sidebar/AiAssistant.qml` must advertise the final operational command surface rather than every legacy parser entry.

- `clipboard` and `emoji` remain listed after their CLI commands become working IPC clients.
- `install` and `update` are removed from the advertised operational list because the NixOS CLI fork intentionally leaves them as failing compatibility stubs.
- `search` remains listed; it is the screen-region Google Lens workflow, not a duplicate launcher search.

## Integration Order

1. Shell adds and verifies `launcher.openClipboard` and `launcher.openEmoji` IPC.
2. CLI changes its clipboard and emoji stubs into clients for those exact IPC methods.
3. CLI implements and tests `scheme preview --variant`.
4. Shell changes `M3Variants.qml` to consume the new command.
5. Shell updates its CLI flake input/lock to a revision containing both CLI changes.
6. Shell updates the AI assistant command catalogue and runs full verification.

The shell must not update `M3Variants.qml` or its CLI pin before a compatible CLI revision exists.

## Error Handling

- Launcher IPC follows current fullscreen behavior: ignored requests are not treated as shell crashes.
- A missing or stopped shell is handled by the CLI process invoking IPC; the shell has no fallback responsibility.
- `M3Variants.qml` keeps the currently displayed preview if the CLI command fails or returns invalid JSON and logs a concise warning.
- No failed preview may alter the persisted scheme.

## Verification

Automated/static checks:

- Existing QML formatting and lint checks pass.
- `modules/Shortcuts.qml` contains one shared action-opening path used by shortcuts and IPC.
- `M3Variants.qml` contains no import of `caelestia.utils.scheme` and no `python3 -c` preview command.
- `AiAssistant.qml` no longer advertises `install` or `update` as operational commands.

Integration checks with the compatible CLI revision:

```bash
qs -c caelestia ipc call launcher openClipboard
qs -c caelestia ipc call launcher openEmoji
caelestia clipboard
caelestia emoji
caelestia scheme preview --variant vibrant
```

Expected results:

- Direct IPC and CLI aliases open the same launcher modes as existing shortcuts.
- Fullscreen behavior remains unchanged.
- Scheme preview emits valid JSON and updates only the temporary visual preview.
- Persisted scheme state remains byte-identical after preview.

## CLI Handoff

The CLI agent receives the exact IPC and scheme-preview contracts above. CLI-side implementation, tests, dependency cleanup, and documentation remain outside this shell spec.
