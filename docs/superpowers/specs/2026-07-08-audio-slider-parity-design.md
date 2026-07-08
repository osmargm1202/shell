# Audio slider parity design

## Problem

Volume/mic/brightness controls are inconsistent across the three surfaces that expose them:

- **OSD** (`modules/osd/Content.qml`, the popup shown on volume/brightness key presses and hover): already has volume, mic, and brightness sliders wired up correctly (respects `GlobalConfig.services.maxVolume` for both audio sliders), but the mic slider is hidden by default (`Config.osd.enableMicrophone` defaults to `false` in `plugin/src/Caelestia/Config/osdconfig.hpp`).
- **Bar Audio popout** (`modules/bar/popouts/Audio.qml`, opened from the bar's audio tray icon): has a speaker volume slider but no mic volume slider at all (only an input-device *selector*, no source-volume control), and the volume slider ignores `maxVolume` entirely — it's a plain 0–100% `StyledSlider`.
- **Neither slider component** (`FilledSlider.qml` used by OSD, `StyledSlider.qml` used by the bar popout) has any way to mark where 100% sits once the configurable max goes above it (up to 200% via the existing "Max volume" stepper in Nexus settings).

## Goals

1. OSD mic slider visible by default.
2. Bar Audio popout gets a mic volume slider, styled/wired the same way as OSD's.
3. Bar Audio popout's volume slider respects `GlobalConfig.services.maxVolume` like OSD already does.
4. Both slider components show a visual tick mark at the 100% position when `to > 1.0`, so raising the cap doesn't lose the sense of where "normal" volume is.

## Non-goals

- No change to `GlobalConfig.services.maxVolume`'s default (stays 1.0/100%); users opt in via the existing Nexus stepper.
- No sidebar changes — clarified during brainstorming that "the slider popout on the right" the user meant is the OSD, not a separate sidebar panel. Sidebar audio controls are out of scope here.
- No changes to device-selection UI in Audio.qml (output/input radio button lists stay as-is).

## Design

### 1. Enable OSD mic slider by default

One-line change in `plugin/src/Caelestia/Config/osdconfig.hpp`:
`CONFIG_PROPERTY(bool, enableMicrophone, false)` → `true`. Requires a native plugin rebuild (C++ change, not pure QML).

### 2. Mic slider in Audio.qml

Add a second `CustomMouseArea` + `StyledSlider` block mirroring the existing volume block (lines ~113–139 of `modules/bar/popouts/Audio.qml`), using `Audio.sourceVolume` / `Audio.setSourceVolume` / `Audio.incrementSourceVolume` / `Audio.decrementSourceVolume` (same API OSD already uses) instead of the speaker equivalents. Label reads `qsTr("Microphone (%1)").arg(...)` mirroring the volume label's muted/percentage formatting. Placed directly below the existing volume slider block, above the "Open settings" button.

### 3. maxVolume cap on Audio.qml's volume slider

`StyledSlider` in the existing volume block gets a `to: GlobalConfig.services.maxVolume` binding (currently unset, implicitly 0–1). The new mic slider gets the same `to` binding from the start.

### 4. 100% tick mark on both slider components

Add a `markValue: real` property (default `1.0`, meaningless/hidden when `>= to`) to both `FilledSlider.qml` and `StyledSlider.qml`. When `to > markValue`, render a small fixed-width tick (a thin `Rectangle`, using the existing outline/border colour token) positioned at `markValue / to` proportionally along the track, overlaid on top of the filled-track visuals so it stays visible regardless of current fill level. No animation needed — position only changes when `to` changes, which is rare (a settings change, not a live-drag event).

Both OSD's volume/mic sliders and the bar popout's volume/mic sliders bind `markValue` implicitly via the default (`1.0`), so no call-site changes are needed there beyond the `to:` binding already described.

## Testing

- Build + switch on the NixOS host (per this session's established flow: `nixos-rebuild build --override-input` or updated flake.lock, then `nh os switch`).
- Manually verify: OSD mic slider appears on mic-related key press / hover without any config change.
- Open bar Audio popout, confirm mic slider present and moves the actual input volume (verify via `wpctl` or system volume indicator).
- Set `Max volume` to 150% in Nexus Services settings, confirm both OSD and Audio popout volume/mic sliders extend past the old 100% point and show a tick mark where 100% used to be.
- Confirm behavior at default 100% max: no tick mark visible (since `to == markValue`).
