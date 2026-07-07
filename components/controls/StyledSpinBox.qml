import QtQuick
import QtQuick.Templates as T
import Caelestia.Config
import qs.components
import qs.services

// QtQuick.Templates.DoubleSpinBox only exists on Qt 6.11+; nixpkgs pins Qt 6.10.2
// for caelestia-shell, so this wraps a plain (int) SpinBox and scales real values
// by 10^decimals internally instead.
Item {
    id: root

    property real value
    property real from: 0
    property real to: 99
    property real stepSize: 1
    property int repeatRate: 400
    property int repeatDecay: 50
    property int cLayer: 1
    property bool editable: true

    signal valueModified

    readonly property int decimals: stepSize < 1 ? Math.max(1, Math.ceil(-Math.log10(stepSize))) : 0
    readonly property real factor: Math.pow(10, decimals)

    function increase(): void {
        let newValue = Math.min(to, value + stepSize);
        newValue = Math.round(newValue * factor) / factor;
        value = newValue;
        valueModified();
    }

    function decrease(): void {
        let newValue = Math.max(from, value - stepSize);
        newValue = Math.round(newValue * factor) / factor;
        value = newValue;
        valueModified();
    }

    implicitWidth: spin.implicitWidth
    implicitHeight: spin.implicitHeight

    T.SpinBox {
        id: spin

        anchors.fill: parent

        from: Math.round(root.from * root.factor)
        to: Math.round(root.to * root.factor)
        stepSize: Math.max(1, Math.round(root.stepSize * root.factor))
        value: Math.round(root.value * root.factor)

        editable: root.editable
        spacing: Tokens.spacing.small

        implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
        implicitHeight: Math.max(up.indicator.implicitHeight, down.indicator.implicitHeight, contentItem.implicitHeight) + topPadding + bottomPadding

        leftPadding: up.indicator.implicitWidth + Tokens.spacing.extraSmall / 2
        rightPadding: down.indicator.implicitWidth + Tokens.spacing.extraSmall / 2

        textFromValue: function (value, locale) {
            return Number(value / root.factor).toLocaleString(locale, "f", root.decimals);
        }

        valueFromText: function (text, locale) {
            return Math.round(Number.fromLocaleString(locale, text) * root.factor);
        }

        validator: DoubleValidator {
            bottom: Math.min(spin.from, spin.to) / root.factor
            top: Math.max(spin.from, spin.to) / root.factor
            decimals: root.decimals
            notation: DoubleValidator.StandardNotation
        }

        onValueModified: {
            root.value = value / root.factor;
            root.valueModified();
        }

        contentItem: TextFieldBase {
            text: spin.textFromValue(spin.value, spin.locale)

            readOnly: !spin.editable
            validator: spin.validator
            inputMethodHints: Qt.ImhFormattedNumbersOnly

            leftPadding: Tokens.padding.medium
            rightPadding: Tokens.padding.medium

            implicitWidth: 65
            horizontalAlignment: TextField.AlignHCenter

            background: StyledRect {
                radius: Tokens.rounding.extraSmall
                color: Colours.layer(Colours.palette.m3surfaceContainerHighest, root.cLayer)
            }
        }

        down.indicator: IconButton {
            id: downButton

            topRightRadius: pressed ? Tokens.rounding.small : Tokens.rounding.extraSmall
            bottomRightRadius: pressed ? Tokens.rounding.small : Tokens.rounding.extraSmall

            icon: "remove"
            disabledColour: Qt.alpha(Colours.palette.m3surfaceContainerHighest, 0.4)
            color: disabled ? disabledColour : Colours.layer(Colours.palette.m3surfaceContainerHighest, root.cLayer)
            type: IconButton.Text
            padding: Tokens.padding.extraSmall
            isRound: true
            label.anchors.horizontalCenterOffset: pressed ? 0 : 2
            disabled: !enabled

            Behavior on topRightRadius {
                Anim {
                    type: Anim.DefaultEffects
                }
            }

            Behavior on bottomRightRadius {
                Anim {
                    type: Anim.DefaultEffects
                }
            }

            Behavior on label.anchors.horizontalCenterOffset {
                Anim {
                    type: Anim.DefaultEffects
                }
            }
        }

        up.indicator: IconButton {
            id: upButton

            anchors.right: parent.right

            topLeftRadius: pressed ? Tokens.rounding.small : Tokens.rounding.extraSmall
            bottomLeftRadius: pressed ? Tokens.rounding.small : Tokens.rounding.extraSmall

            icon: "add"
            disabledColour: Qt.alpha(Colours.palette.m3surfaceContainerHighest, 0.4)
            color: disabled ? disabledColour : Colours.layer(Colours.palette.m3surfaceContainerHighest, root.cLayer)
            type: IconButton.Text
            padding: Tokens.padding.extraSmall
            isRound: true
            label.anchors.horizontalCenterOffset: pressed ? 0 : -2
            disabled: !enabled

            Behavior on topLeftRadius {
                Anim {
                    type: Anim.DefaultEffects
                }
            }

            Behavior on bottomLeftRadius {
                Anim {
                    type: Anim.DefaultEffects
                }
            }

            Behavior on label.anchors.horizontalCenterOffset {
                Anim {
                    type: Anim.DefaultEffects
                }
            }
        }

        Timer {
            id: timer

            running: upButton.pressed || downButton.pressed
            onRunningChanged: {
                if (!running)
                    interval = root.repeatRate;
            }

            interval: root.repeatRate
            repeat: true
            triggeredOnStart: true
            onTriggered: {
                if (upButton.pressed)
                    root.increase();
                else if (downButton.pressed)
                    root.decrease();
                if (interval > root.repeatDecay)
                    interval -= root.repeatDecay;
            }
        }
    }
}
