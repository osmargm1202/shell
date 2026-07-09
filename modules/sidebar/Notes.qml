import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Quickshell
import Quickshell.Io
import Caelestia
import Caelestia.Config
import qs.components
import qs.components.controls
import qs.services
import qs.utils

Item {
    id: root

    property bool loaded: false

    readonly property color cBgHigh: Colours.tPalette.m3surfaceContainerHigh
    readonly property color cOnSurface: Colours.palette.m3onSurface
    readonly property color cOnSurfaceVariant: Colours.palette.m3onSurfaceVariant

    FileView {
        id: storage

        printErrors: false
        path: `${Paths.state}/notes.txt`
        onLoaded: {
            textArea.text = text();
            root.loaded = true;
        }
        onLoadFailed: err => {
            if (err === FileViewError.FileNotFound) {
                root.loaded = true;
                Qt.callLater(() => setText(""));
            }
        }
    }

    Timer {
        id: saveDelay

        interval: 500
        onTriggered: storage.setText(textArea.text)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.padding.medium
        spacing: Tokens.spacing.medium

        StyledText {
            Layout.fillWidth: true
            text: qsTr("Notes")
            font: Tokens.font.title.medium
            color: root.cOnSurface
        }

        StyledRect {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Tokens.rounding.medium
            color: root.cBgHigh

            Flickable {
                id: flick

                anchors.fill: parent
                anchors.margins: Tokens.padding.medium
                contentWidth: width
                contentHeight: textArea.implicitHeight
                clip: true

                ScrollBar.vertical: StyledScrollBar { flickable: flick }

                TextArea.flickable: TextArea {
                    id: textArea

                    enabled: root.loaded
                    wrapMode: TextArea.Wrap
                    color: root.cOnSurface
                    placeholderText: qsTr("Type anything — saved automatically")
                    placeholderTextColor: root.cOnSurfaceVariant
                    background: null
                    font: Tokens.font.body.medium

                    onTextChanged: {
                        if (root.loaded)
                            saveDelay.restart();
                    }
                }
            }
        }
    }
}
