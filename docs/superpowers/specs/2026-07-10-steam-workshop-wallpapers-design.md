# Steam Workshop Wallpaper Integration Design

**Date:** 2026-07-10
**Status:** Approved

## Goal

Add Steam Workshop as a wallpaper source alongside Wallhaven. Users can search Wallpaper Engine Workshop items, inspect previews and metadata, download supported media through `steamcmd`, and immediately set downloaded media as the active wallpaper.

Wallhaven and Steam Workshop are independently configurable services. Both are enabled by default.

## Reference implementation

The design follows the established patterns in:

- `liixini/skwd-wall`
- `liixini/skwd-daemon`
- Existing Caelestia `WallhavenSearcher.qml` and `WallhavenTab.qml`

The relevant skwd pattern separates public Workshop browsing from authenticated downloads:

- Steam Web API searches and returns metadata and preview URLs.
- `steamcmd` downloads Workshop content using the user's Steam session.
- Wallpaper Engine uses app ID `431960` for API queries, `steamcmd`, and local Workshop paths.

The originally supplied `108600` app ID belongs to Project Zomboid and is intentionally not used.

## Scope

### Included

- Wallpaper Engine Workshop only (`appid=431960`).
- Search through `IPublishedFileService/QueryFiles/v1/`.
- Preview images and item metadata.
- Filtering downloaded content to supported images, GIFs, and videos.
- `steamcmd` as a required runtime dependency.
- Steam Workshop access from Nexus and Dashboard.
- Wallhaven access from Nexus and Dashboard.
- Independent Wallhaven and Steam Workshop service toggles, enabled by default.
- API keys stored in Caelestia's `shell.json` service configuration.
- Fix the existing undeclared `wallhavenApiKey` configuration property.

### Excluded

- General Workshop app ID selection.
- Wallpaper Engine scene rendering through `linux-wallpaperengine`.
- Steam Client native integration.
- A separate persistent download daemon.
- Multi-item download queues or recovery across shell restarts.
- Fish environment variable overrides.

## Configuration

Add these global service properties to `ServiceConfig`:

```cpp
CONFIG_GLOBAL_PROPERTY(QString, wallhavenApiKey)
CONFIG_GLOBAL_PROPERTY(bool, wallhavenEnabled, true)
CONFIG_GLOBAL_PROPERTY(QString, steamWorkshopApiKey)
CONFIG_GLOBAL_PROPERTY(bool, steamWorkshopEnabled, true)
CONFIG_GLOBAL_PROPERTY(QString, steamWorkshopSteamRoot, u"~/.local/share/Steam"_s)
CONFIG_GLOBAL_PROPERTY(QString, steamWorkshopUsername)
```

Example `~/.config/caelestia/shell.json`:

```json
{
  "services": {
    "wallhavenEnabled": true,
    "wallhavenApiKey": "",
    "steamWorkshopEnabled": true,
    "steamWorkshopApiKey": "STEAM_WEB_API_KEY_VALUE",
    "steamWorkshopSteamRoot": "~/.local/share/Steam",
    "steamWorkshopUsername": "steam-account-name"
  }
}
```

The API key remains in the user's config file and must never be printed in logs. Search logs may print the endpoint and non-secret parameters, but not the final URL containing `key`.

`steamWorkshopUsername` is optional for browsing. Downloads should use the configured username when present and otherwise try `anonymous`; restricted Workshop items may require the user to configure a username and complete one interactive `steamcmd +login <username>` session.

## Architecture

```text
Nexus SteamWorkshopPage ─┐
Dashboard Steam tab ─────┼── SteamWorkshopSearcher singleton
                         │      ├── Steam Web API via Requests
                         │      ├── steamcmd via Quickshell Process
                         │      └── Wallpapers.setWallpaper(path)
Nexus WallhavenPage ─────┐
Dashboard Wallhaven tab ─┴── WallhavenSearcher singleton

GlobalConfig.services
  ├── Wallhaven enabled/key
  └── Steam Workshop enabled/key/root/username
```

The new Steam service mirrors the existing Wallhaven boundary: UI owns presentation and selection state; the singleton owns API requests, pagination, downloads, and download signals.

## Components

### `services/SteamWorkshopSearcher.qml`

A singleton responsible for Workshop API and `steamcmd` operations.

#### Constants and configuration

- API base: `https://api.steampowered.com/IPublishedFileService/QueryFiles/v1/`
- Wallpaper Engine app ID: `431960`
- API key: `GlobalConfig.services.steamWorkshopApiKey`
- Enabled flag: `GlobalConfig.services.steamWorkshopEnabled`
- Steam root: resolved from `steamWorkshopSteamRoot`
- Username: `steamWorkshopUsername`

