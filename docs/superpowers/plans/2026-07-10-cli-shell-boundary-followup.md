# CLI–Shell Boundary Follow-up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose clipboard and emoji launcher modes through stable shell IPC, consume Rust CLI scheme previews without importing the Python package, and pin the compatible CLI revision.

**Architecture:** `modules/Shortcuts.qml` remains the single owner of launcher-opening behavior and exposes two thin IPC methods that reuse the existing shortcut path. `M3Variants.qml` delegates headless colour calculation to `caelestia scheme preview`, validates successful JSON before changing preview state, and preserves the current preview on failure. CLI implementation is an external prerequisite, not part of this plan.

**Tech Stack:** Quickshell QML, Quickshell `IpcHandler`/`Process`/`StdioCollector`, Nix flakes, Qt `qmlformat`/`qmllint`, repository QML convention linter.

## Global Constraints

- Work only in `/home/osmarg/Hobby/shell`.
- Preserve current visual behavior and fullscreen guard.
- Shell owns clipboard/emoji UI; do not add Fuzzel or another fallback.
- Shell must not implement material-colour calculation.
- Do not modify animation management or audit unrelated `Process` commands.
- Use exact IPC target `launcher` and methods `openClipboard` and `openEmoji`.
- Use exact CLI command `caelestia scheme preview --variant <variant>`.
- Do not update `M3Variants.qml` or `flake.lock` until a compatible CLI revision exists.
- Spec: `docs/superpowers/specs/2026-07-10-cli-shell-boundary-followup-design.md`.

---

### Task 1: Add launcher clipboard/emoji IPC

**Files:**
- Modify: `modules/Shortcuts.qml:10-15,127-153,203-245`

**Interfaces:**
- Consumes: `GlobalConfig.launcher.actionPrefix`, `Visibilities.launcherInitialSearch`, `Visibilities.getForActive()`, and `root.hasFullscreen`.
- Produces: QML function `openLauncherAction(action: string): void` and IPC methods `launcher.openClipboard()` / `launcher.openEmoji()`.

- [ ] **Step 1: Verify IPC methods do not exist yet**

With the development shell running, execute:

```bash
qs -c caelestia ipc call launcher openClipboard
qs -c caelestia ipc call launcher openEmoji
```

Expected: both calls fail because target `launcher` or its methods are not registered. Existing global shortcuts `caelestia:clipboard` and `caelestia:emoji` must still work.

- [ ] **Step 2: Add one shared launcher-action helper**

Add inside the root `Scope`, immediately after `hasFullscreen`:

```qml
function openLauncherAction(action: string): void {
    if (root.hasFullscreen)
        return;
    Visibilities.launcherInitialSearch = `${GlobalConfig.launcher.actionPrefix}${action} `;
    const visibilities = Visibilities.getForActive();
    visibilities.launcher = true;
}
```

This function preserves the current trailing space after the action name and the fullscreen behavior.

- [ ] **Step 3: Make existing shortcuts use the helper**

Replace the `emoji` shortcut handler with:

```qml
onPressed: root.openLauncherAction("emoji")
```

Replace the `clipboard` shortcut handler with:

```qml
onPressed: root.openLauncherAction("clipboard")
```

Do not change shortcut names or descriptions.

- [ ] **Step 4: Add stable IPC methods**

Add before the existing `drawers` `IpcHandler`:

```qml
IpcHandler {
    function openClipboard(): void {
        root.openLauncherAction("clipboard");
    }

    function openEmoji(): void {
        root.openLauncherAction("emoji");
    }

    target: "launcher"
}
```

- [ ] **Step 5: Run focused formatting and convention checks**

```bash
nix develop -c sh -lc 'qmlformat modules/Shortcuts.qml | diff -u modules/Shortcuts.qml -'
nix develop -c python3 scripts/qml-lint-conventions.py
```

Expected: both commands exit 0 with no diff and no convention errors. If `qmlformat` emits a diff, apply `qmlformat -i modules/Shortcuts.qml`, inspect it, then rerun both commands.

- [ ] **Step 6: Verify direct IPC and shortcut equivalence**

Reload the development shell, then run:

```bash
qs -c caelestia ipc call launcher openClipboard
qs -c caelestia ipc call launcher openEmoji
```

Expected:

- each command exits 0;
- clipboard opens with the configured `clipboard ` action prefix;
- emoji opens with the configured `emoji ` action prefix;
- existing global shortcuts open the same modes;
- neither route opens while fullscreen guard is active.

- [ ] **Step 7: Commit launcher IPC**

```bash
git add modules/Shortcuts.qml
git commit -m "feat(ipc): expose clipboard and emoji launcher modes"
```

---

### Task 2: Replace Python Material 3 preview with CLI contract

