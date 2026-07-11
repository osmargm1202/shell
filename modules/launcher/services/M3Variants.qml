pragma Singleton

import ".."
import QtQuick
import Quickshell
import Quickshell.Io
import Caelestia.Config
import qs.services
import qs.utils

Searcher {
    id: root

    function transformSearch(search: string): string {
        return search.slice(`${GlobalConfig.launcher.actionPrefix}variant `.length);
    }

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

    list: [
        Variant {
            variant: "vibrant"
            icon: "sentiment_very_dissatisfied"
            name: qsTr("Vibrant")
            description: qsTr("A high chroma palette. The primary palette's chroma is at maximum.")
        },
        Variant {
            variant: "tonalspot"
            icon: "android"
            name: qsTr("Tonal Spot")
            description: qsTr("Default for Material theme colours. A pastel palette with a low chroma.")
        },
        Variant {
            variant: "expressive"
            icon: "compare_arrows"
            name: qsTr("Expressive")
            description: qsTr("A medium chroma palette. The primary palette's hue is different from the seed colour, for variety.")
        },
        Variant {
            variant: "fidelity"
            icon: "compare"
            name: qsTr("Fidelity")
            description: qsTr("Matches the seed colour, even if the seed colour is very bright (high chroma).")
        },
        Variant {
            variant: "content"
            icon: "sentiment_calm"
            name: qsTr("Content")
            description: qsTr("Almost identical to fidelity.")
        },
        Variant {
            variant: "fruitsalad"
            icon: "nutrition"
            name: qsTr("Fruit Salad")
            description: qsTr("A playful theme - the seed colour's hue does not appear in the theme.")
        },
        Variant {
            variant: "rainbow"
            icon: "looks"
            name: qsTr("Rainbow")
            description: qsTr("A playful theme - the seed colour's hue does not appear in the theme.")
        },
        Variant {
            variant: "neutral"
            icon: "contrast"
            name: qsTr("Neutral")
            description: qsTr("Close to grayscale, a hint of chroma.")
        },
        Variant {
            variant: "monochrome"
            icon: "filter_b_and_w"
            name: qsTr("Monochrome")
            description: qsTr("All colours are grayscale, no chroma.")
        }
    ]
    useFuzzy: GlobalConfig.launcher.useFuzzy.variants

    component Variant: QtObject {
        required property string variant
        required property string icon
        required property string name
        required property string description

        function onClicked(list: AppList): void {
            list.screenState.launcher = false;
            Quickshell.execDetached(["caelestia", "scheme", "set", "-v", variant]);
        }
    }
}
