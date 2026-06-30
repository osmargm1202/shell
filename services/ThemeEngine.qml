pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Io
import qs.services

Singleton {
    id: root

    // Per-mode selections ("dynamic" = From Wallpaper, any other = theme id)
    property alias darkTheme: props.darkTheme
    property alias lightTheme: props.lightTheme

    // Hard-coded theme list for v1 (one theme: Teal)
    readonly property var themeList: [
        {
            "id": "teal",
            "name": "Teal",
            "preview": {
                "dark":  { "primary": "62bba2", "secondary": "abcec1",
                           "bg": "101413", "surface": "1c211f" },
                "light": { "primary": "22836c", "secondary": "45655a",
                           "bg": "f6faf7", "surface": "ebefeb" }
            }
        }
    ]

    // Reacts to both mode toggles and theme selection changes
    readonly property string activeTheme: Colours.light ? lightTheme : darkTheme

    // Guard: prevents _applyToSystem firing during initialization
    property bool _ready: false

    PersistentProperties {
        id: props

        property string darkTheme: "dynamic"
        property string lightTheme: "dynamic"

        reloadableId: "themeEngine"
    }

    Component.onCompleted: {
        // PersistentProperties auto-loads saved values; just mark ready and apply
        _ready = true
        // Apply predefined theme on startup (dynamic re-applies itself via wallpaper)
        if (activeTheme !== "dynamic") _applyToSystem()
    }

    onActiveThemeChanged: {
        if (_ready) _applyToSystem()
    }

    // Called from ColourSelect when user clicks a theme card
    function selectTheme(id) {
        if (Colours.light) props.lightTheme = id
        else props.darkTheme = id
        // Reactive: alias change → activeTheme recomputes → onActiveThemeChanged → _applyToSystem()
    }

    // Called from ColourSelect when user clicks "From Wallpaper"
    function selectDynamic() {
        props.darkTheme = "dynamic"
        props.lightTheme = "dynamic"
        // Reactive: alias change → activeTheme recomputes → onActiveThemeChanged → _applyToSystem()
    }

    function _applyToSystem() {
        const theme = Colours.light ? lightTheme : darkTheme
        const mode  = Colours.light ? "light" : "dark"
        if (theme === "dynamic") {
            Quickshell.execDetached(["caelestia", "scheme", "set", "-n", "dynamic"])
        } else {
            Quickshell.execDetached(["caelestia", "scheme", "set",
                                     "-n", theme, "-f", "default", "-m", mode])
        }
    }
}
