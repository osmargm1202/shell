# Steam Workshop Wallpapers Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add independently toggleable Wallhaven and Steam Workshop wallpaper sources to Nexus and Dashboard, with Steam Web API search and authenticated `steamcmd` downloads.

**Architecture:** Extend `GlobalConfig.services` with source settings, then add a QML singleton that owns Steam API pagination and the `steamcmd` download lifecycle. Reuse the existing Wallhaven UI structure for a Steam-specific tab, and register both sources in Nexus and Dashboard. Keep `steamcmd` in the package runtime closure; do not add a daemon or Steam Client integration.

**Tech Stack:** C++/Qt configuration plugin, QML/Quickshell, Steam Web API, `steamcmd`, Python/pytest contract tests, Nix flakes.

## Global Constraints

- Wallpaper Engine app ID is exactly `431960`; never use `108600` in implementation code.
- Steam Web API endpoint is `https://api.steampowered.com/IPublishedFileService/QueryFiles/v1/`.
- `wallhavenEnabled` and `steamWorkshopEnabled` default to `true` and operate independently.
- API keys come only from `GlobalConfig.services` / `shell.json`; no fish or environment-variable override.
- Never print either API key or a URL containing it.
- `steamcmd` is an unconditional runtime dependency.
- Supported downloaded media: `.mp4`, `.webm`, `.gif`, `.jpg`, `.jpeg`, `.png`.
- Media preference is video, then GIF, then still image; within a class choose the largest file.
- Do not add scene rendering, general app-ID selection, Steam Client integration, a daemon, or persistent download queues.
- Preserve Steam-managed Workshop content by copying, not moving, the selected media.

---

## File Structure

### New files

- `services/SteamWorkshopSearcher.qml` — Steam API state, normalization, cursor pagination, steamcmd process, media selection/copy, IPC.
- `modules/dashboard/SteamWorkshopTab.qml` — Steam search/results/detail/download UI reusable by Nexus and Dashboard.
- `modules/nexus/pages/wallandstyle/SteamWorkshopPage.qml` — Nexus `PageBase` wrapper.
- `tests/services/test_wallpaper_source_contracts.py` — source-level regression contracts for configuration, API safety, registrations, and packaging.

### Modified files

- `plugin/src/Caelestia/Config/serviceconfig.hpp` — declare Wallhaven and Steam Workshop configuration.
- `utils/Paths.qml` — derive Steam root and Workshop directories.
- `services/WallhavenSearcher.qml` — enabled guards and secret-safe logging.
- `modules/nexus/pages/wallandstyle/WallpaperSelect.qml` — service-aware source buttons.
- `modules/nexus/PageCompRegistry.qml` — register Steam Workshop Nexus page.
- `modules/dashboard/Content.qml` — register both source tabs and clamp selected index.
- `nix/default.nix` — place `steamcmd` in runtime closure and wrapper `PATH`.
- `README.md` — document configuration, authentication, and Nix unfree requirement.

---

### Task 1: Configuration, Paths, and Wallhaven Safety

**Files:**
- Create: `tests/services/test_wallpaper_source_contracts.py`
- Modify: `plugin/src/Caelestia/Config/serviceconfig.hpp:36-39`
- Modify: `utils/Paths.qml:20-25`
- Modify: `services/WallhavenSearcher.qml:14-18,44-62,65-113,116-152,155-190,244-278`

**Interfaces:**
- Produces: `GlobalConfig.services.wallhavenApiKey: QString`
- Produces: `GlobalConfig.services.wallhavenEnabled: bool`
- Produces: `GlobalConfig.services.steamWorkshopApiKey: QString`
- Produces: `GlobalConfig.services.steamWorkshopEnabled: bool`
- Produces: `GlobalConfig.services.steamWorkshopSteamRoot: QString`
- Produces: `GlobalConfig.services.steamWorkshopUsername: QString`
- Produces: `Paths.steamRoot`, `Paths.steamWorkshopContentDir`, `Paths.steamWorkshopDownloadDir`

- [ ] **Step 1: Write failing configuration and Wallhaven contract tests**

Create `tests/services/test_wallpaper_source_contracts.py`:

```python
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    return (ROOT / path).read_text()


def test_service_config_declares_wallpaper_sources() -> None:
    source = text("plugin/src/Caelestia/Config/serviceconfig.hpp")
    assert "CONFIG_GLOBAL_PROPERTY(QString, wallhavenApiKey)" in source
    assert "CONFIG_GLOBAL_PROPERTY(bool, wallhavenEnabled, true)" in source
    assert "CONFIG_GLOBAL_PROPERTY(QString, steamWorkshopApiKey)" in source
    assert "CONFIG_GLOBAL_PROPERTY(bool, steamWorkshopEnabled, true)" in source
    assert "CONFIG_GLOBAL_PROPERTY(QString, steamWorkshopSteamRoot" in source
    assert "CONFIG_GLOBAL_PROPERTY(QString, steamWorkshopUsername)" in source


def test_paths_expose_wallpaper_engine_directories() -> None:
    source = text("utils/Paths.qml")
    assert "readonly property string steamRoot" in source
    assert "readonly property string steamWorkshopContentDir" in source
    assert "readonly property string steamWorkshopDownloadDir" in source
    assert source.count("431960") == 2


def test_wallhaven_obeys_enabled_flag_and_does_not_log_secret_url() -> None:
    source = text("services/WallhavenSearcher.qml")
    assert "property bool enabled: GlobalConfig.services.wallhavenEnabled" in source
    assert source.count("if (!enabled)") >= 4
    assert 'console.log("Wallhaven search:", url)' not in source
    assert 'console.log("Wallhaven random:", url)' not in source
```

- [ ] **Step 2: Run tests and verify failure**

Run:

```bash
pytest -q tests/services/test_wallpaper_source_contracts.py
```

Expected: three failures because configuration, paths, and guards are absent.

- [ ] **Step 3: Add source configuration properties**

Insert after `bluetoothAutoReconnectDevices` in `serviceconfig.hpp`:

```cpp
    // Remote wallpaper source settings
    CONFIG_GLOBAL_PROPERTY(QString, wallhavenApiKey)
    CONFIG_GLOBAL_PROPERTY(bool, wallhavenEnabled, true)
    CONFIG_GLOBAL_PROPERTY(QString, steamWorkshopApiKey)
    CONFIG_GLOBAL_PROPERTY(bool, steamWorkshopEnabled, true)
    CONFIG_GLOBAL_PROPERTY(QString, steamWorkshopSteamRoot, u"~/.local/share/Steam"_s)
    CONFIG_GLOBAL_PROPERTY(QString, steamWorkshopUsername)
```

- [ ] **Step 4: Add derived Steam paths**

Insert after `wallsdir` in `utils/Paths.qml`:

```qml
    readonly property string steamRoot: absolutePath(GlobalConfig.services.steamWorkshopSteamRoot)
    readonly property string steamWorkshopContentDir: `${steamRoot}/steamapps/workshop/content/431960`
    readonly property string steamWorkshopDownloadDir: `${steamRoot}/steamapps/workshop/downloads/431960`
```

- [ ] **Step 5: Guard Wallhaven and remove secret-bearing URL logs**

Add beside `apiKey`:

```qml
    property bool enabled: GlobalConfig.services.wallhavenEnabled
```

Add this first statement to `search`, `searchRandom`, `searchNextPage`, `loadPage`, and `downloadWallpaper`:

```qml
        if (!enabled)
            return;
```

Replace URL logs with secret-free metadata:

```qml
        console.log("Wallhaven search:", query, "page", page);
```

```qml
        console.log("Wallhaven random:", query);
```

- [ ] **Step 6: Run focused tests**

Run:

```bash
pytest -q tests/services/test_wallpaper_source_contracts.py
```

Expected: `3 passed`.

- [ ] **Step 7: Build configuration plugin**

Run:

```bash
nix develop --impure -c cmake -S . -B build-plan -G Ninja -DENABLE_MODULES=plugin
nix develop --impure -c cmake --build build-plan -j2
```

Expected: plugin build completes with exit code 0 and generates `Caelestia` QML plugin artifacts.

- [ ] **Step 8: Commit**

```bash
git add tests/services/test_wallpaper_source_contracts.py \
  plugin/src/Caelestia/Config/serviceconfig.hpp utils/Paths.qml \
  services/WallhavenSearcher.qml
git commit -m "feat(config): add remote wallpaper service settings"
```

---

### Task 2: Package `steamcmd`

**Files:**
- Modify: `tests/services/test_wallpaper_source_contracts.py`
- Modify: `nix/default.nix:1-55`

**Interfaces:**
- Consumes: existing `pkgs.callPackage ./nix` argument injection.
- Produces: `steamcmd` on the wrapped shell's `PATH` and in development shells through `inputsFrom`.

- [ ] **Step 1: Add a failing packaging test**

Append:

```python
def test_steamcmd_is_an_unconditional_runtime_dependency() -> None:
    source = text("nix/default.nix")
    assert "  steamcmd," in source
    runtime_block = source.split("runtimeDeps =", 1)[1].split("]", 1)[0]
    assert "steamcmd" in runtime_block
```

