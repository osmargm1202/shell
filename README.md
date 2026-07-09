<h1 align=center>caelestia-shell</h1>

<div align=center>

![GitHub last commit](https://img.shields.io/github/last-commit/osmargm1202/shell?style=for-the-badge&labelColor=101418&color=9ccbfb)
![GitHub repo size](https://img.shields.io/github/repo-size/osmargm1202/shell?style=for-the-badge&labelColor=101418&color=d3bfe6)

</div>

> [!NOTE]
> This is a fork building toward a unified `caelestia-shell` that consolidates the best ideas scattered
> across the community's many forks, on top of one base, instead of living with just one variant.
> Built on [dim-ghub/caelestia-shell](https://github.com/dim-ghub/caelestia-shell), itself a fork of the
> original [caelestia-dots/shell](https://github.com/caelestia-dots/shell) by
> [@soramanew](https://github.com/soramanew).

<!-- Demo video: coming soon -->

## About this fork

Features are ported in one at a time, sourced directly from other community forks where the idea
already exists and works, instead of rewriting things from scratch. Implementation work is AI-assisted.

This fork currently inherits everything from `dim-ghub/caelestia-shell` (GIF/video wallpapers with
Wallhaven integration, extra launchers, redesigned dock and dashboard, and more — see its
[README](https://github.com/dim-ghub/caelestia-shell) for the full list). Additional features being
pulled in from other forks will be listed here as they land.

## Installation

> [!NOTE]
> This repo is for the desktop shell of the caelestia dots. If you want installation instructions
> for the entire dots, head to [the main repo](https://github.com/caelestia-dots/caelestia) instead.

### Arch Linux / Manual (this fork)

Dependencies:

-   [`caelestia-cli` (this fork is recommended and required for some features to work)](https://github.com/dim-ghub/caelestia-cli)
-   [`quickshell-git`](https://quickshell.outfoxxed.me) - this has to be the git version, not the latest tagged version
-   [`ddcutil`](https://github.com/rockowitz/ddcutil)
-   [`brightnessctl`](https://github.com/Hummer12007/brightnessctl)
-   [`app2unit`](https://github.com/Vladimir-csp/app2unit)
-   [`libcava`](https://github.com/LukashonakV/cava)
-   [`networkmanager`](https://networkmanager.dev)
-   [`lm-sensors`](https://github.com/lm-sensors/lm-sensors)
-   [`fish`](https://github.com/fish-shell/fish-shell)
-   [`aubio`](https://github.com/aubio/aubio)
-   [`libpipewire`](https://pipewire.org)
-   [`ffmpeg`](https://ffmpeg.org) - used to generate thumbnails for video wallpapers
-   [`curl`](https://curl.se) - used to fetch the NixOS news (r/NixOS) sidebar feed
-   `glibc`
-   `qt6-declarative`
-   `gcc-libs`
-   [`material-symbols`](https://fonts.google.com/icons)
-   [`caskaydia-cove-nerd`](https://www.nerdfonts.com/font-downloads)
-   [`swappy`](https://github.com/jtheoof/swappy)
-   [`libqalculate`](https://github.com/Qalculate/libqalculate)
-   [`bash`](https://www.gnu.org/software/bash)
-   `qt6-base`
-   `qt6-declarative`
Build dependencies:

-   [`cmake`](https://cmake.org)
-   [`ninja`](https://github.com/ninja-build/ninja)

To install the shell, first install the `pkgit` package manager (available on the AUR as `pkgit-git`).
Then you can simply install the shell directly from GitHub without cloning it:

```sh
pkgit -i https://github.com/osmargm1202/shell
```

> [!TIP]
> You can also use `pkgit -qi https://github.com/osmargm1202/shell` for a quiet installation.

If you prefer to clone and install it manually:

```sh
cd $XDG_CONFIG_HOME/quickshell
git clone https://github.com/osmargm1202/shell.git caelestia
cd caelestia
pkgit -i .
```

### Nix

You can run the shell directly via `nix run`:

```sh
nix run github:osmargm1202/shell
```

Or add it to your system configuration:

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

The package is available as `caelestia-shell.packages.<system>.default`, which can be added to your
`environment.systemPackages`, `users.users.<username>.packages`, `home.packages` if using home-manager,
or a devshell. The shell can then be run via `caelestia-shell`.

> [!TIP]
> The default package does not have the CLI enabled by default, which is required for full funcionality.
> To enable the CLI, use the `with-cli` package.

For home-manager, you can also use the Caelestia's home manager module (explained in [configuring](https://github.com/caelestia-dots/shell?tab=readme-ov-file#home-manager-module)) that installs and configures the shell and the CLI.

## Components

-   Widgets: [`Quickshell`](https://quickshell.outfoxxed.me)
-   Window manager: [`Hyprland`](https://hyprland.org)
-   Dots: [`caelestia`](https://github.com/caelestia-dots)

## Global Shortcuts

All keybinds are accessible via Hyprland [global shortcuts](https://wiki.hyprland.org/Configuring/Binds/#dbus-global-shortcuts).

### Available Shortcuts

| Shortcut Name | Description |
|---------------|-------------|
| `caelestia:controlCenter` | Open control center |
| `caelestia:launcher` | Toggle launcher |
| `caelestia:dashboard` | Toggle dashboard |
| `caelestia:session` | Toggle session menu |
| `caelestia:sidebar` | Toggle sidebar |
| `caelestia:utilities` | Toggle utilities panel |
| `caelestia:emoji` | Open emoji picker |
| `caelestia:clipboard` | Open clipboard history |
| `caelestia:windowSwitcher` | Open window switcher |
| `caelestia:keybinds` | Open keybinds list |
| `caelestia:wallpaper` | Open wallpaper picker |
| `caelestia:showall` | Toggle all UI elements |
| `caelestia:terminal` | Toggle terminal drawer |

### Hyprland Keybind Examples

To bind these shortcuts in Hyprland, add to your config:

```conf
# Launcher and UI elements
bind = SUPER, SPACE, global, caelestia:launcher
bind = SUPER, RETURN, global, caelestia:launcher
bind = SUPER, S, global, caelestia:controlCenter

# New features in this fork
bind = SUPER, E, global, caelestia:emoji
bind = SUPER, V, global, caelestia:clipboard
bind = SUPER, W, global, caelestia:windowSwitcher
bind = SUPER, K, global, caelestia:keybinds
bind = SUPER, B, global, caelestia:wallpaper
bind = SUPER, T, global, caelestia:terminal

# Other toggles
bind = SUPER, D, global, caelestia:dashboard
bind = SUPER, N, global, caelestia:sidebar
bind = SUPER, M, global, caelestia:utilities
```

## Migration from Official Caelestia

If you're migrating from the official caelestia shell to this fork, you may need to update your `shell.json` to include the new configuration options:

```json
"launcher": {
    "favouriteEmojis": [],
    "favouriteClips": []
},
"shimeji": {
    "enabled": false,
    "path": "root:/assets/shimeji/pusheen/",
    "count": 1,
    "autoHide": true,
    "excludedScreens": [],
    "screenCounts": {}
},
"background": {
    "videoWallpaperPaused": false,
    "videoWallpaperSoundEnabled": false,
    "videoWallpaperPauseOnFullscreen": false,
    "videoWallpaperPauseOnTiled": false,
    "videoWallpaperPauseOnAllDisplays": false,
    "videoWallpaperMuteOnMedia": false,
    "desktopLyrics": {
        "enabled": false,
        "autoHide": true,
        "scale": 1.0,
        "position": "bottom-center",
        "alignment": 1,
        "invertColors": false,
        "background": {
            "enabled": false,
            "opacity": 0.7,
            "blur": true
        },
        "shadow": {
            "enabled": true,
            "opacity": 0.7,
            "blur": 0.4
        }
    }
},
"utilities": {
    "quickToggles": [
        { "id": "wallpaper", "enabled": true },
        { "id": "badapple", "enabled": true },
        { "id": "pauseWallpaper", "enabled": true }
    ]
}
```

## Usage

The shell can be started via the `caelestia shell -d` command or `qs -c caelestia`.
If the entire caelestia dots are installed, the shell will be autostarted on login
via an `exec-once` in the hyprland config.

### Shortcuts/IPC

All keybinds are accessible via Hyprland [global shortcuts](https://wiki.hyprland.org/Configuring/Binds/#dbus-global-shortcuts).
If using the entire caelestia dots, the keybinds are already configured for you.
Otherwise, [this file](https://github.com/caelestia-dots/caelestia/blob/main/hypr/hyprland/keybinds.conf#L1-L39)
contains an example on how to use global shortcuts.

All IPC commands can be accessed via `caelestia shell ...`. For example

```sh
caelestia shell mpris getActive trackTitle
```

The list of IPC commands can be shown via `caelestia shell -s`:

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

### PFP/Wallpapers

The profile picture for the dashboard is read from the file `~/.face`, so to set
it you can copy your image to there or set it via the dashboard.

The wallpapers for the wallpaper switcher are read from `~/Pictures/Wallpapers`
by default. To change it, change the wallpapers path in `~/.config/caelestia/shell.json`.

To set the wallpaper, you can use the command `caelestia wallpaper`. Use `caelestia wallpaper -h` for more info about
the command.

## Updating

If installed via the AUR package, simply update your system (e.g. using `yay`).

If you installed via `pkgit`, you can update using `pkgit -u`.
> [!NOTE]
> If `pkgit -u` fails to update the shell for any reason, run `pkgit -fi caelestia-shell`. If that command throws an error, simply run it again.

If installed manually, pull the latest changes and re-run the installation:

```sh
cd $XDG_CONFIG_HOME/quickshell/caelestia
git pull
pkgit -i .
```

## Uninstalling

To cleanly uninstall the shell and its components, simply run `pkgit`'s uninstall command:

```sh
pkgit -r caelestia-shell
```

## Configuring

All configuration options should be put in `~/.config/caelestia/shell.json`. This file is _not_ created by
default, you must create it manually. Options that you omit from the config file will use their default
values.

### Per-monitor configuration

You can configure options per-monitor in `~/.config/caelestia/monitors/<screen-name>/shell.json`. Options
set in this file will **override** the respective options in the global config. Otherwise, the options will
use their values from the global config.

For example, to disable the bar on DP-1:

**`~/.config/caelestia/monitors/DP-1/shell.json`**

```json
{
    "bar": {
        "persistent": false
    }
}
```

> [!NOTE]
> Not all options are respect per-monitor overrides. Most notably, the following options will only read
> from the global config, and ignore the respective option in per-monitor config files.
>
> <details><summary>Ignored options</summary>
>
> - `appearance` (`anim`, `transparency`)
> - `general` (`logo`, `apps`, `idle`, `battery`)
> - `bar.workspaces` (`perMonitorWorkspaces`, `specialWorkspaceIcons`, `windowIcons`, `wsIcons`)
> - `bar.tray` (`iconSubs`, `hiddenIcons`)
> - `dashboard` (`mediaUpdateInterval`, `resourceUpdateInterval`)
> - `launcher` (`specialPrefix`, `actionPrefix`, `enableDangerousActions`, `vimKeybinds`,
>   `favouriteApps`, `hiddenApps`, `actions`)
> - `launcher.useFuzzy` (`apps`, `actions`, `schemes`, `variants`, `wallpapers`)
> - `notifs` (`expire`, `fullscreen`, `defaultExpireTimeout`, `fullscreenExpireTimeout`, `actionOnClick`)
> - `lock` (`enableFprint`, `maxFprintTries`)
> - `nexus` (`networkRescanInterval`)
> - `utilities.toasts` (all except `fullscreen`)
> - `utilities.vpn` (`enabled`, `provider`)
> - `services` (`weatherLocation`, `useFahrenheit`, `useFahrenheitPerformance`, `useTwelveHourClock`,
>   `gpuType`, `visualiserBars`, `audioIncrement`, `brightnessIncrement`, `maxVolume`, `smartScheme`,
>   `defaultPlayer`, `playerAliases`, `lyricsBackend`)
> - `paths` (`wallpaperDir`, `lyricsDir`, `screenshotDir`)
>
> </details>

### Example configuration

> [!NOTE]
> The example configuration includes ALL configuration options in `shell.json`. You are
> **not** recommended to copy and paste this entire configuration into `shell.json`.
> This is meant to serve as a reference of all the available options, and you should
> only add the ones you want to change to `shell.json`.

<details><summary>Example</summary>

```json
{
    "ai": {
        "activeOllamaModel": "llama3",
        "activeProvider": "ollama",
        "defaultOllamaModel": "llama3",
        "defaultProvider": "ollama",
        "enableCelestialMode": false,
        "enableOllama": true,
        "ollamaHistoryJson": "[]",
        "ollamaModel": "llama3",
        "ollamaUrl": "http://localhost:11434",
        "saveChatHistory": true,
        "snapToDefaultOllama": true
    },
    "appearance": {
        "anim": {
            "durations": {
                "scale": 1
            }
        },
        "deformScale": 1,
        "font": {
            "body": {
                "family": "GoogleSansFlex",
                "large": {
                    "family": "",
                    "italic": false,
                    "size": 16,
                    "vaxes": {
                        "ROND": 25
                    },
                    "weight": 400
                },
                "medium": {
                    "family": "",
                    "italic": false,
                    "size": 14,
                    "vaxes": {
                        "ROND": 25
                    },
                    "weight": 400
                },
                "small": {
                    "family": "",
                    "italic": false,
                    "size": 12,
                    "vaxes": {
                        "ROND": 25
                    },
                    "weight": 400
                }
            },
            "clock": "Rubik",
            "headline": {
                "family": "GoogleSansFlex",
                "large": {
                    "family": "",
                    "italic": false,
                    "size": 32,
                    "vaxes": {
                        "ROND": 25
                    },
                    "weight": 500
                },
                "medium": {
                    "family": "",
                    "italic": false,
                    "size": 28,
                    "vaxes": {
                        "ROND": 25
                    },
                    "weight": 500
                },
                "small": {
                    "family": "",
                    "italic": false,
                    "size": 24,
                    "vaxes": {
                        "ROND": 25
                    },
                    "weight": 500
                }
            },
            "icon": {
                "extraLarge": {
                    "family": "",
                    "italic": false,
                    "size": 36,
                    "vaxes": {},
                    "weight": 400
                },
                "family": "Material Symbols Rounded",
                "large": {
                    "family": "",
                    "italic": false,
                    "size": 24,
                    "vaxes": {},
                    "weight": 400
                },
                "medium": {
                    "family": "",
                    "italic": false,
                    "size": 18,
                    "vaxes": {},
                    "weight": 400
                },
                "small": {
                    "family": "",
                    "italic": false,
                    "size": 15,
                    "vaxes": {},
                    "weight": 400
                }
            },
            "label": {
                "family": "GoogleSansFlex",
                "large": {
                    "family": "",
                    "italic": false,
                    "size": 14,
                    "vaxes": {
                        "ROND": 25
                    },
                    "weight": 500
                },
                "medium": {
                    "family": "",
                    "italic": false,
                    "size": 12,
                    "vaxes": {
                        "ROND": 25
                    },
                    "weight": 500
                },
                "small": {
                    "family": "",
                    "italic": false,
                    "size": 11,
                    "vaxes": {
                        "ROND": 25
                    },
                    "weight": 400
                }
            },
            "mono": {
                "family": "CaskaydiaCove NF",
                "large": {
                    "family": "",
                    "italic": false,
                    "size": 16,
                    "vaxes": {},
                    "weight": 400
                },
                "medium": {
                    "family": "",
                    "italic": false,
                    "size": 14,
                    "vaxes": {},
                    "weight": 400
                },
                "small": {
                    "family": "",
                    "italic": false,
                    "size": 12,
                    "vaxes": {},
                    "weight": 400
                }
            },
            "scale": 1,
            "title": {
                "family": "GoogleSansFlex",
                "large": {
                    "family": "",
                    "italic": false,
                    "size": 22,
                    "vaxes": {
                        "ROND": 25
                    },
                    "weight": 500
                },
                "medium": {
                    "family": "",
                    "italic": false,
                    "size": 16,
                    "vaxes": {
                        "ROND": 25
                    },
                    "weight": 500
                },
                "small": {
                    "family": "",
                    "italic": false,
                    "size": 14,
                    "vaxes": {
                        "ROND": 25
                    },
                    "weight": 500
                }
            },
            "workspaces": "Rubik"
        },
        "padding": {
            "scale": 1
        },
        "pitchBlack": false,
        "rounding": {
            "scale": 1
        },
        "spacing": {
            "scale": 1
        },
        "transparency": {
            "base": 0.85,
            "enabled": false,
            "layers": 0.4
        }
    },
    "audio": {
        "sounds": {
            "cameraClick": true,
            "chargingStarted": true,
            "disabledNotifApps": [],
            "effectTick": true,
            "enabled": true,
            "lock": true,
            "lowBattery": true,
            "notificationSound": "Iapetus.wav",
            "notificationVolume": 1,
            "screenRecord": true,
            "sfxVolume": 1,
            "unlock": true
        }
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
            "explorer": ["thunar"],
            "screenshot": ["swappy"]
        },
        "idle": {
            "lockBeforeSleep": true,
            "inhibitWhenAudio": true,
            "inhibitWhenCharging": false,
            "timeouts": [
                {
                    "timeout": 180,
                    "idleAction": "lock",
                    "inhibitWhenAudio": false,
                    "inhibitWhenCharging": false,
                    "respectInhibitors": true
                },
                {
                    "timeout": 300,
                    "idleAction": "dpms off",
                    "returnAction": "dpms on"
                },
                {
                    "timeout": 600,
                    "idleAction": ["suspendThenHibernate"]
                }
            ]
        },
        "battery": {
            "warnLevels": [
                {
                    "level": 20,
                    "title": "Low battery",
                    "message": "You might want to plug in a charger",
                    "icon": "battery_android_frame_2"
                },
                {
                    "level": 10,
                    "title": "Did you see the previous message?",
                    "message": "You should probably plug in a charger <b>now</b>",
                    "icon": "battery_android_frame_1"
                },
                {
                    "level": 5,
                    "title": "Critical battery level",
                    "message": "PLUG THE CHARGER RIGHT NOW!!",
                    "icon": "battery_android_alert",
                    "critical": true
                }
            ],
            "criticalLevel": 3
        }
    },
    "background": {
        "desktopClock": {
            "background": {
                "blur": true,
                "enabled": false,
                "opacity": 0.7
            },
            "enabled": false,
            "invertColors": false,
            "position": "bottom-right",
            "scale": 1,
            "shadow": {
                "blur": 0.4,
                "enabled": true,
                "opacity": 0.7
            }
        },
        "desktopLyrics": {
            "alignment": 1,
            "autoHide": true,
            "background": {
                "blur": true,
                "enabled": false,
                "opacity": 0.7
            },
            "enabled": false,
            "invertColors": false,
            "position": "bottom-center",
            "scale": 1,
            "shadow": {
                "blur": 0.4,
                "enabled": true,
                "opacity": 0.7
            }
        },
        "enabled": true,
        "videoWallpaperMuteOnMedia": false,
        "videoWallpaperPauseOnAllDisplays": false,
        "videoWallpaperPauseOnFullscreen": false,
        "videoWallpaperPauseOnTiled": false,
        "videoWallpaperPaused": false,
        "videoWallpaperSoundEnabled": false,
        "visualiser": {
            "autoHide": true,
            "blur": false,
            "enabled": false,
            "rounding": 1,
            "spacing": 1
        },
        "wallpaperEnabled": true
    },
    "bar": {
        "activeWindow": {
            "compact": false,
            "inverted": false,
            "showOnHover": true
        },
        "clock": {
            "background": false,
            "showDate": false,
            "showIcon": true
        },
        "dock": {
            "hoverHideDelay": 300,
            "hoverIconScale": 1.0,
            "monitorCenter": true,
            "recolourIcons": false
        },
        "dragThreshold": 20,
        "entries": [
            {
                "enabled": true,
                "id": "logo"
            },
            {
                "enabled": true,
                "id": "workspaces"
            },
            {
                "enabled": true,
                "id": "github"
            },
            {
                "enabled": true,
                "id": "spacer"
            },
            {
                "enabled": true,
                "id": "activeWindow"
            },
            {
                "enabled": true,
                "id": "spacer"
            },
            {
                "enabled": true,
                "id": "tray"
            },
            {
                "enabled": true,
                "id": "clock"
            },
            {
                "enabled": true,
                "id": "statusIcons"
            },
            {
                "enabled": true,
                "id": "power"
            }
        ],
        "excludedScreens": [],
        "github": {
            "background": false,
            "token": ""
        },
        "persistent": true,
        "popouts": {
            "activeWindow": true,
            "statusIcons": true,
            "tray": true
        },
        "position": "left",
        "scrollActions": {
            "brightness": true,
            "volume": true,
            "workspaces": true
        },
        "showOnHover": true,
        "status": {
            "showAudio": false,
            "showBattery": true,
            "showBluetooth": true,
            "showKbLayout": false,
            "showLockStatus": true,
            "showMicrophone": false,
            "showNetwork": true,
            "showNotifications": true,
            "showPeripheralBattery": true,
            "peripheralBatteryExcluded": [],
            "showWifi": true
        },
        "tray": {
            "background": false,
            "compact": false,
            "hiddenIcons": [],
            "iconSubs": [],
            "recolour": false
        },
        "workspaces": {
            "activeIndicator": true,
            "activeLabel": " 󰮯",
            "activeTrail": false,
            "capitalisation": "preserve",
            "label": " ",
            "maxWindowIcons": 5,
            "occupiedBg": false,
            "occupiedLabel": " 󰮯",
            "perMonitorWorkspaces": true,
            "showWindows": true,
            "showWindowsOnSpecialWorkspaces": true,
            "shown": 5,
            "specialWorkspaceIcons": [],
            "useIcon": true,
            "windowIcons": [
                {
                    "icon": "sports_esports",
                    "regex": "steam(_app_(default|[0-9]+))?"
                }
            ],
            "wsIcons": []
        }
    },
    "border": {
        "rounding": 25,
        "smoothing": 20,
        "thickness": 10
    },
    "dashboard": {
        "colorizeMediaGif": true,
        "dragThreshold": 50,
        "enabled": true,
        "mediaUpdateInterval": 500,
        "performance": {
            "showBattery": true,
            "showCpu": true,
            "showGpu": true,
            "showMemory": true,
            "showNetwork": true,
            "showStorage": true
        },
        "profilePicShape": 9,
        "resourceUpdateInterval": 1000,
        "showDashboard": true,
        "showHyprlandSplash": false,
        "showMedia": true,
        "showOnHover": true,
        "showPerformance": true,
        "showTerminal": true,
        "showWeather": true
    },
    "enabled": true,
    "general": {
        "apps": {
            "audio": [
                "pavucontrol"
            ],
            "explorer": [
                "thunar"
            ],
            "playback": [
                "mpv"
            ],
            "screenshot": [
                "swappy"
            ],
            "terminal": [
                "foot"
            ]
        },
        "battery": {
            "criticalLevel": 3,
            "warnLevels": [
                {
                    "icon": "battery_android_frame_2",
                    "level": 20,
                    "message": "You might want to plug in a charger",
                    "title": "Low battery"
                },
                {
                    "icon": "battery_android_frame_1",
                    "level": 10,
                    "message": "You should probably plug in a charger <b>now</b>",
                    "title": "Did you see the previous message?"
                },
                {
                    "critical": true,
                    "icon": "battery_android_alert",
                    "level": 5,
                    "message": "PLUG THE CHARGER RIGHT NOW!!",
                    "title": "Critical battery level"
                }
            ]
        },
        "idle": {
            "inhibitWhenAudio": true,
            "lockBeforeSleep": true,
            "timeouts": [
                {
                    "idleAction": "lock",
                    "timeout": 180
                },
                {
                    "idleAction": "dpms off",
                    "returnAction": "dpms on",
                    "timeout": 300
                },
                {
                    "idleAction": [
                        "loginctl",
                        "suspend"
                    ],
                    "timeout": 600
                }
            ]
        },
        "logo": "",
        "mediaGifSpeedAdjustment": 300,
        "sessionGifSpeed": 0.7,
        "showOverFullscreen": false
    },
    "launcher": {
        "actionPrefix": ">",
        "actions": [
            {
                "command": [
                    "autocomplete",
                    "calc"
                ],
                "description": "Do simple math equations (powered by Qalc)",
                "icon": "calculate",
                "name": "Calculator"
            },
            {
                "command": [
                    "autocomplete",
                    "scheme"
                ],
                "description": "Change the current colour scheme",
                "icon": "palette",
                "name": "Scheme"
            },
            {
                "command": [
                    "autocomplete",
                    "wallpaper"
                ],
                "description": "Change the current wallpaper",
                "icon": "image",
                "name": "Wallpaper"
            },
            {
                "command": [
                    "autocomplete",
                    "variant"
                ],
                "description": "Change the current scheme variant",
                "icon": "colors",
                "name": "Variant"
            },
            {
                "command": [
                    "caelestia",
                    "wallpaper",
                    "-r"
                ],
                "description": "Switch to a random wallpaper",
                "icon": "casino",
                "name": "Random"
            },
            {
                "command": [
                    "setMode",
                    "light"
                ],
                "description": "Change the scheme to light mode",
                "icon": "light_mode",
                "name": "Light"
            },
            {
                "command": [
                    "setMode",
                    "dark"
                ],
                "description": "Change the scheme to dark mode",
                "icon": "dark_mode",
                "name": "Dark"
            },
            {
                "command": [
                    "loginctl",
                    "poweroff"
                ],
                "dangerous": true,
                "description": "Shutdown the system",
                "icon": "power_settings_new",
                "name": "Shutdown"
            },
            {
                "command": [
                    "loginctl",
                    "reboot"
                ],
                "dangerous": true,
                "description": "Reboot the system",
                "icon": "cached",
                "name": "Reboot"
            },
            {
                "command": [
                    "hyprctl",
                    "dispatch",
                    "exit"
                ],
                "dangerous": true,
                "description": "Log out of the current session",
                "icon": "exit_to_app",
                "name": "Logout"
            },
            {
                "command": [
                    "loginctl",
                    "lock-session"
                ],
                "description": "Lock the current session",
                "icon": "lock",
                "name": "Lock"
            },
            {
                "command": [
                    "loginctl",
                    "suspend"
                ],
                "description": "Suspend then hibernate",
                "icon": "bedtime",
                "name": "Sleep"
            },
            {
                "command": [
                    "caelestia",
                    "shell",
                    "nexus",
                    "open"
                ],
                "description": "Configure the shell",
                "icon": "settings",
                "name": "Settings"
            },
            {
                "command": [
                    "autocomplete",
                    "emoji"
                ],
                "description": "Pick an emoji to copy",
                "icon": "emoji_emotions",
                "name": "Emoji"
            },
            {
                "command": [
                    "autocomplete",
                    "clipboard"
                ],
                "description": "View clipboard history",
                "icon": "content_paste",
                "name": "Clipboard"
            },
            {
                "command": [
                    "autocomplete",
                    "windows"
                ],
                "description": "Switch to another window",
                "enabled": true,
                "icon": "apps",
                "name": "Windows"
            },
            {
                "command": [
                    "autocomplete",
                    "keybinds"
                ],
                "description": "View all keybinds",
                "icon": "keyboard",
                "name": "Keybinds"
            }
        ],
        "dragThreshold": 50,
        "enableDangerousActions": false,
        "enabled": true,
        "favouriteApps": [],
        "favouriteClips": [],
        "favouriteEmojis": [],
        "hiddenApps": [],
        "maxShown": 7,
        "maxWallpapers": 9,
        "showOnHover": false,
        "specialPrefix": "@",
        "useFuzzy": {
            "actions": false,
            "apps": false,
            "clipboard": false,
            "emoji": false,
            "schemes": false,
            "variants": false,
            "wallpapers": false
        },
        "vimKeybinds": false
    },
    "lock": {
        "enabled": true,
        "enableFprint": true,
        "hideNotifs": false,
        "maxFprintTries": 3,
        "profilePicShape": 12,
        "recolourLogo": true,
        "enableHowdy": true,
        "maxHowdyTries": 3,
        "triggerHowdyOnWake": true
    },
    "nexus": {
        "networkRescanInterval": 15000,
        "wallpapersPerRow": 4
    },
    "notifs": {
        "actionOnClick": false,
        "clearThreshold": 0.3,
        "defaultExpireTimeout": 5000,
        "expandOnHover": false,
        "expandThreshold": 20,
        "expire": true,
        "fullscreen": "on",
        "fullscreenExpireTimeout": 2000,
        "groupPreviewNum": 3,
        "openExpanded": false
    },
    "osd": {
        "enableBrightness": true,
        "enableMicrophone": true,
        "enabled": true,
        "hideDelay": 2000
    },
    "paths": {
        "cacheDir": "/home/dim/.cache/caelestia",
        "lockNoNotifsPic": "root:/assets/dino.png",
        "lyricsDir": "/home/dim/Music/Lyrics/",
        "mediaGif": "root:/assets/bongocat.gif",
        "noNotifsPic": "root:/assets/dino.png",
        "screenshotDir": "/home/dim/Pictures/Screenshots/",
        "sessionGif": "root:/assets/kurukuru.gif",
        "wallpaperDir": "/home/dim/Pictures/Wallpapers"
    },
    "services": {
        "audioIncrement": 0.1,
        "brightnessIncrement": 0.1,
        "defaultPlayer": "Spotify",
        "gpuType": "",
        "lyricsBackend": "Auto",
        "maxVolume": 1,
        "playerAliases": [
            {
                "from": "com.github.th_ch.youtube_music",
                "to": "YT Music"
            }
        ],
        "smartScheme": true,
        "useFahrenheit": true,
        "useFahrenheitPerformance": false,
        "useTwelveHourClock": true,
        "visualiserBars": 60,
        "weatherLocation": ""
    },
    "session": {
        "commands": {
            "hibernate": [
                "loginctl",
                "hibernate"
            ],
            "logout": [
                "hyprctl",
                "dispatch",
                "exit"
            ],
            "reboot": [
                "loginctl",
                "reboot"
            ],
            "shutdown": [
                "loginctl",
                "poweroff"
            ]
        },
        "dragThreshold": 30,
        "enabled": true,
        "icons": {
            "hibernate": "downloading",
            "logout": "logout",
            "reboot": "cached",
            "shutdown": "power_settings_new"
        },
        "vimKeybinds": false
    },
    "shimeji": {
        "autoHide": true,
        "count": 1,
        "enabled": true,
        "excludedScreens": [],
        "path": "root:/assets/shimeji/pusheen/",
        "screenCounts": {}
    },
    "sidebar": {
        "dragThreshold": 80,
        "enableArchNews": false,
        "enableNixosNews": false,
        "enabled": true,
        "minHoverThreshold": 200,
        "showOnHover": false
    },
    "utilities": {
        "enabled": true,
        "maxToasts": 4,
        "gameModeAutoKeepAwake": true,
        "quickToggles": [
            {
                "enabled": true,
                "id": "wifi"
            },
            {
                "enabled": true,
                "id": "bluetooth"
            },
            {
                "enabled": true,
                "id": "mic"
            },
            {
                "enabled": true,
                "id": "settings"
            },
            {
                "enabled": true,
                "id": "gameMode"
            },
            {
                "enabled": true,
                "id": "dnd"
            },
            {
                "enabled": false,
                "id": "vpn"
            },
            {
                "enabled": true,
                "id": "wallpaper"
            },
            {
                "enabled": true,
                "id": "badapple"
            }
        ],
        "toasts": {
            "audioInputChanged": true,
            "audioOutputChanged": true,
            "capsLockChanged": true,
            "chargingChanged": true,
            "configLoaded": true,
            "dndChanged": true,
            "fullscreen": "off",
            "gameModeChanged": true,
            "kbLayoutChanged": true,
            "kbLimit": true,
            "nowPlaying": false,
            "numLockChanged": true,
            "transparency": false,
            "transparencyBase": 0.85,
            "vpnChanged": true
        },
        "vpn": {
            "enabled": false,
            "provider": []
        }
    },
    "winfo": {}
}
```

</details>

### Advanced configuration

> [!WARNING]
> Do NOT change any of these options if you do not know what you are doing. These options control the
> tokens used internally within the shell, and can cause visual issues if changed. The existence of
> the options are also not guaranteed across versions, and may change or be removed without notice.

A separate `~/.config/caelestia/shell-tokens.json` file allows editing the internal tokens without
touching the source code of the shell. These tokens affect, for example, individual rounding,
spacing, padding, font size, animation duration and easing curves tokens, and the sizes of certain
components. The appearance scale values in `shell.json` are multiplied against these base
token values to produce the final computed values.

Per-monitor token overrides are also available at
`~/.config/caelestia/monitors/<screen-name>/shell-tokens.json`.

### Home Manager Module

For NixOS users, a home manager module is also available.

<details><summary><code>home.nix</code></summary>

```nix
programs.caelestia = {
  enable = true;
  systemd = {
    enable = false; # if you prefer starting from your compositor
    target = "graphical-session.target";
    environment = [];
  };
  settings = {
    bar.status = {
      showBattery = false;
    };
    paths.wallpaperDir = "~/Images";
  };
  cli = {
    enable = true; # Also add caelestia-cli to path
    settings = {
      theme.enableGtk = false;
    };
  };
};
```

The module automatically adds Caelestia shell to the path with **full functionality**. The CLI is not required, however you have the option to enable and configure it.

</details>

## FAQ

### My screen is flickering, help pls!

Try disabling VRR in the hyprland config. You can do this by adding the following to `~/.config/caelestia/hypr-user.conf`:

```conf
misc {
    vrr = 0
}
```

### How do I enable blur for the Polkit dialog?

Add the following layer rule to your `~/.config/caelestia/hypr-user.conf`:

```conf
layerrule = no_anim true, match:namespace caelestia-polkit, blur true, ignore_alpha 0.1
```

### I want to make my own changes to the hyprland config!

You can add your custom hyprland configs to `~/.config/caelestia/hypr-user.conf`.

### I want to make my own changes to other stuff!

See the [manual installation](#arch-linux--manual-this-fork) section above.

### I want to disable XXX feature!

Please read the [configuring](#configuring) section in the readme.
If there is no corresponding option, make a feature request via GitHub issues.

### How do I make my colour scheme change with my wallpaper?

Set a wallpaper via the launcher or `caelestia wallpaper` and set the scheme to the dynamic scheme via the launcher
or `caelestia scheme set`. e.g.

```sh
caelestia wallpaper -f <path/to/file>
caelestia scheme set -n dynamic
```

### My wallpapers aren't showing up in the launcher!

The launcher pulls wallpapers from `~/Pictures/Wallpapers` by default. You can change this in the config. Additionally,
the launcher only shows an odd number of wallpapers at one time. If you only have 2 wallpapers, consider getting more
(or just putting one).

## Indexing settings

The settings panel (nexus) has a full-text search that lets users jump straight to any setting by name, description, or
the section/page it lives under. The index is generated from the page QML at build time and baked into the plugin binary,
so it always matches the UI and ships with the compiled module rather than as a user-editable file.

<details><summary>Developer guide: how it works, and how to add or remove settings</summary>

### How it works at a glance

```
  page QML files                build-settings-index.py            plugin binary
  (ToggleRow, NavRow, …)  ──►   (parses QML, builds index)   ──►   (JSON embedded
   + settingAnchor                                                   as a qrc resource)
                                                                         │
                                                                         ▼
                                                        SettingsSearcher.qml reads it
                                                        via CUtils.settingsIndex()
                                                                         │
                                                                         ▼
                                                        query() → grouped results →
                                                        NexusState.jumpToSetting()
```

The index is **generated, not hand-written**. The build script reads the page QML, finds every indexable row, and emits a
JSON file. CMake bakes that JSON into the plugin binary so it ships with the compiled module rather than as a
user-editable file. At runtime the search service reads it back out and serves queries from an inverted index.

### Adding a setting to the search

A row is indexed when two things are true:

1. It is one of the indexable row types listed in `ROW_RE` in `scripts/build-settings-index.py` — currently `ToggleRow`,
   `SliderRow`, `SelectRow`, `StepperRow`, `NavRow`, `InfoRow`, `PopupRow` (and its `DefaultRow` alias). These all derive
   from `ConnectedRect`, which is what makes the deep-link scroll/flash work.
2. It has a `settingAnchor` property set to a unique kebab-case id.

So to make a setting searchable, add a `settingAnchor` to its row:

```qml
ToggleRow {
    icon: "notifications"
    label: qsTr("Show in fullscreen")
    status: qsTr("Keep showing notifications over fullscreen apps")
    settingAnchor: "notif-show-in-fullscreen"   // ← add this
    // …
}
```

Then regenerate the index (see "Regenerating the index" below) and commit. That's it — everything else is automatic:

- **Page, sub-pages, breadcrumbs** are discovered from the page tree (`PageRegistry.qml` for icons/labels,
  `PageCompRegistry.qml` for the hierarchy), so you don't list them anywhere.
- **The title** comes from the row's `label`.
- **The description** comes from the row's `subtext` or `status`.
- **The section** comes from the nearest `SectionHeader` above the row.
- **Search tokens** (the inverted index) are built from all of the above.

#### Choosing a good anchor

The anchor is a stable id used for deep-linking, not shown to the user. Keep it kebab-case and prefix it with the page so
ids stay unique and readable, e.g. `notif-default-timeout`, `apps-all-apps`, `ethernet-ip-address`. Once an anchor ships,
avoid renaming it gratuitously — it's the durable handle for that setting.

#### Indexing a new or different row type

The generator only looks at the row types listed in `ROW_RE`. If a setting uses a component that isn't in that list,
**it won't be indexed even if you add a `settingAnchor`** — the generator simply never sees it. This is an easy thing to
miss: the setting works fine in the UI but never shows up in search.

This is exactly what happened with the "Default applications" rows (Terminal, Audio, Media playback, File manager) on the
Apps page. They use a `PopupRow` (via its `DefaultRow` alias) rather than a `ToggleRow`/`NavRow`, so they were invisible to
search until `PopupRow`/`DefaultRow` were added to `ROW_RE`.

To make a new row type indexable:

1. **Confirm it derives from `ConnectedRect`.** This is required — the deep-link scroll-and-flash relies on
   `ConnectedRect`'s `settingAnchor` and `flashHighlight()`. A component that isn't a `ConnectedRect` (e.g. a bare
   `M3TextField`) can't be deep-linked and shouldn't be added.
2. **Add the component name to `ROW_RE`** in `scripts/build-settings-index.py`. The generator matches on the literal name
   as written in the QML, so if a page uses a local alias (like `DefaultRow` for `PopupRow`), add the alias too — or
   better, add the underlying type and prefer using it directly.
3. **Make sure its title/description come from the expected properties.** The generator reads the title from `label` or
   `text`, and the description from `subtext` or `status` (see `LABEL_RE` and `SUBTEXT_RE`). If your component exposes
   those under different names, either alias them or extend the regexes.
4. Add `settingAnchor`s, regenerate, and commit.

If you find yourself adding lots of one-off aliases, that's a sign the underlying row type (e.g. `PopupRow`) should be in
`ROW_RE` directly so future pages using it are indexed automatically.

### Removing a setting from the search

There are four ways, depending on how broadly you want to exclude:

1. **One setting** — delete its `settingAnchor`. The row stays in the UI but drops out of search. This is the usual case.
2. **A title everywhere** — add the title to `SKIP_LABELS` in `scripts/build-settings-index.py`. Useful for generic labels
   like `Muted` or `None` that would otherwise produce noise.
3. **A whole page** — remove the `settingAnchor` from every row on that page.
4. **Conditionally / at runtime** — filter in `NavLocations.qml`. This is how ethernet settings are hidden when no ethernet
   is available: the results list drops entries whose anchor starts with `ethernet-` unless a wired connection exists. Use
   this when "should it be searchable" depends on runtime state, not on the source.

After options 1–3, regenerate the index and commit. Option 4 is pure QML and needs no regeneration.

### Regenerating the index

The build runs the generator automatically, so a normal `cmake --build` produces a fresh index. But `qs -c caelestia`
(used for quick iteration) does **not** run CMake, so after any change that affects the index you must regenerate it
manually before testing:

```sh
python3 scripts/build-settings-index.py modules/nexus <output.json>
```

During development the simplest flow is to point it at a temporary file and rebuild the plugin once, or just run a full
`cmake --build`. The committed source of truth is the generator and the page QML — there is no checked-in JSON to keep in
sync (the index lives inside the plugin binary, see below).

> **Note:** changes that affect the index — adding/removing a `settingAnchor`, editing a `label`/`subtext`/`status`/
> `SectionHeader`, or restructuring pages — only show up after the index is regenerated. Pure styling or behaviour changes
> to the search UI (`NavLocations.qml`, `SettingsSearcher.qml`) take effect with a plain `qs -c caelestia`.

### Where the index lives

The generated JSON is **embedded into the plugin binary as a Qt resource**, not installed as a config file. This keeps it
out of the user-editable config tree (it can't be accidentally edited or deleted), and means it ships wherever the module
is installed — manual build, AUR, Nix, all the same.

The flow in CMake:

1. `CMakeLists.txt` runs `build-settings-index.py` at configure time, before the plugin subdirectory, writing to
   `${CMAKE_BINARY_DIR}/settings-index.json` (the `SETTINGS_INDEX_JSON` variable).
2. `plugin/src/Caelestia/CMakeLists.txt` adds that file to the `caelestia-core` module as a `RESOURCES` entry, with
   `QT_RESOURCE_ALIAS` mapping it to the stable path `settings-index.json` regardless of the build-dir layout.
3. At runtime it is available at the qrc path `:/qt/qml/Caelestia/settings-index.json` (Qt's `qt_add_qml_module` prefixes
   resources with `:/qt/qml/<URI>/`).

`CUtils::settingsIndex()` (in `plugin/src/Caelestia/cutils.{hpp,cpp}`) reads that resource and returns it as a string to
QML.

> Because `rcc` compresses embedded resources, you won't see the JSON text with `strings` on the `.so` — that's expected,
> the data is there but zlib-compressed. To verify, log `CUtils.settingsIndex().length` from QML instead.

### The generated JSON

Schema (version 2):

```jsonc
{
  "version": 2,
  "entries": [
    {
      "pageIdx":     0,                    // index of the owning top-level page
      "subPath":     [2, 9],               // sub-page navigation path (empty = main page)
      "crumbIcons":  ["palette", "…"],     // breadcrumb icons, page → setting
      "crumbLabels": ["Wallpaper", "…"],   // breadcrumb labels
      "title":       "Display wallpaper",  // the setting label
      "section":     "Wallpaper",          // nearest SectionHeader, if any
      "subtext":     "…",                  // description (subtext/status)
      "anchor":      "wallpaper-display"   // settingAnchor, used for deep-linking
    }
    // …
  ],
  "inverted": { "token": [entryIdx, …] }, // inverted index: token → matching entries
  "ranking":  { "token": { "entryIdx": weight } } // per-token relevance weights
}
```

`title` weighs more than keyword tokens in ranking, so a query that hits a setting's name ranks above one that only hits
its description.

### Runtime pieces

| File | Role |
| --- | --- |
| `scripts/build-settings-index.py` | Parses page QML, builds the index JSON. |
| `SettingsSearcher.qml` | Singleton search service. Loads the index via `CUtils.settingsIndex()`, exposes `query(search)` over the inverted index, plus `highlight()` for match emphasis. |
| `NavLocations.qml` | Renders grouped result cards, runtime filtering (e.g. ethernet), click-to-navigate. |
| `NexusState.qml` | `jumpToSetting(pageIdx, subPath, anchor)` drives navigation + deferred scroll target. |
| `common/PageBase.qml` | `scrollToAnchor()` scrolls to and flashes the target row once the page is ready (handles async-loaded content). |
| `common/ConnectedRect.qml` | Base of the indexable rows; provides `settingAnchor` and the flash highlight. |
| `plugin/src/Caelestia/cutils.{hpp,cpp}` | `settingsIndex()` returns the embedded JSON to QML. |

#### Search internals

`query(search)` tokenizes the input, looks up each token in the inverted index (exact match first, then prefix — so `wall`
matches `wallpaper`), keeps only entries that match **all** tokens (AND semantics), sorts by summed relevance weight (ties
broken by entry id for stability), and caps the result count. Each result is exposed as a `SettingEntry` QObject so the UI
can bind to its fields.

`highlight(text, search, colour)` wraps query-matched prefixes in a `<font color>` tag for display with `Text.StyledText`.
(StyledText supports `<font color>` but not CSS `<span style>`, which is a common gotcha.)

### Gotchas

- **`qs -c` won't regenerate the index.** Always rerun the generator after index-affecting edits, or do a full build.
- **Only `ConnectedRect`-derived rows can take a `settingAnchor`.** Plain `M3TextField`s and other non-`ConnectedRect`
  components can't be deep-linked, so they can't be indexed this way.
- **A `settingAnchor` does nothing if the row type isn't in `ROW_RE`.** The generator only sees the row types it's told
  about, so a new component (or a page-local alias) needs adding to `ROW_RE` first — otherwise the setting works in the UI
  but silently never appears in search. See "Indexing a new or different row type" above.
- **Tokenization splits on non-alphanumerics.** A single query word won't match across a hyphen boundary in a hyphenated
  name (e.g. `wifi` vs `Wi-Fi`): the result still appears via the index, but that exact word may not be highlighted.
- **Anchors are forever-ish.** They're the deep-link handle; renaming one is a breaking change for anything that linked to
  it.

</details>

## Credits

Original project: [caelestia-dots/shell](https://github.com/caelestia-dots/shell), created by
[@soramanew](https://github.com/soramanew).
