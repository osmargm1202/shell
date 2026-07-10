# Clipboard + Emojis Launcher Services C++ Port — Design Spec

**Date:** 2026-07-10
**Goal:** Port `modules/launcher/services/Clipboard.qml` (cliphist list parse, ~750 items per open) and `modules/launcher/services/Emojis.qml` (12k-line asset parse) to the C++ plugin, same recipe as the Nmcli port (`2026-07-10-nmcli-cpp-port.md`). Thin QML facades, API identical.

## Architecture

- `ClipboardCore` + `EmojisCore` — C++ `QML_ELEMENT` classes in `Caelestia.Services`, instantiated inside the existing QML singleton files (facades).
- **Items as `QVariantList` of `QVariantMap`** (NOT QObject entries): consumers run JS `Array.filter` over them (`AppList.qml` clipboardResults/emojiResults) and delegates read `modelData.preview/.id/.isImage/.char/.name`. QVariantList → real JS array; exact member types: `id` int, `preview`/`char`/`name` QString, `isImage` bool.
- `Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)` — AppList root-property bindings depend on the NOTIFY.

## Clipboard API contract

- `items`: `[{id:int, preview:string, isImage:bool}]` — parse `cliphist list` output: per line `^(\d+)\t(.+)`, skip non-matching; `isImage` = regex `^\[\[ binary data \d+ KiB png \d+x\d+ \]\]` **byte-identical** (png+KiB only — do not widen).
- `reload()`: spawn `cliphist list` (QProcess, capture stdout), parse on finish, set items, then preload images.
- `getSortedItems()`: favourites first — `GlobalConfig.launcher.favouriteClips` is a list of **strings** (compare `String(id)`), stable partition preserving cliphist order.
- `ensureImageCached(id, onReady)`: keep 2-arg signature; onReady optional QJSValue — current QML never fires it (dead timer that never ran) and sole caller passes no callback; C++ may invoke it when the file exists (QTimer 1s parity is fine) but must not require it.
- `getImagePath(id)` → `"/tmp/caelestia-clipboard/<id>.png"` — **path scheme is load-bearing**: `ClipItem.qml:61` hardcodes it.
- `imageCacheDir` constant property (API parity).
- Image preload: for each isImage item, decode via `cliphist decode <id>` writing to the cache path. C++: QProcess writing to file (no `sh -c` string concat). Batch/queue politely (don't fork 100 at once), but keep per-file output path.
- Copy/favourite/delete actions stay in ClipItem.qml (not service scope).

## Emojis API contract

- `items`: `[{char:string, name:string}]` — parse `assets/emojis.txt` (~12k lines) directly with QFile (no `cat` spawn): per line, first space splits char|name, skip empty/no-space lines.
- `reload()`: **one-shot** (`_loaded` guard) — loads once per shell run; also triggers frequency load.
- `getSortedItems()`: copy + std::stable_sort — favourites first (`GlobalConfig.launcher.favouriteEmojis`, raw emoji chars, keyed by `char`), then frequency desc (`frequencies[char] || 0`), stable (preserve file order).
- `recordUsage(charStr)`: increment frequency map, persist.
- Frequencies file `~/.config/caelestia/emoji-frequencies.json` (XDG config + `/caelestia/`): read with fallback `{}` on missing/corrupt; write via `QDir::mkpath` + `QSaveFile` + QJsonDocument (replaces sh printf pipeline).
- `frequencies` property optional (internal in QML today — keep internal in C++).

## Facades

- `modules/launcher/services/Clipboard.qml` and `Emojis.qml` → `pragma Singleton` wrappers instantiating the cores; alias `items`, forward functions. No consumer file changes expected (no type annotations involved).

## Non-goals

- No behavior widening (isImage regex, sort orders, path schemes stay exact).
- ClipItem/EmojiItem copy/favourite logic untouched.

## Phases

1. C++ cores + CMakeLists.
2. Facades.
3. `nix build` clean. Review with VPN port combined. Ship.
