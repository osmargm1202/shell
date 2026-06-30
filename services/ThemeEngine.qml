pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Caelestia.Config
import qs.services

Singleton {
    id: root

    // Per-mode selections ("dynamic" = From Wallpaper, any other = theme id)
    property string darkTheme: "dynamic"
    property string lightTheme: "dynamic"

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

    Component.onCompleted: {
        const c = GlobalConfig.colours
        if (c) {
            darkTheme = c.darkTheme ?? "dynamic"
            lightTheme = c.lightTheme ?? "dynamic"
        }
        _ready = true
        // Apply predefined theme on startup (dynamic re-applies itself via wallpaper)
        if (activeTheme !== "dynamic") _applyToSystem()
    }

    onActivethemeChanged: {
        if (_ready) _applyToSystem()
    }

    // Called from ColourSelect when user clicks a theme card
    function selectTheme(id) {
        if (Colours.light) lightTheme = id
        else darkTheme = id
        _savePrefs()
        _applyToSystem()
    }

    // Called from ColourSelect when user clicks "From Wallpaper"
    function selectDynamic() {
        darkTheme = "dynamic"
        lightTheme = "dynamic"
        _savePrefs()
        _applyToSystem()
    }

    function _savePrefs() {
        GlobalConfig.colours = { "darkTheme": darkTheme, "lightTheme": lightTheme }
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