- [ ] **Step 2: Run test and verify failure**

Run:

```bash
pytest -q tests/services/test_wallpaper_source_contracts.py::test_steamcmd_is_an_unconditional_runtime_dependency
```

Expected: failure because `steamcmd` is absent.

- [ ] **Step 3: Add `steamcmd` to package arguments and runtime dependencies**

Add `steamcmd` after `curl` in the function argument list and runtime list:

```nix
  curl,
  steamcmd,
  material-symbols,
```

```nix
      ffmpeg
      curl
      steamcmd
```

Do not add a flake input; `callPackage` supplies `pkgs.steamcmd`.

- [ ] **Step 4: Run focused test**

```bash
pytest -q tests/services/test_wallpaper_source_contracts.py::test_steamcmd_is_an_unconditional_runtime_dependency
```

Expected: `1 passed`.

- [ ] **Step 5: Evaluate package with unfree enabled**

Run:

```bash
NIXPKGS_ALLOW_UNFREE=1 nix build --impure .#caelestia-shell --no-link
```

Expected: successful evaluation/build; no missing `steamcmd` argument and no unfree-license rejection.

- [ ] **Step 6: Commit**

```bash
git add nix/default.nix tests/services/test_wallpaper_source_contracts.py
git commit -m "build: include steamcmd for Workshop downloads"
```

---

### Task 3: Steam Workshop Search Service

**Files:**
- Create: `services/SteamWorkshopSearcher.qml`
- Modify: `tests/services/test_wallpaper_source_contracts.py`

**Interfaces:**
- Consumes: `GlobalConfig.services.steamWorkshopApiKey`, `steamWorkshopEnabled`, `steamWorkshopUsername`
- Consumes: `Paths.steamRoot`, `Paths.steamWorkshopContentDir`, `Paths.steamWorkshopDownloadDir`, `Paths.wallsdir`
- Produces: `search(query: string): void`, `searchNextPage(): void`, `setQueryType(type: int): void`, `downloadItem(item: var): void`
- Produces signals: `searchComplete(var results, var meta)`, `downloadProgress(string id, real progress)`, `downloadComplete(string id, string path)`, `downloadFailed(string id, string error)`, `authRequired(string username)`

- [ ] **Step 1: Add failing service contract tests**

Append:

```python
def test_steam_service_uses_wallpaper_engine_api_without_leaking_key() -> None:
    source = text("services/SteamWorkshopSearcher.qml")
    assert "IPublishedFileService/QueryFiles/v1/" in source
    assert source.count("431960") >= 2
    assert "108600" not in source
    assert 'target: "steamworkshop"' in source
    assert 'console.log("Steam Workshop URL"' not in source


def test_steam_service_exposes_cursor_search_and_download_contract() -> None:
    source = text("services/SteamWorkshopSearcher.qml")
    for contract in (
        "property string nextCursor",
        "function search(query: string)",
        "function searchNextPage()",
        "function downloadItem(item: var)",
        "signal authRequired(string username)",
        "signal downloadComplete(string id, string path)",
    ):
        assert contract in source
    for extension in ("mp4", "webm", "gif", "jpg", "jpeg", "png"):
        assert extension in source
```

- [ ] **Step 2: Run tests and verify failure**

```bash
pytest -q \
  tests/services/test_wallpaper_source_contracts.py::test_steam_service_uses_wallpaper_engine_api_without_leaking_key \
  tests/services/test_wallpaper_source_contracts.py::test_steam_service_exposes_cursor_search_and_download_contract
```

Expected: failures because the service file does not exist.

- [ ] **Step 3: Create singleton state and normalized search API**

Create `services/SteamWorkshopSearcher.qml` with these exact public members:

```qml
pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io
import Caelestia.Config
import qs.utils

Singleton {
    id: root

    readonly property string apiBase: "https://api.steampowered.com/IPublishedFileService/QueryFiles/v1/"
    readonly property string appId: "431960"
    property string apiKey: GlobalConfig.services.steamWorkshopApiKey
    property bool enabled: GlobalConfig.services.steamWorkshopEnabled
    property string username: GlobalConfig.services.steamWorkshopUsername

    property bool loading: false
    property bool missingApiKey: false
    property string lastQuery: ""
    property string nextCursor: "*"
    property bool hasMore: false
    property int queryType: 12
    property list<var> results
    property var currentItem: null

    property string activeId: ""
    property string expectedBytes: "0"
    property string selectedSource: ""
    property string selectedDestination: ""
    property string stderrBuffer: ""

    signal searchComplete(var results, var meta)
    signal downloadProgress(string id, real progress)
    signal downloadComplete(string id, string path)
    signal downloadFailed(string id, string error)
    signal authRequired(string username)

    function normalizeItem(item: var): var {
        return {
            "id": String(item.publishedfileid ?? ""),
            "title": String(item.title ?? ""),
            "description": String(item.file_description ?? item.description ?? ""),
            "previewUrl": String(item.preview_url ?? (item.previews && item.previews.length ? item.previews[0].url : "")),
            "fileSize": Number(item.file_size ?? 0),
            "timeUpdated": Number(item.time_updated ?? 0),
            "tags": item.tags ?? [],
            "raw": item
        };
    }

    function buildUrl(query: string, cursor: string): string {
        const params = {
            "key": apiKey,
            "query_type": queryType.toString(),
            "cursor": cursor || "*",
            "numperpage": "20",
            "creator_appid": appId,
            "appid": appId,
            "search_text": query,
            "filetype": "0",
            "return_tags": "true",
            "return_previews": "true"
        };
        return apiBase + "?" + Object.keys(params)
            .map(key => `${key}=${encodeURIComponent(params[key])}`)
            .join("&");
    }

    function search(query: string): void {
        if (!enabled || !query.trim())
            return;
        missingApiKey = !apiKey;
        if (missingApiKey) {
            searchComplete([], {"missingApiKey": true});
            return;
        }
        lastQuery = query.trim();
        nextCursor = "*";
        results = [];
        requestPage(nextCursor, false);
    }

    function searchNextPage(): void {
        if (!enabled || loading || !hasMore || !lastQuery)
            return;
        requestPage(nextCursor, true);
    }

    function setQueryType(type: int): void {
        queryType = type;
    }
}
```

- [ ] **Step 4: Implement cursor request and response parsing**

Add inside the singleton:

```qml
    function requestPage(cursor: string, append: bool): void {
        loading = true;
        console.log("Steam Workshop search:", lastQuery, "cursor", cursor === "*" ? "initial" : "next");
        Requests.get(buildUrl(lastQuery, cursor), text => {
            try {
                const json = JSON.parse(text);
                const response = json.response ?? {};
                if (response.result && response.result !== 1)
                    throw new Error(response.result_details ?? `Steam result ${response.result}`);
                const page = (response.publishedfiledetails ?? []).map(item => normalizeItem(item));
                results = append ? [...results, ...page] : page;
                nextCursor = String(response.next_cursor ?? "");
                hasMore = nextCursor.length > 0 && page.length > 0;
                loading = false;
                searchComplete(page, {
                    "nextCursor": nextCursor,
                    "hasMore": hasMore,
                    "total": Number(response.total ?? results.length)
                });
            } catch (error) {
                loading = false;
                hasMore = false;
                console.warn("Steam Workshop request failed:", error);
                searchComplete([], {"error": String(error)});
            }
        });
    }
```

Do not log `buildUrl(...)` or `apiKey`.

- [ ] **Step 5: Implement steamcmd invocation and progress polling**

Add:

```qml
    function downloadItem(item: var): void {
        if (!enabled || !item)
            return;
        const id = String(item.id ?? item.publishedfileid ?? "").replace(/[^0-9]/g, "");
        if (!id) {
            downloadFailed("", "Invalid Workshop item ID");
            return;
        }
        activeId = id;
        expectedBytes = String(Number(item.fileSize ?? item.file_size ?? 0));
        stderrBuffer = "";
        downloadProc.command = [
            "steamcmd",
            "+force_install_dir", Paths.steamRoot,
            "+login", username || "anonymous",
            "+workshop_download_item", appId, id,
            "+quit"
        ];
        watchdog.restart();
        progressPoller.start();
        downloadProc.running = true;
    }

    function finishFailure(error: string): void {
        watchdog.stop();
        progressPoller.stop();
        downloadFailed(activeId, error);
        activeId = "";
    }

    Process {
        id: downloadProc
        stderr: SplitParser {
            splitMarker: ""
            onRead: data => root.stderrBuffer += data
        }
        onExited: code => {
            watchdog.stop();
            progressPoller.stop();
            if (code !== 0) {
                const lower = stderrBuffer.toLowerCase();
                if (lower.includes("login failure") || lower.includes("invalid password") || lower.includes("two-factor")) {
                    authRequired(username);
                    finishFailure("Steam authentication required");
                } else {
                    finishFailure(`steamcmd failed with exit code ${code}`);
                }
                return;
            }
            mediaFinder.command = ["bash", "-c",
                "find -- \"$1\" -type f \\\( -iname '*.mp4' -o -iname '*.webm' -o -iname '*.gif' -o -iname '*.jpg' -o -iname '*.jpeg' -o -iname '*.png' \\\) -printf '%s\\t%p\\n'",
                "bash", `${Paths.steamWorkshopContentDir}/${activeId}`];
            mediaFinder.running = true;
        }
    }

    Timer {
        id: progressPoller
        interval: 800
        repeat: true
        onTriggered: {
            if (!root.activeId || Number(root.expectedBytes) <= 0)
                return;
            sizeProc.command = ["du", "-sb", `${Paths.steamWorkshopDownloadDir}/${root.activeId}`];
            sizeProc.running = true;
        }
    }

    Process {
        id: sizeProc
        property string output: ""
        stdout: SplitParser {
            splitMarker: ""
            onRead: data => sizeProc.output += data
        }
        onRunningChanged: if (running) output = ""
        onExited: code => {
            if (code !== 0)
                return;
            const bytes = Number(output.trim().split(/\\s+/)[0] ?? 0);
            downloadProgress(root.activeId, Math.min(bytes / Number(root.expectedBytes), 0.99));
        }
    }

    Timer {
        id: watchdog
        interval: 600000
        onTriggered: {
            downloadProc.running = false;
            root.finishFailure("Steam Workshop download timed out");
        }
    }
```

