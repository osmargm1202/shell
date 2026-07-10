pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io
import Caelestia
import Caelestia.Config
import qs.utils

QtObject {
    id: root

    property var items: []
    property var frequencies: ({})
    property bool _loaded: false

    readonly property string freqFile: Paths.config + "/emoji-frequencies.json"

    property Process reader: Process {
        running: false
        command: ["cat", Quickshell.shellPath("assets/emojis.txt")]
        stdout: StdioCollector {
            onStreamFinished: {
                const result = [];
                const lines = text.trim().split("\n");

                for (let i = 0; i < lines.length; i++) {
                    const line = lines[i];
                    if (!line)
                        continue;

                    const spaceIdx = line.indexOf(" ");
                    if (spaceIdx < 0)
                        continue;

                    result.push({
                        char: line.substring(0, spaceIdx),
                        name: line.substring(spaceIdx + 1).trim()
                    });
                }

                root.items = result;
                root._loaded = true;
            }
        }
    }

    property Process freqReader: Process {
        running: false
        command: ["sh", "-c", "cat \"$1\" 2> /dev/null || echo '{}'", "--", root.freqFile]
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    root.frequencies = JSON.parse(text) || {};
                } catch (e) {
                    root.frequencies = {};
                }
            }
        }
    }

    property Process freqWriter: Process {
        running: false
    }

    function reload(): void {
        if (_loaded)
            return;
        reader.running = true;
        loadFrequencies();
    }

    function loadFrequencies(): void {
        freqReader.running = true;
    }

    function saveFrequencies(): void {
        freqWriter.command = ["sh", "-c", "mkdir -p \"$(dirname \"$2\")\" && printf %s \"$1\" > \"$2\"", "--", JSON.stringify(frequencies), freqFile];
        freqWriter.running = true;
    }

    function recordUsage(charStr: string): void {
        frequencies[charStr] = (frequencies[charStr] || 0) + 1;
        saveFrequencies();
    }

    function getSortedItems(): var {
        if (!items.length)
            return [];
        const favEmojis = GlobalConfig.launcher.favouriteEmojis || [];
        const favSet = new Set(favEmojis);
        return [...items].sort((a, b) => {
            const aIsFav = favSet.has(a.char);
            const bIsFav = favSet.has(b.char);
            if (aIsFav !== bIsFav)
                return aIsFav ? -1 : 1;
            const freqA = frequencies[a.char] || 0;
            const freqB = frequencies[b.char] || 0;
            if (freqA !== freqB)
                return freqB - freqA;
            return 0;
        });
    }
}