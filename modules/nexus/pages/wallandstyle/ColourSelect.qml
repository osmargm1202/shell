pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Caelestia.Config
import qs.components
import qs.services
import qs.modules.nexus.common

PageBase {
    id: root

    title: qsTr("Colours")
    isSubPage: true

    readonly property bool isCustom: ThemeEngine.activeTheme !== "dynamic"

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 0

        // ── Section 1: Colour Source ──────────────────────────────

        StyledText {
            Layout.bottomMargin: 8
            text: qsTr("COLOUR SOURCE")
            font: Tokens.font.label.small
            color: Colours.palette.m3primary
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 20
            spacing: 8

            // From Wallpaper card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 76
                radius: 12
                color: !root.isCustom
                    ? Colours.palette.m3surfaceContainerHigh
                    : Colours.palette.m3surfaceContainer
                border.width: 2
                border.color: !root.isCustom ? Colours.palette.m3primary : "transparent"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 4

                    MaterialIcon {
                        Layout.alignment: Qt.AlignHCenter
                        text: "wallpaper"
                        fontStyle: Tokens.font.icon.medium
                        color: !root.isCustom
                            ? Colours.palette.m3primary
                            : Colours.palette.m3onSurfaceVariant
                    }
                    StyledText {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("From Wallpaper")
                        font: Tokens.font.body.small
                        color: Colours.palette.m3onSurface
                    }
                    StyledText {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("auto · matugen")
                        font: Tokens.font.label.small
                        color: Colours.palette.m3onSurfaceVariant
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: ThemeEngine.selectDynamic()
                }
            }

            // Custom Theme card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 76
                radius: 12
                color: root.isCustom
                    ? Colours.palette.m3surfaceContainerHigh
                    : Colours.palette.m3surfaceContainer
                border.width: 2
                border.color: root.isCustom ? Colours.palette.m3primary : "transparent"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 4

                    MaterialIcon {
                        Layout.alignment: Qt.AlignHCenter
                        text: "palette"
                        fontStyle: Tokens.font.icon.medium
                        color: root.isCustom
                            ? Colours.palette.m3primary
                            : Colours.palette.m3onSurfaceVariant
                    }
                    StyledText {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Custom Theme")
                        font: Tokens.font.body.small
                        color: Colours.palette.m3onSurface
                    }
                    StyledText {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.isCustom ? qsTr("active") : qsTr("select below")
                        font: Tokens.font.label.small
                        color: root.isCustom
                            ? Colours.palette.m3primary
                            : Colours.palette.m3onSurfaceVariant
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (ThemeEngine.themeList.length > 0)
                            ThemeEngine.selectTheme(ThemeEngine.themeList[0].id)
                    }
                }
            }
        }

        // ── Section 2: Theme Grid (visible only when Custom is active) ───

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.isCustom
            spacing: 8

            StyledText {
                Layout.bottomMargin: 4
                text: qsTr("THEMES")
                font: Tokens.font.label.small
                color: Colours.palette.m3primary
            }

            Repeater {
                model: ThemeEngine.themeList
                ThemeCard {
                    required property var modelData
                    Layout.fillWidth: true

                    themeId:   modelData.id
                    themeName: modelData.name
                    active:    ThemeEngine.activeTheme === modelData.id

                    // Swatch colours update live when mode toggles
                    previewPrimary:   Colours.light
                        ? modelData.preview.light.primary
                        : modelData.preview.dark.primary
                    previewSecondary: Colours.light
                        ? modelData.preview.light.secondary
                        : modelData.preview.dark.secondary
                    previewBg:        Colours.light
                        ? modelData.preview.light.bg
                        : modelData.preview.dark.bg
                    previewSurface:   Colours.light
                        ? modelData.preview.light.surface
                        : modelData.preview.dark.surface

                    onClicked: ThemeEngine.selectTheme(modelData.id)
                }
            }

            // Status row: per-mode current selections
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 8
                implicitHeight: 52
                radius: 8
                color: Colours.palette.m3surfaceContainerLow

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 4

                    RowLayout {
                        StyledText {
                            text: "🌙 " + qsTr("Dark")
                            font: Tokens.font.label.small
                            color: Colours.palette.m3onSurfaceVariant
                        }
                        Item { Layout.fillWidth: true }
                        StyledText {
                            text: ThemeEngine.darkTheme === "dynamic"
                                  ? qsTr("Wallpaper") : ThemeEngine.darkTheme
                            font: Tokens.font.label.small
                            color: Colours.palette.m3primary
                        }
                    }

                    RowLayout {
                        StyledText {
                            text: "☀️ " + qsTr("Light")
                            font: Tokens.font.label.small
                            color: Colours.palette.m3onSurfaceVariant
                        }
                        Item { Layout.fillWidth: true }
                        StyledText {
                            text: ThemeEngine.lightTheme === "dynamic"
                                  ? qsTr("Wallpaper") : ThemeEngine.lightTheme
                            font: Tokens.font.label.small
                            color: Colours.palette.m3primary
                        }
                    }
                }
            }
        }
    }
}