**External prerequisite:** The pinned or development `caelestia` executable must already implement `scheme preview --variant`, emit the agreed JSON object, preserve persisted scheme state, and return non-zero without partial stdout on failure.

**Files:**
- Modify: `modules/launcher/services/M3Variants.qml:18-32`

**Interfaces:**
- Consumes: `caelestia scheme preview --variant <variant>` stdout JSON with `name`, `flavour`, `mode`, `variant`, and `colours`.
- Produces: preview updates through existing `Colours.load(data, true)` and `Colours.showPreview` only after successful process exit and valid JSON.

- [ ] **Step 1: Verify CLI prerequisite and state immutability**

```bash
state="${XDG_STATE_HOME:-$HOME/.local/state}/caelestia/scheme.json"
before=$(sha256sum "$state")
out=$(caelestia scheme preview --variant vibrant)
after=$(sha256sum "$state")
test "$before" = "$after"
printf '%s\n' "$out" | jq -e '
  type == "object" and
  (.name | type == "string") and
  (.flavour | type == "string") and
  (.mode | type == "string") and
  (.variant == "vibrant") and
  (.colours | type == "object")
'
```

Expected: exit 0, `jq` prints `true`, and both hashes are identical. Stop this task if any assertion fails; return failure to the CLI agent.

- [ ] **Step 2: Record the source-level failure before replacement**

```bash
! rg -n 'python3|caelestia\.utils\.scheme' modules/launcher/services/M3Variants.qml
```

Expected: command fails because current source still contains both the Python executable and package import.

- [ ] **Step 3: Change preview command and buffer stdout**

Replace `previewVariant` and the current `Process` with:

```qml
function previewVariant(variant: string): void {
    getPreviewColoursProc.output = "";
    getPreviewColoursProc.command = ["caelestia", "scheme", "preview", "--variant", variant];
    getPreviewColoursProc.running = true;
}

Process {
    id: getPreviewColoursProc

    property string output

    stdout: StdioCollector {
        onStreamFinished: getPreviewColoursProc.output = text
    }

    // qmllint disable signal-handler-parameters
    onExited: code => {
        if (code !== 0) {
            console.warn(`M3 variant preview failed with exit code ${code}`);
            return;
        }

        try {
            const preview = JSON.parse(output);
            if (typeof preview !== "object" || preview === null || typeof preview.colours !== "object" || preview.colours === null)
                throw new Error("invalid scheme preview payload");
            Colours.load(output, true);
            Colours.showPreview = true;
        } catch (error) {
            console.warn(`M3 variant preview returned invalid JSON: ${error}`);
        }
    }
}
```

This intentionally delays `Colours.load` until process success. Failed commands and malformed JSON leave existing preview state untouched.

- [ ] **Step 4: Verify Python package coupling is gone**

```bash
! rg -n 'python3|caelestia\.utils\.scheme' modules/launcher/services/M3Variants.qml
rg -n 'caelestia.*scheme.*preview.*--variant' modules/launcher/services/M3Variants.qml
```

Expected: first command exits 0 with no matches; second finds the new command array.

- [ ] **Step 5: Run focused formatting and convention checks**

```bash
nix develop -c sh -lc 'qmlformat modules/launcher/services/M3Variants.qml | diff -u modules/launcher/services/M3Variants.qml -'
nix develop -c python3 scripts/qml-lint-conventions.py
```

Expected: exit 0. Apply `qmlformat -i` first only if its check prints a diff.

- [ ] **Step 6: Smoke-test success and failure paths**

Reload the shell and preview `vibrant`, then another variant from the launcher.

Expected success:

- preview changes without persisting the selected variant;
- clicking a variant still executes `caelestia scheme set -v <variant>` and persists it;
- no Python import error appears.

Temporarily run the shell with a `caelestia` executable that returns non-zero for `scheme preview`, or temporarily move that executable out of `PATH`, then trigger a preview.

Expected failure:

- concise warning appears;
- launcher remains running;
- current preview remains unchanged;
- no partial colour state is applied.

Restore the valid CLI before continuing.

- [ ] **Step 7: Commit CLI-backed preview**

```bash
git add modules/launcher/services/M3Variants.qml
git commit -m "refactor(launcher): use CLI for scheme previews"
```

---

### Task 3: Correct AI assistant command catalogue

**Files:**
- Modify: `modules/sidebar/AiAssistant.qml:790-804`

**Interfaces:**
- Consumes: final operational CLI surface from the approved design.
- Produces: accurate `caelestia_command` tool description; runtime command execution remains unchanged.

- [ ] **Step 1: Confirm obsolete commands are still advertised**

```bash
rg -n 'Valid subcommands:.*install, update' modules/sidebar/AiAssistant.qml
```

