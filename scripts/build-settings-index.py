#!/usr/bin/env python3
"""Build-time settings index extractor for the nexus settings search.

Parses the nexus page QML files, PageRegistry.qml (page icons/labels) and
PageCompRegistry.qml (page ordering and sub-page nesting) to produce a search
index as JSON. Run at build time (see CMakeLists.txt); the shell loads the
result at runtime via SettingsSearcher.qml.

The output contains three parts:
  - entries:  forward index, one record per setting (title, anchor, nav path)
  - inverted: token -> list of entry indices (classic inverted index)
  - ranking:  token -> {entry index: weight} precomputed match weights

Nothing here is hand-maintained per page: page metadata comes from
PageRegistry, the page tree from PageCompRegistry, and the directory layout is
discovered by walking the pages folder.

Usage: build-settings-index.py <nexus-dir> <output-json>
"""
from __future__ import annotations

import json
import re
import sys
from collections import defaultdict
from functools import lru_cache
from pathlib import Path


@lru_cache(maxsize=None)
def read_lines(path: Path) -> tuple[str, ...]:
    """Read a file's lines, cached so each page file is only read once."""
    return tuple(path.read_text().splitlines())

ROW_RE = re.compile(r'^\s*(ToggleRow|SliderRow|SelectRow|StepperRow|NavRow|InfoRow|PopupRow|DefaultRow)\s*\{')
LABEL_RE = re.compile(r'^\s*(?:label|text):\s*qsTr\("([^"]+)"\)')
ANCHOR_RE = re.compile(r'^\s*settingAnchor:\s*"([^"]+)"')
# A ToggleRow whose value is a plain config property can be flipped straight from
# the search results. We capture the property path from `checked:` and require
# `onToggled:` to write the same path back (a symmetric binding), so reading and
# writing go through one path. Toggles bound to functions or multi-line handlers
# are left without a path and just deep-link as usual.
CHECKED_RE = re.compile(r'^\s*checked:\s*(?:GlobalConfig|Config)\.([\w.]+)\s*$')
ONTOGGLED_RE = re.compile(r'^\s*onToggled:\s*(?:GlobalConfig|Config)\.([\w.]+)\s*=\s*checked\s*$')
ICON_RE = re.compile(r'^\s*icon:\s*"([^"]+)"')
SKIP_LABELS = {"Muted", "None"}
# Field weights for ranking: a token matching the title counts more than one
# matching the keywords blob.
FIELD_WEIGHT = {"title": 1.0, "keywords": 0.4}
STOPWORDS = {"the", "a", "an", "of", "notification", "are", "not", "out", "and", "or", "to", "on", "in", "for"}