#### Search state

- `loading`
- `lastQuery`
- `currentPage`
- `lastPage`
- `results`
- `selectedItem`
- active filters and sort order

#### API request

A search request sends:

```text
key=<configured key>
query_type=<selected sort/query mode>
cursor=*
numperpage=20
creator_appid=431960
appid=431960
search_text=<query>
filetype=0
return_tags=true
return_previews=true
```

The service converts Steam's response into a stable UI model with at least:

- `id` from `publishedfileid`
- `title`
- `description`
- `previewUrl`
- `fileSize`
- `timeUpdated`
- `tags`
- raw item data for future compatibility

Pagination should use Steam's returned cursor where available. The UI-facing service state may still expose current-page and last-page semantics for parity with Wallhaven, but implementation must not invent a fixed final page when Steam only supplies cursor-based continuation.

Search methods return immediately when the service is disabled, the query is blank, or the API key is blank. Missing-key state is exposed separately from an empty result set so the UI can show a configuration prompt.

#### Download flow

1. Validate service state and numeric Workshop ID.
2. Start:

   ```text
   steamcmd
     +force_install_dir <steam-root>
     +login <username-or-anonymous>
     +workshop_download_item 431960 <published-file-id>
     +quit
   ```

3. Poll the relevant Workshop download/content directory size every 800 ms when expected file size is known.
4. On successful exit, recursively locate supported media under:

   ```text
   <steam-root>/steamapps/workshop/content/431960/<published-file-id>/
   ```

5. Supported outputs are `.mp4`, `.webm`, `.gif`, `.jpg`, `.jpeg`, and `.png`.
6. Copy the selected media into `Paths.wallsdir` as `steam-<id>.<ext>`. Copying preserves Steam-managed Workshop content.
7. Emit `downloadComplete(id, destination)`.
8. UI calls `Wallpapers.setWallpaper(destination)` and closes the detail panel.

If an item contains several supported media files, selection is deterministic: prefer video, then GIF, then still image; within each class choose the largest file. This avoids accidentally selecting thumbnails or small assets.

#### Signals

```qml
signal searchComplete(var results, var meta)
signal downloadProgress(string id, real progress)
signal downloadComplete(string id, string path)
signal downloadFailed(string id, string error)
signal authRequired(string username)
```

#### IPC

Expose a minimal `IpcHandler` target named `steamworkshop` with search support. Download IPC is excluded until command argument validation and user feedback requirements are defined.

### `modules/dashboard/SteamWorkshopTab.qml`

Use the visual and interaction structure of `WallhavenTab.qml`:

- Search field and submit action.
- Loading indicator.
- Sort and media-type controls.
- Thumbnail grid using Steam preview URLs.
- Cursor-based load-more control.
- Detail panel with title, preview, tags, update date, and file size.
- Download progress and actionable authentication errors.
- `Download & Set` action.

Media-type filtering applies to downloaded content metadata when Steam exposes reliable tags and otherwise acts as a post-download supported-file preference. The UI must not claim that a preview image proves the final Workshop item contains that same media type.

### `modules/nexus/pages/wallandstyle/SteamWorkshopPage.qml`

A `PageBase` wrapper around `SteamWorkshopTab`, matching `WallhavenPage.qml`.

Register it in the wallpaper/style `StackPage` in `PageCompRegistry.qml`. Add a Steam Workshop button to `WallpaperSelect.qml`. Keep subpage indices explicit and update existing indices if insertion changes their order.

The Wallhaven button is disabled when `wallhavenEnabled` is false. The Steam Workshop button is disabled when `steamWorkshopEnabled` is false.

### Dashboard integration

`modules/dashboard/Content.qml` currently does not register Wallhaven despite `WallhavenTab.qml` living in the dashboard module. Add two Dashboard tabs:

- Wallhaven, enabled by `GlobalConfig.services.wallhavenEnabled`
- Steam Workshop, enabled by `GlobalConfig.services.steamWorkshopEnabled`

Each tab gets a component entry, icon, label, and loader component. Existing Dashboard tabs and order remain unchanged; the two wallpaper-source tabs follow the existing Terminal tab.

Dashboard index handling must remain valid when either service is disabled. Clamp `screenState.dashboardTab` after the filtered tab list changes so it never points beyond the last visible tab.

### Wallhaven corrections

Update `WallhavenSearcher.qml` to use the newly declared configuration properties. Add enabled guards to search, random search, pagination, and download methods. Do not include the API key in logs.