- [ ] **Step 6: Implement deterministic media selection and atomic copy**

Add a `mediaFinder` `Process` that accumulates `size<TAB>path` lines. Parse each valid line into `{size, path, rank}` where rank is `0` for mp4/webm, `1` for gif, and `2` for still images. Sort by rank ascending, then size descending. If no candidate exists, call `finishFailure("No supported wallpaper media found")`.

Use these exact copy stages:

```qml
    function extension(path: string): string {
        const match = path.toLowerCase().match(/\.([a-z0-9]+)$/);
        return match ? match[1] : "";
    }

    function beginCopy(source: string): void {
        const ext = extension(source);
        selectedSource = source;
        selectedDestination = `${Paths.wallsdir}/steam-${activeId}.${ext}`;
        copyProc.command = ["cp", "--", source, `${selectedDestination}.tmp`];
        copyProc.running = true;
    }

    Process {
        id: copyProc
        onExited: code => {
            if (code !== 0) {
                root.finishFailure("Failed to copy Workshop media");
                return;
            }
            replaceProc.command = ["mv", "-f", "--", `${root.selectedDestination}.tmp`, root.selectedDestination];
            replaceProc.running = true;
        }
    }

    Process {
        id: replaceProc
        onExited: code => {
            if (code !== 0) {
                root.finishFailure("Failed to install Workshop media");
                return;
            }
            root.downloadProgress(root.activeId, 1);
            root.downloadComplete(root.activeId, root.selectedDestination);
            root.activeId = "";
        }
    }
```

- [ ] **Step 7: Add IPC search endpoint**

```qml
    IpcHandler {
        function doSearch(query: string): void {
            root.search(query);
        }

        target: "steamworkshop"
    }
```

- [ ] **Step 8: Run service contracts and QML conventions lint**

```bash
pytest -q tests/services/test_wallpaper_source_contracts.py
python scripts/qml-lint-conventions.py services/SteamWorkshopSearcher.qml
```

Expected: all pytest tests pass; linter exits 0 or reports only pre-existing repository-wide violations, with no crash/traceback and no violations for `SteamWorkshopSearcher.qml`.

- [ ] **Step 9: Commit**

```bash
git add services/SteamWorkshopSearcher.qml tests/services/test_wallpaper_source_contracts.py
git commit -m "feat(services): add Steam Workshop search and download"
```

---

### Task 4: Steam Workshop Tab and Nexus Entry

**Files:**
- Create: `modules/dashboard/SteamWorkshopTab.qml`
- Create: `modules/nexus/pages/wallandstyle/SteamWorkshopPage.qml`
- Modify: `modules/nexus/pages/wallandstyle/WallpaperSelect.qml:82-148`
- Modify: `modules/nexus/PageCompRegistry.qml:20-52`
- Modify: `tests/services/test_wallpaper_source_contracts.py`

**Interfaces:**
- Consumes: all `SteamWorkshopSearcher` public state, methods, and signals from Task 3.
- Produces: reusable `SteamWorkshopTab` component and Nexus subpage index `6`.

- [ ] **Step 1: Add failing Nexus/UI contracts**

Append:

```python
def test_nexus_registers_steam_workshop_page() -> None:
    registry = text("modules/nexus/PageCompRegistry.qml")
    selector = text("modules/nexus/pages/wallandstyle/WallpaperSelect.qml")
    page = text("modules/nexus/pages/wallandstyle/SteamWorkshopPage.qml")
    assert "SteamWorkshopPage {}" in registry
    assert 'text: qsTr("Steam Workshop")' in selector
    assert "GlobalConfig.services.steamWorkshopEnabled" in selector
    assert "SteamWorkshopTab" in page


def test_steam_tab_connects_search_download_and_set() -> None:
    source = text("modules/dashboard/SteamWorkshopTab.qml")
    assert "SteamWorkshopSearcher.search(" in source
    assert "SteamWorkshopSearcher.searchNextPage()" in source
    assert "SteamWorkshopSearcher.downloadItem(" in source
    assert "Wallpapers.setWallpaper(path)" in source
    assert "Steam authentication required" in source
```

- [ ] **Step 2: Run tests and verify failure**

```bash
pytest -q \
  tests/services/test_wallpaper_source_contracts.py::test_nexus_registers_steam_workshop_page \
  tests/services/test_wallpaper_source_contracts.py::test_steam_tab_connects_search_download_and_set
```

Expected: failures because UI files and registrations are absent.

- [ ] **Step 3: Create `SteamWorkshopTab.qml` from the established Wallhaven layout**

Use `WallhavenTab.qml` as the visual baseline, but bind only to the Steam contract:

```qml
property string searchQuery: ""
property var currentResults: []
property bool isLoading: SteamWorkshopSearcher.loading
property var selectedWallpaper: null
property bool detailPanelOpen: false
property int selectedIndex: -1
property string statusMessage: SteamWorkshopSearcher.missingApiKey
    ? qsTr("Add a Steam Web API key in services.steamWorkshopApiKey")
    : ""
```

Search controls:

```qml
Keys.onReturnPressed: {
    if (root.searchQuery.trim())
        SteamWorkshopSearcher.search(root.searchQuery);
}
```

```qml
TextButton {
    text: qsTr("Load more")
    visible: SteamWorkshopSearcher.hasMore && !root.isLoading
    onClicked: SteamWorkshopSearcher.searchNextPage()
}
```

Result image source:

```qml
source: modelData.previewUrl || ""
```

Detail metadata must include:

```qml
text: root.selectedWallpaper?.title || ""
```

```qml
text: root.selectedWallpaper
    ? qsTr("%1 MiB").arg((root.selectedWallpaper.fileSize / 1048576).toFixed(1))
    : ""
```

Download action:

```qml
TextButton {
    text: qsTr("Download & Set")
    onClicked: {
        if (root.selectedWallpaper)
            SteamWorkshopSearcher.downloadItem(root.selectedWallpaper);
    }
}
```

Connections:

```qml
Connections {
    target: SteamWorkshopSearcher

    function onSearchComplete(results, meta) {
        root.currentResults = SteamWorkshopSearcher.results;
        root.statusMessage = meta.error ?? "";
    }

    function onDownloadProgress(id, progress) {
        if (root.selectedWallpaper?.id === id)
            root.statusMessage = qsTr("Downloading… %1%").arg(Math.round(progress * 100));
    }

    function onAuthRequired(username) {
        root.statusMessage = qsTr("Steam authentication required. Run: steamcmd +login %1").arg(username || "<username>");
    }

    function onDownloadComplete(id, path) {
        if (root.selectedWallpaper?.id === id) {
            Wallpapers.setWallpaper(path);
            root.detailPanelOpen = false;
            root.statusMessage = "";
        }
    }

    function onDownloadFailed(id, error) {
        root.statusMessage = error;
    }
}
```

Keep the Wallhaven component's clipping, grid, selection navigation, animation, and typography structure. Remove Wallhaven-only random-search and resolution controls. Add a sort menu with exact mappings: Relevance → `12` (`RankedByTextSearch`), Trending → `3` (`RankedByTrend`), Newest → `1` (`RankedByPublicationDate`), and Popular → `11` (`RankedByVotesUp`). Each menu item calls `SteamWorkshopSearcher.setQueryType(value)` and reruns `search(root.searchQuery)` when the query is non-empty. Default remains `12`.

- [ ] **Step 4: Create Nexus wrapper**

Create `modules/nexus/pages/wallandstyle/SteamWorkshopPage.qml`:

```qml
import QtQuick
import qs.modules.dashboard
import qs.modules.nexus.common

PageBase {
    title: qsTr("Steam Workshop")
    isSubPage: true
    scrollable: false

    SteamWorkshopTab {
        anchors.fill: parent
    }
}
```