# The Material Symbols icon shown on each setting's search-result card, keyed
# by its anchor. Hand-picked so every setting reads at a glance and none just
# repeats its page's icon; anything unmapped (e.g. a newly added setting)
# falls back to the deepest breadcrumb icon.
SETTING_ICONS = {
    "style-display-wallpaper": "wallpaper",
    "style-transparency": "opacity",
    "style-dark-theme": "dark_mode",
    "network-wi-fi": "network_wifi",
    "ethernet-status": "link",
    "ethernet-interface": "settings_ethernet",
    "ethernet-speed": "speed",
    "ethernet-ip-address": "tag",
    "ethernet-gateway": "router",
    "ethernet-mac-address": "fingerprint",
    "ethernet-ip-assignment": "tune",
    "bluetooth-bluetooth": "bluetooth_connected",
    "bluetooth-discoverable": "visibility",
    "bluetooth-pairable": "add_link",
    "audio-output": "speaker",
    "audio-input": "mic",
    "panels-dashboard": "space_dashboard",
    "panels-taskbar": "toolbar",
    "panels-launcher": "apps",
    "panels-sidebar": "view_sidebar",
    "dash-enabled": "toggle_on",
    "dash-show-on-hover": "mouse",
    "dash-dashboard": "tab",
    "dash-media": "play_circle",
    "dash-performance": "speed",
    "dash-weather": "partly_cloudy_day",
    "dash-battery": "battery_full",
    "dash-gpu": "developer_board",
    "dash-cpu": "memory",
    "dash-memory": "memory_alt",
    "dash-storage": "hard_drive",
    "dash-network": "network_check",
    "dash-drag-threshold": "drag_pan",
    "taskbar-persistent": "push_pin",
    "taskbar-show-on-hover": "mouse",
    "taskbar-drag-threshold": "drag_pan",
    "taskbar-workspaces": "workspaces",
    "taskbar-active-window": "select_window",
    "taskbar-tray": "widgets",
    "taskbar-status-icons": "instant_mix",
    "taskbar-clock": "schedule",
    "taskbar-workspaces-2": "swipe",
    "taskbar-volume": "volume_up",
    "taskbar-brightness": "brightness_6",
    "bar-ws-shown": "format_list_numbered",
    "bar-ws-active-indicator": "radio_button_checked",
    "bar-ws-active-trail": "gesture",
    "bar-ws-occupied-background": "layers",
    "bar-ws-show-windows": "grid_view",
    "bar-ws-windows-on-special-workspaces": "star",
    "bar-ws-max-window-icons": "filter_9_plus",
    "bar-ws-per-monitor-workspaces": "desktop_windows",
    "bar-aw-compact": "compress",
    "bar-aw-inverted": "invert_colors",
    "bar-aw-show-on-hover": "mouse",
    "bar-aw-popout-on-hover": "open_in_new",
    "bar-tray-background": "format_color_fill",
    "bar-tray-recolour-icons": "format_paint",
    "bar-tray-compact": "compress",
    "bar-tray-popout-on-hover": "open_in_new",
    "bar-si-speakers": "speaker",
    "bar-si-microphone": "mic",
    "bar-si-keyboard-layout": "keyboard",
    "bar-si-network": "lan",
    "bar-si-wi-fi": "network_wifi",
    "bar-si-bluetooth": "bluetooth",
    "bar-si-battery": "battery_full",
    "bar-si-caps-lock": "keyboard_capslock_badge",
    "bar-si-popout-on-hover": "open_in_new",
    "bar-clock-background": "format_color_fill",
    "bar-clock-show-date": "calendar_month",
    "bar-clock-show-icon": "visibility",
    "launcher-enabled": "toggle_on",
    "launcher-show-on-hover": "mouse",
    "launcher-max-items-shown": "format_list_numbered",
    "launcher-max-wallpapers": "photo_library",
    "launcher-drag-threshold": "drag_pan",
    "launcher-vim-keybinds": "keyboard_command_key",
    "launcher-enable-dangerous-actions": "warning",
    "launcher-apps": "grid_view",
    "launcher-actions": "bolt",
    "launcher-schemes": "palette",
    "launcher-variants": "contrast",
    "launcher-wallpapers": "wallpaper",
    "sidebar-enabled": "toggle_on",
    "sidebar-drag-threshold": "drag_pan",
    "apps-default-terminal": "terminal",
    "apps-default-audio": "music_note",
    "apps-default-playback": "play_circle",
    "apps-default-file-manager": "folder",
    "apps-all-apps": "view_apps",
    "services-notifications": "notifications",
    "services-media-refresh": "autorenew",
    "services-system-stats-refresh": "monitoring",
    "services-wi-fi-rescan": "wifi_find",
    "services-lyrics-backend": "lyrics",
    "services-default-player": "queue_music",
    "services-volume-step": "volume_up",
    "services-brightness-step": "brightness_6",
    "services-max-volume": "vertical_align_top",
    "services-visualiser-bars": "graphic_eq",
    "services-smart-colour-scheme": "auto_awesome",
    "services-gpu": "developer_board",
    "notif-show-in-fullscreen": "fullscreen",
    "notif-expire-automatically": "auto_delete",
    "notif-open-expanded": "unfold_more",
    "notif-default-timeout": "timer",
    "notif-group-preview-count": "stacks",
    "notif-show-in-fullscreen-2": "fullscreen",
    "notif-visible-toasts": "filter_9_plus",
    "notif-charging-changes": "battery_charging_full",
    "notif-game-mode-changes": "sports_esports",
    "notif-do-not-disturb-changes": "do_not_disturb_on",
    "notif-audio-output-changes": "speaker",
    "notif-audio-input-changes": "mic",
    "notif-caps-lock-changes": "keyboard_capslock_badge",
    "notif-num-lock-changes": "looks_one",
    "notif-keyboard-layout-changes": "keyboard",
    "notif-vpn-changes": "vpn_key",
    "notif-now-playing": "play_circle",
    "lang-temperature": "thermostat",
    "lang-system-temperatures": "device_thermostat",
    "lang-clock-format": "schedule",
}


