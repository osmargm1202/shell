pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell.Services.Pipewire
import Caelestia.Config
import qs.components
import qs.components.controls
import qs.services

Item {
    id: root

    required property PopoutState popouts

    implicitWidth: layout.implicitWidth + Tokens.padding.medium * 2
    implicitHeight: layout.implicitHeight + Tokens.padding.medium * 2

    ButtonGroup {
        id: sinks
    }

    ButtonGroup {
        id: sources
    }

    ColumnLayout {
        id: layout

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: Tokens.spacing.medium

        StyledText {
            text: qsTr("Output device")
            font: Tokens.font.body.builders.medium.weight(Font.Medium).build()
        }

        Repeater {
            model: Audio.sinks

            StyledRadioButton {
                id: control

                required property PwNode modelData

                ButtonGroup.group: sinks
                checked: Audio.sink?.id === modelData.id
                onClicked: Audio.setAudioSink(modelData)
                text: modelData.description
            }
        }

        StyledText {
            Layout.topMargin: Tokens.spacing.medium
            text: qsTr("Input device")
            font: Tokens.font.body.builders.medium.weight(Font.Medium).build()
        }

        Repeater {
            model: Audio.sources

            StyledRadioButton {
                required property PwNode modelData

                ButtonGroup.group: sources
                checked: Audio.source?.id === modelData.id
                onClicked: Audio.setAudioSource(modelData)
                text: modelData.description
            }
        }

        StyledText {
            Layout.topMargin: Tokens.spacing.medium
            text: qsTr("Volume (%1)").arg(Audio.muted ? qsTr("Muted") : `${Math.round(Audio.volume * 100)}%`)
            font: Tokens.font.body.builders.medium.weight(Font.Medium).build()
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: Tokens.padding.medium * 3

            CustomMouseArea {
                anchors.fill: parent

                onWheel: event => {
                    if (event.angleDelta.y > 0)
                        Audio.incrementVolume();
                    else if (event.angleDelta.y < 0)
                        Audio.decrementVolume();
                }

                StyledSlider {
                    anchors.fill: parent

                    readonly property real maxVol: GlobalConfig.services.maxVolume

                    value: maxVol > 1 ? Audio.volume / maxVol : Audio.volume
                    onInteraction: v => Audio.setVolume(maxVol > 1 ? v * maxVol : v)
                }

                // 100% marker — visible only when maxVolume > 1
                Rectangle {
                    visible: GlobalConfig.services.maxVolume > 1.0
                    anchors.verticalCenter: parent.verticalCenter
                    x: parent.width * (1.0 / GlobalConfig.services.maxVolume) - 1
                    width: 2
                    height: parent.implicitHeight
                    radius: 1
                    color: Colours.palette.m3outline
                }
            }
        }

        StyledText {
            Layout.topMargin: Tokens.spacing.medium
            text: qsTr("Mic Volume (%1)").arg(Audio.sourceMuted ? qsTr("Muted") : `${Math.round(Audio.sourceVolume * 100)}%`)
            font: Tokens.font.body.builders.medium.weight(Font.Medium).build()
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: Tokens.padding.medium * 3

            CustomMouseArea {
                anchors.fill: parent

                onWheel: event => {
                    if (event.angleDelta.y > 0)
                        Audio.incrementSourceVolume();
                    else if (event.angleDelta.y < 0)
                        Audio.decrementSourceVolume();
                }

                StyledSlider {
                    anchors.fill: parent

                    readonly property real maxVol: GlobalConfig.services.maxVolume

                    value: maxVol > 1 ? Audio.sourceVolume / maxVol : Audio.sourceVolume
                    onInteraction: v => Audio.setSourceVolume(maxVol > 1 ? v * maxVol : v)
                }

                Rectangle {
                    visible: GlobalConfig.services.maxVolume > 1.0
                    anchors.verticalCenter: parent.verticalCenter
                    x: parent.width * (1.0 / GlobalConfig.services.maxVolume) - 1
                    width: 2
                    height: parent.implicitHeight
                    radius: 1
                    color: Colours.palette.m3outline
                }
            }
        }

        IconTextButton {
            Layout.fillWidth: true
            Layout.topMargin: Tokens.spacing.medium
            inactiveColour: Colours.palette.m3primaryContainer
            inactiveOnColour: Colours.palette.m3onPrimaryContainer
            verticalPadding: Tokens.padding.extraSmall
            text: qsTr("Open settings")
            icon: "settings"

            onClicked: root.popouts.detachRequested("audio")
        }
    }
}