The existing Wallhaven UI should show or hide according to the service flag in both Nexus and Dashboard.

### Paths

Add derived paths to `utils/Paths.qml`:

```qml
readonly property string steamRoot: absolutePath(GlobalConfig.services.steamWorkshopSteamRoot)
readonly property string steamWorkshopContentDir: `${steamRoot}/steamapps/workshop/content/431960`
readonly property string steamWorkshopDownloadDir: `${steamRoot}/steamapps/workshop/downloads/431960`
```

The service uses these properties rather than duplicating path construction.

## Nix packaging

Add `steamcmd` to `nix/default.nix` function arguments and unconditional `runtimeDeps`. The shell wrapper therefore places `steamcmd` on `PATH` for all packaged installations and development shells derived from the package.

Do not add a new flake input. `steamcmd` comes from the existing `nixpkgs` input through `callPackage` argument injection.

Because Steam packages may require unfree licensing acceptance, document that Nix evaluation must permit `steamcmd`'s unfree license. The implementation should verify the repository's current Nix policy and avoid changing global user policy implicitly.

## Error handling

| Failure | Required behavior |
|---|---|
| Service disabled | Hide/disable entry points and early-return from service methods. |
| Missing API key | Show configuration prompt; do not perform request. |
| Invalid/non-JSON API response | Clear loading state, preserve prior query metadata only when safe, and show request failure. |
| Steam API error payload | Surface Steam's message without exposing the API key. |
| Rate limit | Show retry-later state and permit a manual retry. |
| `steamcmd` missing | Emit explicit dependency error, even though packaged builds include it. |
| Authentication required | Detect common steamcmd login failures, emit `authRequired`, and explain one-time login command. |
| Deleted/private item | Emit item-specific download failure. |
| Unsupported content | Explain that no supported media file was found; do not set wallpaper. |
| Timeout | Stop the process after ten minutes and emit timeout failure. |
| Destination collision | Replace atomically through a temporary destination, or reuse the existing complete file. |

All shell-facing paths and IDs must be passed as structured `Process.command` arguments where possible. If a shell is unavoidable for recursive file discovery, validate the numeric item ID and quote every filesystem path.

## Testing

### Automated

- Compile the C++ plugin to validate new `ServiceConfig` properties.
- Run existing QML lint checks for every new and modified QML file.
- Add focused tests for any extracted JavaScript normalization helper if the repository's test harness supports it.
- Evaluate/build the Nix package with unfree packages enabled to confirm `steamcmd` injection.

### Manual smoke tests

1. Start a debug shell with both services enabled.
2. Confirm Wallhaven and Steam Workshop appear in Nexus and Dashboard.
3. Disable each service independently and confirm only its entry points disappear.
4. Search Wallpaper Engine Workshop for a known term and verify 20 previews render.
5. Load the next cursor page without duplicate items.
6. Open detail view and verify metadata.
7. Download a video item after an authenticated `steamcmd` login.
8. Verify progress updates, copied destination, and immediate wallpaper activation.
9. Test an item with GIF or still-image content.
10. Test missing API key, invalid key, offline network, login failure, unsupported Workshop content, and download timeout.
11. Confirm logs never contain either API key.
12. Confirm existing Wallhaven search and download behavior still works.

## Files

### New

- `services/SteamWorkshopSearcher.qml`
- `modules/dashboard/SteamWorkshopTab.qml`
- `modules/nexus/pages/wallandstyle/SteamWorkshopPage.qml`
- `docs/superpowers/specs/2026-07-10-steam-workshop-wallpapers-design.md`

### Modified

- `plugin/src/Caelestia/Config/serviceconfig.hpp`
- `services/WallhavenSearcher.qml`
- `modules/dashboard/Content.qml`
- `modules/dashboard/WallhavenTab.qml`
- `modules/nexus/pages/wallandstyle/WallpaperSelect.qml`
- `modules/nexus/PageCompRegistry.qml`
- `utils/Paths.qml`
- `nix/default.nix`
- `README.md`

`flake.nix` should remain unchanged unless implementation verification reveals that existing `callPackage` injection is insufficient.

## Acceptance criteria

- Steam Workshop queries use app ID `431960` and never log the API key.
- Search results display previews and metadata in Nexus and Dashboard.
- `steamcmd` downloads supported media and the selected media becomes the active wallpaper.
- Wallhaven and Steam Workshop can each be enabled or disabled through `services` configuration.
- Both service flags default to enabled.
- Existing Wallhaven API key configuration works through a declared property.
- Packaged and development environments expose `steamcmd` on `PATH`.
- New QML passes lint, C++ plugin builds, and manual failure states are actionable.