def find_pages_dir(nexus: Path) -> Path:
    return nexus / "pages"


def discover_files(nexus: Path) -> dict[str, Path]:
    """component name -> file path, discovered by walking pages/."""
    files: dict[str, Path] = {}
    for p in find_pages_dir(nexus).rglob("*.qml"):
        files[p.stem] = p
    return files


def parse_page_registry(nexus: Path) -> list[tuple[str, str]]:
    """Ordered (icon, label) for each *active* page entry in PageRegistry.
    Commented-out entries are ignored, matching pageComps ordering."""
    text = (nexus / "PageRegistry.qml").read_text()
    out: list[tuple[str, str]] = []
    # Each entry is a { ... } block with label/icon; commented lines start //.
    # Walk brace blocks at the array level.
    in_array = False
    depth = 0
    label = icon = None
    for line in text.splitlines():
        s = line.strip()
        if "pages:" in s and "[" in s:
            in_array = True
            continue
        if not in_array:
            continue
        if s.startswith("//"):
            continue
        if s.startswith("{"):
            depth += 1
            label = icon = None
            continue
        if s.startswith("}"):
            if label is not None:
                out.append((icon or "tune", label))
            depth -= 1
            continue
        if depth >= 1:
            m = LABEL_RE.match(line)
            if m and label is None:
                label = m.group(1)
            mi = ICON_RE.match(line)
            if mi and icon is None:
                icon = mi.group(1)
    return out


def parse_page_comps(nexus: Path) -> list[list[str]]:
    """Per top-level pageComps entry, ordered component names inside it."""
    text = (nexus / "PageCompRegistry.qml").read_text()
    body = text[text.index("pageComps:"):]
    comps: list[list[str]] = []
    current: list[str] | None = None
    for line in body.splitlines():
        if re.match(r"^        Component \{", line):
            current = []
            comps.append(current)
        m = re.search(r"([A-Z][A-Za-z]+)\s*\{\}", line)
        if m and current is not None:
            comps.append(current) if False else current.append(m.group(1))
    return comps


def dedup_crumbs(labels: list[str], icons: list[str]) -> tuple[list[str], list[str]]:
    """Drop consecutive duplicate labels (e.g. a section header that repeats the
    page name), keeping icons aligned."""
    out_labels: list[str] = []
    out_icons: list[str] = []
    for lbl, ico in zip(labels, icons):
        if out_labels and out_labels[-1] == lbl:
            continue
        out_labels.append(lbl)
        out_icons.append(ico)
    return out_labels, out_icons