- [ ] **Step 5: Register Nexus page without changing existing indices**

Append after `WallpaperEnginePage` in the wallpaper/style stack:

```qml
                Component {
                    SteamWorkshopPage {}
                }
```

This makes Steam Workshop subpage index `6` and leaves Wallhaven at `4`, Wallpaper Engine at `5`.

- [ ] **Step 6: Add service-aware source buttons**

Update Wallhaven button:

```qml
visible: GlobalConfig.services.wallhavenEnabled
disabled: !Config.background.wallpaperEnabled || !GlobalConfig.services.wallhavenEnabled
```

Add after Wallpaper Engine:

```qml
IconTextButton {
    icon: "workspaces"
    text: qsTr("Steam Workshop")
    font: Tokens.font.body.large
    isRound: true
    shapeMorph: true
    horizontalPadding: Tokens.padding.extraLarge
    verticalPadding: Tokens.padding.medium
    type: IconTextButton.Tonal
    visible: GlobalConfig.services.steamWorkshopEnabled
    disabled: !Config.background.wallpaperEnabled || !GlobalConfig.services.steamWorkshopEnabled
    onClicked: root.nState.openSubPage(6)
}
```

- [ ] **Step 7: Run UI contracts and QML lint**

```bash
pytest -q tests/services/test_wallpaper_source_contracts.py
python scripts/qml-lint-conventions.py \
  modules/dashboard/SteamWorkshopTab.qml \
  modules/nexus/pages/wallandstyle/SteamWorkshopPage.qml \
  modules/nexus/pages/wallandstyle/WallpaperSelect.qml \
  modules/nexus/PageCompRegistry.qml
```

Expected: pytest passes; linter completes without traceback and reports no new-file violations.

- [ ] **Step 8: Commit**

```bash
git add modules/dashboard/SteamWorkshopTab.qml \
  modules/nexus/pages/wallandstyle/SteamWorkshopPage.qml \
  modules/nexus/pages/wallandstyle/WallpaperSelect.qml \
  modules/nexus/PageCompRegistry.qml \
  tests/services/test_wallpaper_source_contracts.py
git commit -m "feat(nexus): add Steam Workshop wallpaper browser"
```

---

### Task 5: Dashboard Source Tabs

**Files:**
- Modify: `modules/dashboard/Content.qml:19-54,170-207`
- Modify: `tests/services/test_wallpaper_source_contracts.py`

**Interfaces:**
- Consumes: `WallhavenTab`, `SteamWorkshopTab`, and both `GlobalConfig.services.*Enabled` flags.
- Produces: dynamic Dashboard tabs with a valid `screenState.dashboardTab` index.

- [ ] **Step 1: Add failing Dashboard contracts**

Append:

```python
def test_dashboard_registers_both_remote_wallpaper_sources() -> None:
    source = text("modules/dashboard/Content.qml")
    assert "component: wallhavenComponent" in source
    assert "enabled: GlobalConfig.services.wallhavenEnabled" in source
    assert "component: steamWorkshopComponent" in source
    assert "enabled: GlobalConfig.services.steamWorkshopEnabled" in source
    assert "WallhavenTab {}" in source
    assert "SteamWorkshopTab {}" in source
    assert "Math.min(root.screenState.dashboardTab" in source
```

- [ ] **Step 2: Run test and verify failure**

```bash
pytest -q tests/services/test_wallpaper_source_contracts.py::test_dashboard_registers_both_remote_wallpaper_sources
```

Expected: failure because Dashboard has neither source.

- [ ] **Step 3: Add filtered source tab descriptors**

Append to `allTabs` after Terminal:

```qml
            {
                component: wallhavenComponent,
                iconName: "image_search",
                text: qsTr("Wallhaven"),
                enabled: GlobalConfig.services.wallhavenEnabled
            },
            {
                component: steamWorkshopComponent,
                iconName: "workspaces",
                text: qsTr("Steam Workshop"),
                enabled: GlobalConfig.services.steamWorkshopEnabled
            }
```

- [ ] **Step 4: Clamp selected tab when service visibility changes**

Add:

```qml
    onDashboardTabsChanged: {
        root.screenState.dashboardTab = Math.min(
            root.screenState.dashboardTab,
            Math.max(root.dashboardTabs.length - 1, 0)
        );
    }
```

- [ ] **Step 5: Add loader components**

After `terminalComponent`:

```qml
            Component {
                id: wallhavenComponent

                WallhavenTab {}
            }

            Component {
                id: steamWorkshopComponent

                SteamWorkshopTab {}
            }
```

