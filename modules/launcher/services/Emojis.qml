pragma Singleton

import QtQuick
import Quickshell
import Caelestia.Config
import Caelestia.Services

QtObject {
    id: root

    readonly property alias items: core.items

    readonly property EmojisCore core: EmojisCore {
        id: core

        sourcePath: Quickshell.shellPath("assets/emojis.txt")
        favouriteEmojis: GlobalConfig.launcher.favouriteEmojis || []
    }

    function reload(): void {
        core.reload();
    }

    function getSortedItems(): var {
        // Reading these registers them as binding dependencies; the reads
        // inside C++ are invisible to QML's dependency capturer, so without
        // this a binding using getSortedItems() never re-evaluates.
        void [core.items, core.favouriteEmojis];
        return core.getSortedItems();
    }

    function recordUsage(charStr: string): void {
        core.recordUsage(charStr);
    }
}