def build_nav_map(nexus: Path, files: dict[str, Path]) -> dict[str, dict]:
    comps = parse_page_comps(nexus)
    registry = parse_page_registry(nexus)

    # Top-level index -> (icon, label) from PageRegistry (same order as pageComps).
    top_meta: dict[int, tuple[str, str]] = {}
    for i, (icon, label) in enumerate(registry):
        top_meta[i] = (icon, label)

    # parentName -> {childPos: (icon, label, section)} from openSubPage() +
    # nearby NavRow, remembering the section header the NavRow sits under.
    nav_children: dict[str, dict[int, tuple[str, str, str]]] = {}
    for names in comps:
        for name in names:
            pf = files.get(name)
            if not pf:
                continue
            pending_icon = pending_label = None
            section = ""  # text of the most recent SectionHeader
            expect_section = False  # next label line is that header's text
            for ln in read_lines(pf):
                if SECTION_RE.match(ln):
                    expect_section = True
                    continue
                ml = LABEL_RE.match(ln)
                if ml:
                    if expect_section:
                        section = ml.group(1)
                        expect_section = False
                    else:
                        pending_label = ml.group(1)
                    continue
                mi = ICON_RE.match(ln)
                if mi:
                    pending_icon = mi.group(1)
                mo = re.search(r"openSubPage\((\d+)\)", ln)
                if mo:
                    pos = int(mo.group(1))
                    nav_children.setdefault(name, {})[pos] = (
                        pending_icon or "tune", pending_label or "", section)
                    pending_icon = pending_label = None

    nav: dict[str, dict] = {}
    for top_idx, names in enumerate(comps):
        if not names:
            continue
        main = names[0]
        main_icon, main_label = top_meta.get(top_idx, ("tune", main))
        nav[main] = {"pageIdx": top_idx, "subPath": [],
                     "crumbIcons": [main_icon], "crumbLabels": [main_label]}
        children = dict(nav_children.get(main, {}))
        # Components that some other page opens via openSubPage. Those are reached
        # through that page (e.g. the bar pages are opened from inside Taskbar's
        # "Components" section), so they must not be linked directly here, which
        # would give them a wrong, shorter breadcrumb and navigation path.
        opened_via_subpage = set()
        for owner, kids in nav_children.items():
            # Find the group this owner component belongs to.
            owner_group = next((ns for ns in comps if owner in ns), None)
            if not owner_group:
                continue
            for kpos in kids:
                if kpos < len(owner_group):
                    opened_via_subpage.add(owner_group[kpos])
        # Fallback: a StackPage may list sub-pages (pos > 0) whose openSubPage()
        # call lives in a separate component file we don't scan (e.g. the
        # Ethernet detail page is opened from EthernetSection.qml). Link any such
        # sub-page by its position, deriving a label from its component name -
        # but skip ones already reached through another page.
        for pos in range(1, len(names)):
            if pos not in children and names[pos] not in opened_via_subpage:
                label = re.sub(r"(Detail)?Page$", "", names[pos])
                label = re.sub(r"(?<!^)(?=[A-Z])", " ", label)
                children[pos] = (main_icon, label, "")
        for pos, (icon, label, section) in children.items():
            if pos >= len(names):
                continue
            child = names[pos]
            # Insert the section header (e.g. "Components") as a breadcrumb step
            # between the parent page and the sub-page, when present.
            labels = [main_label] + ([section] if section else []) + [label]
            icons = [main_icon] + ([icon] if section else []) + [icon]
            labels, icons = dedup_crumbs(labels, icons)
            nav[child] = {"pageIdx": top_idx, "subPath": [pos],
                          "crumbIcons": icons,
                          "crumbLabels": labels}
            for gpos, (gicon, glabel, gsection) in nav_children.get(child, {}).items():
                if gpos >= len(names):
                    continue
                glabels = labels + ([gsection] if gsection else []) + [glabel]
                gicons = icons + ([gicon] if gsection else []) + [gicon]
                glabels, gicons = dedup_crumbs(glabels, gicons)
                nav[names[gpos]] = {
                    "pageIdx": top_idx, "subPath": [pos, gpos],
                    "crumbIcons": gicons,
                    "crumbLabels": glabels}
    return nav


def tokenize(text: str) -> list[str]:
    toks: list[str] = []
    # Process word by word (split on whitespace) so we only collapse separators
    # inside a single word like "Wi-Fi" -> "wifi", not across a whole phrase.
    for word in text.lower().split():
        parts = [p for p in re.split(r"[^a-z0-9]+", word) if p]
        for p in parts:
            if p not in STOPWORDS and p not in toks:
                toks.append(p)
        if len(parts) > 1:
            joined = "".join(parts)
            if joined not in toks:
                toks.append(joined)
    return toks


SUBTEXT_RE = re.compile(r'^\s*(?:subtext|status):\s*qsTr\("([^"]+)"\)')
SECTION_RE = re.compile(r'^\s*SectionHeader\s*\{')


