<h1 align=center>caelestia-shell (fork)</h1>

<div align=center>

![GitHub last commit](https://img.shields.io/github/last-commit/osmargm1202/shell?style=for-the-badge&labelColor=101418&color=9ccbfb)
![GitHub Repo stars](https://img.shields.io/github/stars/osmargm1202/shell?style=for-the-badge&labelColor=101418&color=b9c8da)
![GitHub repo size](https://img.shields.io/github/repo-size/osmargm1202/shell?style=for-the-badge&labelColor=101418&color=d3bfe6)

</div>

> **Fork of [caelestia-dots/shell](https://github.com/caelestia-dots/shell).**
> All credits to the original author. This fork applies personal tweaks and NixOS-specific fixes on top of the upstream shell.

## Changes from upstream

| Area | Change |
|------|--------|
| Workspaces | 10 shown by default (upstream: 5), numeric labels instead of icons |
| Volume | Max volume raised to 150% (upstream: 100%) |
| Mic OSD | Microphone slider enabled in OSD by default (upstream: disabled) |
| Audio panel | Mic volume section added to the audio popout, with 100% reference marker |
| Clock | Uses `m3onSurface` color (consistent with bar, upstream used `m3tertiary`) |
| OS icon | Uses `m3primary` color (upstream used `m3tertiary`) |
| Lock screen | Fixed `id: char` → `id: charItem` — `char` is a reserved word in newer quickshell QML engine |

All changes are compiled into the C++ plugin defaults. No extra config file is needed.

## Components

-   Widgets: [`Quickshell`](https://quickshell.outfoxxed.me)
-   Window manager: [`Hyprland`](https://hyprland.org)
-   Dots: [`caelestia`](https://github.com/caelestia-dots)

## NixOS / Home Manager

### Adding the input

In your `flake.nix`:

```nix
{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

    caelestia-shell = {
      url = "github:osmargm1202/shell";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };
}
```

### NixOS profile

In your NixOS profile (e.g. `hyprlandqs-caelestia.nix`):

```nix
{ pkgs, inputs, ... }:

let
  system = pkgs.stdenv.hostPlatform.system;
  caelestiaShell = inputs.caelestia-shell.packages.${system}.with-cli;
in
{
  # Required: lets gsr-kms-server run with cap_sys_admin for screen recording
  programs.gpu-screen-recorder.enable = true;

  environment.systemPackages = with pkgs; [
    # Notification daemon
    mako

    # Launcher
    rofi

    # Screen recorder (used by caelestia record)
    gpu-screen-recorder

    # Video wallpapers
    mpvpaper

    # Fonts
    noto-fonts-color-emoji
    nerd-fonts.jetbrains-mono
    nerd-fonts.symbols-only
  ];

  home-manager.users.<your-username> = {
    imports = [
      inputs.caelestia-shell.homeManagerModules.default
    ];

    programs.caelestia = {
      enable = true;
      package = caelestiaShell;
    };
  };
}
```

> [!WARNING]
> **Do NOT use `programs.caelestia.settings`.**
>
> Using `settings` causes Home Manager to write `~/.config/caelestia/shell.json` as a Nix store symlink.
> The caelestia shell writes to this file at runtime (theme changes, wallpaper, etc.) and **will crash**
> because Nix store paths are read-only.
>
> All defaults in this fork (10 workspaces, 150% volume, mic OSD) are baked into the C++ plugin.
> Configure runtime options by writing `~/.config/caelestia/shell.json` directly from an activation script
> or on first run — do not use the HM `settings` option.

### Minimal activation example (optional)

If you want to pre-seed a `shell.json` without overwriting user changes on rebuild, add this to your
`home.activation` in Home Manager:

```nix
home.activation.caelestiaConfig = lib.hm.dag.entryAfter ["writeBoundary"] ''
  cfg="$HOME/.config/caelestia/shell.json"
  if [ ! -e "$cfg" ]; then
    mkdir -p "$(dirname "$cfg")"
    printf '{}' > "$cfg"
  fi
'';
```

This creates an empty (but writable) `shell.json` only if one does not already exist.

## Installation (non-NixOS)

### Arch Linux

The upstream shell is available from the AUR as `caelestia-shell` or `caelestia-shell-git`.
To use this fork, follow the manual installation section below.

### Manual installation

Dependencies:

-   [`caelestia-cli`](https://github.com/caelestia-dots/cli)
-   [`quickshell-git`](https://quickshell.outfoxxed.me) — git version required, not the latest tag
-   [`ddcutil`](https://github.com/rockowitz/ddcutil)
-   [`brightnessctl`](https://github.com/Hummer12007/brightnessctl)
-   [`libcava`](https://github.com/LukashonakV/cava)
-   [`networkmanager`](https://networkmanager.dev)
-   [`lm-sensors`](https://github.com/lm-sensors/lm-sensors)
-   [`fish`](https://github.com/fish-shell/fish-shell)
-   [`aubio`](https://github.com/aubio/aubio)
-   [`libpipewire`](https://pipewire.org)
-   `glibc`, `qt6-base`, `qt6-declarative`, `gcc-libs`
-   [`material-symbols`](https://fonts.google.com/icons)
-   [`caskaydia-cove-nerd`](https://www.nerdfonts.com/font-downloads)
-   [`swappy`](https://github.com/jtheoof/swappy)
-   [`libqalculate`](https://github.com/Qalculate/libqalculate)
-   [`bash`](https://www.gnu.org/software/bash)

Build dependencies: [`cmake`](https://cmake.org), [`ninja`](https://github.com/ninja-build/ninja)

```sh
cd $XDG_CONFIG_HOME/quickshell
git clone https://github.com/osmargm1202/shell.git caelestia

cd caelestia
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/
cmake --build build
sudo cmake --install build
```

## Usage

```sh
caelestia shell -d
# or
qs -c caelestia
```

### Shortcuts / IPC

All keybinds use Hyprland [global shortcuts](https://wiki.hyprland.org/Configuring/Binds/#dbus-global-shortcuts).

IPC commands via `caelestia shell ...`:

```
$ caelestia shell -s
target drawers
  function toggle(drawer: string): void
  function list(): string
target notifs
  function clear(): void
target lock
  function lock(): void
  function unlock(): void
  function isLocked(): bool
target mpris
  function playPause(): void
  function getActive(prop: string): string
  function next(): void
  function stop(): void
  function play(): void
  function list(): string
  function pause(): void
  function previous(): void
target picker
  function openFreeze(): void
  function open(): void
target wallpaper
  function set(path: string): void
  function get(): string
  function list(): string
```

### Profile picture / Wallpapers

-   Profile picture: `~/.face`
-   Wallpapers directory: `~/Pictures/Wallpapers` (configurable in `shell.json`)
-   Set wallpaper: `caelestia wallpaper -f <path>` or via the launcher

## Configuring

Write options to `~/.config/caelestia/shell.json`. Missing options fall back to compiled defaults.
This file must be a **regular file** (not a symlink) — see the NixOS warning above.

### Per-monitor configuration

`~/.config/caelestia/monitors/<screen-name>/shell.json` overrides the global config for that monitor.

Example — disable bar on DP-1:

```json
{
    "bar": {
        "persistent": false
    }
}
```

> [!NOTE]
> Some options ignore per-monitor overrides. See the
> [upstream README](https://github.com/caelestia-dots/shell#per-monitor-configuration) for the full list.

### Full config reference

<details><summary>Example shell.json with all options</summary>

> [!NOTE]
> Copy only the options you want to change. Do not paste the entire block into your config.

```json
{
    "enabled": true,
    "appearance": {
        "deformScale": 1,
        "rounding": { "scale": 1 },
        "spacing": { "scale": 1 },
        "padding": { "scale": 1 },
        "font": {
            "scale": 1,
            "clock": "Rubik",
            "workspaces": "Rubik",
            "headline": {
                "family": "GoogleSansFlex",
                "large": { "size": 32, "weight": 500, "italic": false, "vaxes": { "ROND": 25 } },
                "medium": { "size": 28, "weight": 500, "italic": false, "vaxes": { "ROND": 25 } },
                "small": { "size": 24, "weight": 500, "italic": false, "vaxes": { "ROND": 25 } }
            },
            "title": {
                "family": "GoogleSansFlex",
                "large": { "size": 22, "weight": 500, "italic": false, "vaxes": { "ROND": 25 } },
                "medium": { "size": 16, "weight": 500, "italic": false, "vaxes": { "ROND": 25 } },
                "small": { "size": 14, "weight": 500, "italic": false, "vaxes": { "ROND": 25 } }
            },
            "body": {
                "family": "GoogleSansFlex",
                "large": { "size": 16, "weight": 400, "italic": false, "vaxes": { "ROND": 25 } },
                "medium": { "size": 14, "weight": 400, "italic": false, "vaxes": { "ROND": 25 } },
                "small": { "size": 12, "weight": 400, "italic": false, "vaxes": { "ROND": 25 } }
            },
            "label": {
                "family": "GoogleSansFlex",
                "large": { "size": 14, "weight": 500, "italic": false, "vaxes": { "ROND": 25 } },
                "medium": { "size": 12, "weight": 500, "italic": false, "vaxes": { "ROND": 25 } },
                "small": { "size": 11, "weight": 400, "italic": false, "vaxes": { "ROND": 25 } }
            },
            "mono": {
                "family": "CaskaydiaCove NF",
                "large": { "size": 16, "weight": 400, "italic": false, "vaxes": {} },
                "medium": { "size": 14, "weight": 400, "italic": false, "vaxes": {} },
                "small": { "size": 12, "weight": 400, "italic": false, "vaxes": {} }
            },
            "icon": {
                "family": "Material Symbols Rounded",
                "extraLarge": { "size": 36, "weight": 400, "italic": false, "vaxes": {} },
                "large": { "size": 24, "weight": 400, "italic": false, "vaxes": {} },
                "medium": { "size": 18, "weight": 400, "italic": false, "vaxes": {} },
                "small": { "size": 15, "weight": 400, "italic": false, "vaxes": {} }
            }
        },
        "anim": { "durations": { "scale": 1 } },
        "transparency": { "enabled": false, "base": 0.85, "layers": 0.4 }
    },
    "general": {
        "logo": "",
        "showOverFullscreen": false,
        "mediaGifSpeedAdjustment": 300,
        "sessionGifSpeed": 0.7,
        "apps": {
            "terminal": ["foot"],
            "audio": ["pavucontrol"],
            "playback": ["mpv"],
            "explorer": ["thunar"]
        },
        "idle": {
            "lockBeforeSleep": true,
            "inhibitWhenAudio": true,
            "timeouts": [
                { "timeout": 180, "idleAction": "lock" },
                { "timeout": 300, "idleAction": "dpms off", "returnAction": "dpms on" },
                { "timeout": 600, "idleAction": ["suspendThenHibernate"] }
            ]
        },
        "battery": {
            "warnLevels": [
                { "level": 20, "title": "Low battery", "message": "You might want to plug in a charger", "icon": "battery_android_frame_2" },
                { "level": 10, "title": "Did you see the previous message?", "message": "You should probably plug in a charger <b>now</b>", "icon": "battery_android_frame_1" },
                { "level": 5, "title": "Critical battery level", "message": "PLUG THE CHARGER RIGHT NOW!!", "icon": "battery_android_alert", "critical": true }
            ],
            "criticalLevel": 3
        }
    },
    "background": {
        "enabled": true,
        "wallpaperEnabled": true,
        "desktopClock": {
            "enabled": false,
            "scale": 1.0,
            "position": "bottom-right",
            "invertColors": false,
            "background": { "enabled": false, "opacity": 0.7, "blur": true },
            "shadow": { "enabled": true, "opacity": 0.7, "blur": 0.4 }
        },
        "visualiser": { "enabled": false, "autoHide": true, "blur": false, "rounding": 1, "spacing": 1 }
    },
    "bar": {
        "persistent": true,
        "showOnHover": true,
        "dragThreshold": 20,
        "scrollActions": { "workspaces": true, "volume": true, "brightness": true },
        "popouts": { "activeWindow": true, "tray": true, "statusIcons": true },
        "workspaces": {
            "shown": 10,
            "activeIndicator": true,
            "occupiedBg": false,
            "showWindows": true,
            "showWindowsOnSpecialWorkspaces": true,
            "maxWindowIcons": 5,
            "activeTrail": false,
            "perMonitorWorkspaces": true,
            "label": "",
            "occupiedLabel": "",
            "activeLabel": "",
            "capitalisation": "preserve",
            "specialWorkspaceIcons": [{ "name": "steam", "icon": "sports_esports" }],
            "windowIcons": [{ "regex": "steam(_app_(default|[0-9]+))?", "icon": "sports_esports" }]
        },
        "activeWindow": { "compact": false, "inverted": false, "showOnHover": true },
        "tray": { "background": false, "recolour": false, "compact": false, "iconSubs": [], "hiddenIcons": [] },
        "status": {
            "showAudio": false, "showMicrophone": false, "showKbLayout": false,
            "showNetwork": true, "showWifi": true, "showBluetooth": true,
            "showBattery": true, "showLockStatus": true
        },
        "clock": { "background": false, "showDate": false, "showIcon": true },
        "entries": [
            { "id": "logo", "enabled": true },
            { "id": "workspaces", "enabled": true },
            { "id": "spacer", "enabled": true },
            { "id": "activeWindow", "enabled": true },
            { "id": "spacer", "enabled": true },
            { "id": "tray", "enabled": true },
            { "id": "clock", "enabled": true },
            { "id": "statusIcons", "enabled": true },
            { "id": "power", "enabled": true }
        ],
        "excludedScreens": []
    },
    "border": { "thickness": 10, "rounding": 25, "smoothing": 20 },
    "dashboard": {
        "enabled": true,
        "showOnHover": true,
        "showDashboard": true,
        "showMedia": true,
        "showPerformance": true,
        "showWeather": true,
        "mediaUpdateInterval": 500,
        "resourceUpdateInterval": 1000,
        "dragThreshold": 50,
        "performance": {
            "showBattery": true, "showGpu": true, "showCpu": true,
            "showMemory": true, "showStorage": true, "showNetwork": true
        }
    },
    "launcher": {
        "enabled": true,
        "showOnHover": false,
        "maxShown": 7,
        "maxWallpapers": 9,
        "specialPrefix": "@",
        "actionPrefix": ">",
        "enableDangerousActions": false,
        "dragThreshold": 50,
        "vimKeybinds": false,
        "favouriteApps": [],
        "hiddenApps": [],
        "useFuzzy": { "apps": false, "actions": false, "schemes": false, "variants": false, "wallpapers": false },
        "actions": [
            { "name": "Calculator", "icon": "calculate", "description": "Do simple math equations (powered by Qalc)", "command": ["autocomplete", "calc"], "enabled": true, "dangerous": false },
            { "name": "Scheme", "icon": "palette", "description": "Change the current colour scheme", "command": ["autocomplete", "scheme"], "enabled": true, "dangerous": false },
            { "name": "Wallpaper", "icon": "image", "description": "Change the current wallpaper", "command": ["autocomplete", "wallpaper"], "enabled": true, "dangerous": false },
            { "name": "Variant", "icon": "colors", "description": "Change the current scheme variant", "command": ["autocomplete", "variant"], "enabled": true, "dangerous": false },
            { "name": "Random", "icon": "casino", "description": "Switch to a random wallpaper", "command": ["caelestia", "wallpaper", "-r"], "enabled": true, "dangerous": false },
            { "name": "Light", "icon": "light_mode", "description": "Change the scheme to light mode", "command": ["setMode", "light"], "enabled": true, "dangerous": false },
            { "name": "Dark", "icon": "dark_mode", "description": "Change the scheme to dark mode", "command": ["setMode", "dark"], "enabled": true, "dangerous": false },
            { "name": "Shutdown", "icon": "power_settings_new", "description": "Shutdown the system", "command": ["poweroff"], "enabled": true, "dangerous": true },
            { "name": "Reboot", "icon": "cached", "description": "Reboot the system", "command": ["reboot"], "enabled": true, "dangerous": true },
            { "name": "Logout", "icon": "exit_to_app", "description": "Log out of the current session", "command": ["logout"], "enabled": true, "dangerous": true },
            { "name": "Lock", "icon": "lock", "description": "Lock the current session", "command": ["loginctl", "lock-session"], "enabled": true, "dangerous": false },
            { "name": "Sleep", "icon": "bedtime", "description": "Suspend then hibernate", "command": ["suspendThenHibernate"], "enabled": true, "dangerous": false },
            { "name": "Settings", "icon": "settings", "description": "Configure the shell", "command": ["caelestia", "shell", "nexus", "open"], "enabled": true, "dangerous": false }
        ]
    },
    "lock": { "recolourLogo": true, "enableFprint": true, "maxFprintTries": 3, "hideNotifs": false },
    "nexus": { "wallpapersPerRow": 4, "networkRescanInterval": 15000 },
    "notifs": {
        "expire": true,
        "fullscreen": "on",
        "defaultExpireTimeout": 5000,
        "fullscreenExpireTimeout": 2000,
        "clearThreshold": 0.3,
        "expandThreshold": 20,
        "actionOnClick": false,
        "groupPreviewNum": 3,
        "openExpanded": false
    },
    "osd": {
        "enabled": true,
        "hideDelay": 2000,
        "enableBrightness": true,
        "enableMicrophone": true
    },
    "services": {
        "weatherLocation": "",
        "useFahrenheit": false,
        "useFahrenheitPerformance": false,
        "useTwelveHourClock": false,
        "gpuType": "",
        "visualiserBars": 60,
        "audioIncrement": 0.1,
        "brightnessIncrement": 0.1,
        "maxVolume": 1.5,
        "smartScheme": true,
        "defaultPlayer": "Spotify",
        "playerAliases": [{ "from": "com.github.th_ch.youtube_music", "to": "YT Music" }],
        "lyricsBackend": "Auto"
    },
    "session": {
        "enabled": true,
        "dragThreshold": 30,
        "vimKeybinds": false,
        "icons": { "logout": "logout", "shutdown": "power_settings_new", "hibernate": "downloading", "reboot": "cached" },
        "commands": { "logout": ["logout"], "shutdown": ["poweroff"], "hibernate": ["hibernate"], "reboot": ["reboot"] }
    },
    "sidebar": { "enabled": true, "showOnHover": false, "minHoverThreshold": 200, "dragThreshold": 80 },
    "utilities": {
        "enabled": true,
        "maxToasts": 4,
        "toasts": {
            "fullscreen": "off",
            "configLoaded": true,
            "chargingChanged": true,
            "gameModeChanged": true,
            "dndChanged": true,
            "audioOutputChanged": true,
            "audioInputChanged": true,
            "capsLockChanged": true,
            "numLockChanged": true,
            "kbLayoutChanged": true,
            "kbLimit": true,
            "vpnChanged": true,
            "nowPlaying": false
        },
        "vpn": {
            "enabled": false,
            "provider": [{ "name": "wireguard", "interface": "your-connection-name", "displayName": "Wireguard (Your VPN)", "enabled": false }]
        },
        "quickToggles": [
            { "id": "wifi", "enabled": true },
            { "id": "bluetooth", "enabled": true },
            { "id": "mic", "enabled": true },
            { "id": "settings", "enabled": true },
            { "id": "gameMode", "enabled": true },
            { "id": "dnd", "enabled": true },
            { "id": "vpn", "enabled": false }
        ]
    },
    "paths": {
        "wallpaperDir": "~/Pictures/Wallpapers",
        "lyricsDir": "~/Music/lyrics/",
        "sessionGif": "root:/assets/kurukuru.gif",
        "mediaGif": "root:/assets/bongocat.gif",
        "noNotifsPic": "root:/assets/dino.png",
        "lockNoNotifsPic": "root:/assets/dino.png"
    }
}
```

</details>

### Advanced configuration

> [!WARNING]
> Do NOT change these options unless you know what you are doing. They control internal tokens
> and can cause visual issues. Their existence is not guaranteed across versions.

`~/.config/caelestia/shell-tokens.json` allows editing internal tokens (rounding, spacing, padding,
font sizes, animation curves) without touching source code. Per-monitor overrides available at
`~/.config/caelestia/monitors/<screen-name>/shell-tokens.json`.

## FAQ

### Screen flickering

Disable VRR in `~/.config/caelestia/hypr-user.conf`:

```conf
misc {
    vrr = 0
}
```

### Custom hyprland config

Add to `~/.config/caelestia/hypr-user.conf`.

### Colour scheme follows wallpaper

```sh
caelestia wallpaper -f <path/to/file>
caelestia scheme set -n dynamic
```

### Wallpapers not showing in launcher

Default directory is `~/Pictures/Wallpapers`. Launcher shows an odd number — if you have 2 wallpapers, add one more.

## Credits

Fork of [caelestia-dots/shell](https://github.com/caelestia-dots/shell) by the original caelestia author.

Thanks to [@outfoxxed](https://github.com/outfoxxed) for Quickshell and to the Hyprland community.