- [ ] **Step 6: Run contracts and lint**

```bash
pytest -q tests/services/test_wallpaper_source_contracts.py
python scripts/qml-lint-conventions.py modules/dashboard/Content.qml
```

Expected: all tests pass and linter completes without new violations.

- [ ] **Step 7: Commit**

```bash
git add modules/dashboard/Content.qml tests/services/test_wallpaper_source_contracts.py
git commit -m "feat(dashboard): expose remote wallpaper source tabs"
```

---

### Task 6: Documentation and Full Verification

**Files:**
- Modify: `README.md`

**Interfaces:**
- Documents: service configuration keys, app ID `431960`, Steam API key source, `steamcmd` login, Nix unfree requirement, supported media.

- [ ] **Step 1: Add user configuration documentation**

Add a “Remote wallpaper services” section near existing Wallhaven documentation:

```markdown
### Remote wallpaper services

Wallhaven and Steam Workshop are enabled by default and can be controlled independently:

```json
{
  "services": {
    "wallhavenEnabled": true,
    "wallhavenApiKey": "",
    "steamWorkshopEnabled": true,
    "steamWorkshopApiKey": "YOUR_STEAM_WEB_API_KEY",
    "steamWorkshopSteamRoot": "~/.local/share/Steam",
    "steamWorkshopUsername": "YOUR_STEAM_USERNAME"
  }
}
```

Steam Workshop browsing targets Wallpaper Engine (`appid=431960`). Downloads use `steamcmd`. Before the first restricted download, authenticate once with:

```sh
steamcmd +login YOUR_STEAM_USERNAME +quit
```

Nix users must allow the unfree `steamcmd` package when evaluating or building Caelestia Shell. Downloaded `.mp4`, `.webm`, `.gif`, `.jpg`, `.jpeg`, and `.png` files are copied into the configured wallpaper directory.
```

Ensure the nested Markdown fences use four-space indentation or a longer outer fence so README rendering remains valid.

- [ ] **Step 2: Run Python contract tests**

```bash
pytest -q
```

Expected: all repository tests pass.

- [ ] **Step 3: Run QML conventions lint across changed QML**

```bash
python scripts/qml-lint-conventions.py \
  services/WallhavenSearcher.qml \
  services/SteamWorkshopSearcher.qml \
  utils/Paths.qml \
  modules/dashboard/Content.qml \
  modules/dashboard/SteamWorkshopTab.qml \
  modules/nexus/PageCompRegistry.qml \
  modules/nexus/pages/wallandstyle/WallpaperSelect.qml \
  modules/nexus/pages/wallandstyle/SteamWorkshopPage.qml
```

Expected: no traceback, no new-file violations, and no violations introduced on modified lines.

- [ ] **Step 4: Run whitespace and secret checks**

```bash
git diff --check
! git diff | grep -E 'STEAM_WEB_API_KEY_VALUE|YOUR_STEAM_WEB_API_KEY[^"A-Z_]'
! grep -RIn 'console\.\(log\|warn\|error\).*apiKey\|console\.log.*buildUrl' services/WallhavenSearcher.qml services/SteamWorkshopSearcher.qml
```

Expected: all commands exit 0; no API key or constructed secret URL is logged.

- [ ] **Step 5: Build plugin and full package**

```bash
nix develop --impure -c cmake --build build-plan -j2
NIXPKGS_ALLOW_UNFREE=1 nix build --impure .#debug --no-link
```

Expected: both builds exit 0.

- [ ] **Step 6: Manual smoke test**

Start shell from debug result with a real API key in a temporary Caelestia config, then verify:

1. Wallhaven and Steam Workshop appear in Nexus and Dashboard.
2. Each disappears independently when its `services.*Enabled` flag is false.
3. Search returns Wallpaper Engine previews and load-more follows `next_cursor` without replacing previous results.
4. Missing/invalid API keys produce actionable UI text and no secret-bearing logs.
5. Authenticated `steamcmd` downloads a supported item under app ID `431960`.
6. The preferred largest supported media is copied to `Paths.wallsdir` and immediately selected.
7. Authentication failure emits the one-time login instruction.
8. Unsupported content and ten-minute timeout leave the current wallpaper unchanged.
9. Existing Wallhaven search/download still works.

- [ ] **Step 7: Commit documentation**

```bash
git add README.md
git commit -m "docs: explain remote wallpaper service setup"
```

- [ ] **Step 8: Final review snapshot**

```bash
git status --short
git log --oneline --decorate -8
git diff 94d7c9ec..HEAD --stat
```

Expected: clean working tree; design plus five implementation commits; diff contains only approved files.
