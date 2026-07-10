pragma Singleton

import QtQuick
import Caelestia.Config
import Caelestia.Services

QtObject {
    id: root

    readonly property alias items: core.items
    readonly property alias imageCacheDir: core.imageCacheDir

    readonly property ClipboardCore core: ClipboardCore {
        id: core

        favouriteClips: GlobalConfig.launcher.favouriteClips || []
    }

    function reload(): void {
        core.reload();
    }

    function getSortedItems(): var {
        // Reading these registers them as binding dependencies; the reads
        // inside C++ are invisible to QML's dependency capturer, so without
        // this a binding using getSortedItems() never re-evaluates.
        void [core.items, core.favouriteClips];
        return core.getSortedItems();
    }

    function getImagePath(clipId: int): string {
        return core.getImagePath(clipId);
    }

    function ensureImageCached(id: int, onReady: var): void {
        core.ensureImageCached(id, onReady);
    }
}
