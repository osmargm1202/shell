pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Caelestia.Config
import qs.components
import qs.services
import qs.modules.nexus.common

Rectangle {
    id: root

    required property string themeId
    required property string themeName
    required property bool   active
    required property string previewPrimary
    required property string previewSecondary
    required property string previewBg
    required property string previewSurface

    signal clicked()

    implicitHeight: 60
    radius: 12
    color: Colours.palette.m3surfaceContainerHigh
    border.width: 2
    border.color: active ? Colours.palette.m3primary : "transparent"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // 2×2 colour swatch
        Grid {
            columns: 2
            spacing: 3

            Repeater {
                model: [root.previewPrimary, root.previewSecondary,
                        root.previewBg,      root.previewSurface]
                delegate: Rectangle {
                    required property string modelData
                    width: 18; height: 18; radius: 3
                    color: "#" + modelData
                }
            }
        }

        StyledText {
            Layout.fillWidth: true
            text: root.themeName
            font: Tokens.font.body.medium
            color: Colours.palette.m3onSurface
        }

        MaterialIcon {
            text: "check_circle"
            fontStyle: Tokens.font.icon.small
            color: Colours.palette.m3primary
            visible: root.active
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