Expected: one match in the `caelestia_command` description.

- [ ] **Step 2: Remove failing NixOS compatibility stubs from description**

Change the description to exactly:

```qml
"description": "Execute a caelestia CLI command to manage the system. Valid subcommands: shell, toggle, scheme, search, screenshot, record, clipboard, emoji, wallpaper, resizer.",
```

Keep `clipboard`, `emoji`, and `search`. Do not change tool parameters or command execution logic.

- [ ] **Step 3: Verify catalogue content**

```bash
! rg -n 'Valid subcommands:.*(install|update)' modules/sidebar/AiAssistant.qml
rg -n 'Valid subcommands:.*clipboard, emoji.*resizer' modules/sidebar/AiAssistant.qml
```

Expected: first command exits 0 with no matches; second finds exactly one description.

- [ ] **Step 4: Run focused formatting and convention checks**

```bash
nix develop -c sh -lc 'qmlformat modules/sidebar/AiAssistant.qml | diff -u modules/sidebar/AiAssistant.qml -'
nix develop -c python3 scripts/qml-lint-conventions.py
```

Expected: exit 0 with no diff or convention errors.

- [ ] **Step 5: Commit catalogue correction**

```bash
git add modules/sidebar/AiAssistant.qml
git commit -m "fix(ai): advertise operational CLI commands"
```

---

### Task 4: Pin compatible CLI and run full integration verification

**External prerequisite:** The default branch of `github:osmargm1202/caelestia-cli` must contain both launcher IPC clients and `scheme preview --variant`, with its tests passing.

**Files:**
- Modify: `flake.lock`

**Interfaces:**
- Consumes: CLI revision implementing the exact contracts in the design spec.
- Produces: reproducible shell build containing that CLI revision when built with CLI support.

- [ ] **Step 1: Update only the CLI flake input**

```bash
nix flake update caelestia-cli
```

Expected: `flake.lock` changes the locked `caelestia-cli` revision. `flake.nix` remains unchanged.

- [ ] **Step 2: Inspect lock-file scope**

```bash
git diff -- flake.lock
```

Expected: diff is limited to `caelestia-cli` and transitive lock nodes required by that input. Investigate unrelated input changes before proceeding.

- [ ] **Step 3: Run repository format and convention checks**

```bash
nix develop -c sh -lc '
set -eu
for file in modules/Shortcuts.qml modules/launcher/services/M3Variants.qml modules/sidebar/AiAssistant.qml; do
  qmlformat "$file" | diff -u "$file" -
done
'
nix develop -c python3 scripts/qml-lint-conventions.py
```

Expected: all commands exit 0 with no output other than tool versions or normal status.

- [ ] **Step 4: Build plugin and shell package**

```bash
nix develop -c cmake -B build -G Ninja -DVERSION= -DGIT_REVISION=
nix develop -c cmake --build build
nix build .#default
```

Expected: all three commands exit 0.

- [ ] **Step 5: Run QML lint using generated imports**

Run inside the development shell:

```fish
touch .qmlls.ini
QT_QPA_PLATFORM=offscreen QML2_IMPORT_PATH="$PWD/build/qml:$QML2_IMPORT_PATH" timeout 2 qs -p .
set build_dir (grep -oP '(?<=buildDir=")(.*)(?=")' .qmlls.ini)
set import_paths (grep -oP '(?<=importPaths=")(.*)(?=")' .qmlls.ini | string split :)
set args -I $build_dir
for path in $import_paths
    set -a args -I $path
end
set qml_files (string match -vr '(build|modules/controlcenter)/.*' **.qml)
set lint_out (qmllint --import disable $args $qml_files 2>&1 | tee /dev/stderr)
test -z "$lint_out"
```

Expected: `qmllint` emits no diagnostics and final command exits 0.

- [ ] **Step 6: Run end-to-end contract checks**

Start/reload shell built against the updated lock, then execute:

```bash
qs -c caelestia ipc call launcher openClipboard
qs -c caelestia ipc call launcher openEmoji
caelestia clipboard
caelestia emoji
caelestia scheme preview --variant vibrant | jq -e '.variant == "vibrant" and (.colours | type == "object")'
```

Expected:

- direct IPC and CLI aliases open matching launcher modes;
- all four launcher commands exit 0;
- preview command prints valid JSON;
- M3 variant hover/selection previews correctly;
- persisted scheme changes only after clicking a variant, not while previewing.

- [ ] **Step 7: Commit compatible CLI pin**

```bash
git add flake.lock
git commit -m "chore(flake): pin CLI shell integration contract"
```

- [ ] **Step 8: Confirm final branch state**

```bash
git status --short --branch
git log --oneline -5
```

Expected: clean worktree and four implementation commits after the design/plan documentation commits.