def extract_settings(files: dict[str, Path], nav: dict[str, dict]) -> list[dict]:
    entries: list[dict] = []
    for comp, meta in nav.items():
        pf = files.get(comp)
        if not pf:
            continue
        lines = read_lines(pf)
        section = ""  # text of the most recent SectionHeader
        i = 0
        while i < len(lines):
            # Track the current section header so its words are searchable too.
            if SECTION_RE.match(lines[i]):
                for j in range(i + 1, min(i + 4, len(lines))):
                    m = LABEL_RE.match(lines[j])
                    if m:
                        section = m.group(1)
                        break
            row_match = ROW_RE.match(lines[i])
            if row_match:
                row_type = row_match.group(1)
                label = anchor = subtext = None
                checked_path = toggled_path = None
                for j in range(i + 1, min(i + 12, len(lines))):
                    if label is None:
                        m = LABEL_RE.match(lines[j])
                        if m:
                            label = m.group(1)
                    if anchor is None:
                        a = ANCHOR_RE.match(lines[j])
                        if a:
                            anchor = a.group(1)
                    if subtext is None:
                        st = SUBTEXT_RE.match(lines[j])
                        if st:
                            subtext = st.group(1)
                    if checked_path is None:
                        ch = CHECKED_RE.match(lines[j])
                        if ch:
                            checked_path = ch.group(1)
                    if toggled_path is None:
                        tg = ONTOGGLED_RE.match(lines[j])
                        if tg:
                            toggled_path = tg.group(1)
                # Only expose a toggle path when read and write target the same
                # property (symmetric), so flipping from search is safe.
                toggle_path = (
                    checked_path
                    if row_type == "ToggleRow" and checked_path and checked_path == toggled_path
                    else ""
                )
                if label and label not in SKIP_LABELS and anchor:
                    # keyword sources: breadcrumb path, section header, subtext.
                    extra = " ".join(meta["crumbLabels"]) + " " + section + " " + (subtext or "")
                    entries.append({
                        "pageIdx": meta["pageIdx"], "subPath": meta["subPath"],
                        "crumbIcons": meta["crumbIcons"],
                        "crumbLabels": meta["crumbLabels"],
                        "title": label, "anchor": anchor,
                        "section": section,
                        "subtext": subtext or "",
                        "togglePath": toggle_path,
                        "icon": SETTING_ICONS.get(anchor)
                        or (meta["crumbIcons"][-1] if meta["crumbIcons"] else ""),
                        "keywords": " ".join(sorted(set(tokenize(label + " " + extra)))),
                    })
            i += 1
    return entries


def build_inverted_and_ranking(entries: list[dict]):
    """Classic inverted index + precomputed per-token ranking weights."""
    inverted: dict[str, list[int]] = defaultdict(list)
    ranking: dict[str, dict[int, float]] = defaultdict(dict)
    for idx, e in enumerate(entries):
        fields = {"title": e["title"], "keywords": e["keywords"]}
        seen: set[str] = set()
        for field, text in fields.items():
            weight = FIELD_WEIGHT.get(field, 0.2)
            for tok in tokenize(text):
                if idx not in inverted[tok]:
                    inverted[tok].append(idx)
                # accumulate the strongest field weight for this token/entry
                ranking[tok][idx] = max(ranking[tok].get(idx, 0.0), weight)
                seen.add(tok)
    # sort each posting list by descending rank so runtime can stop early
    for tok, ids in inverted.items():
        ids.sort(key=lambda i: ranking[tok][i], reverse=True)
    return inverted, {t: {str(k): v for k, v in d.items()} for t, d in ranking.items()}


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    nexus = Path(sys.argv[1])
    out = Path(sys.argv[2])
    files = discover_files(nexus)
    nav = build_nav_map(nexus, files)
    entries = extract_settings(files, nav)
    inverted, ranking = build_inverted_and_ranking(entries)
    # keywords were only needed to build the inverted index; the runtime reads
    # the index, not the per-entry keyword blob, so drop it to shrink the JSON.
    for e in entries:
        e.pop("keywords", None)
    out.write_text(json.dumps({
        "version": 2,
        "entries": entries,
        "inverted": inverted,
        "ranking": ranking,
    }, ensure_ascii=False, indent=2))
    print(f"settings index: {len(entries)} entries, "
          f"{len(inverted)} tokens -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
