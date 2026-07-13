import base64
import json
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
HELPERS = ROOT / "services/SteamWorkshopHelpers.js"
MEDIA_TOOL = ROOT / "services/steam-workshop-media.sh"


def call_helper(function: str, *args):
    script = r"""
const fs = require("fs");
const vm = require("vm");
const filename = process.argv[1];
const fn = process.argv[2];
const args = JSON.parse(process.argv[3]);
const source = fs.readFileSync(filename, "utf8").replace(/^\.pragma library\s*$/m, "");
const context = {};
vm.createContext(context);
vm.runInContext(source, context, { filename });
process.stdout.write(JSON.stringify(context[fn](...args)));
"""
    result = subprocess.run(
        ["node", "-e", script, str(HELPERS), function, json.dumps(args)],
        check=True,
        capture_output=True,
        text=True,
    )
    return json.loads(result.stdout)


def discover(root: Path, preference: str = "all") -> list[tuple[int, Path]]:
    result = subprocess.run(
        ["bash", str(MEDIA_TOOL), "discover", str(root), preference],
        check=True,
        capture_output=True,
        text=True,
    )
    records = []
    for line in result.stdout.splitlines():
        size, encoded_path = line.split("\t", 1)
        records.append((int(size), Path(base64.b64decode(encoded_path).decode())))
    return records


@pytest.mark.parametrize("invalid", ["12/34", " 123", "123 ", "-1", "1e3", "", None])
def test_workshop_id_rejects_original_non_numeric_value(invalid) -> None:
    assert call_helper("validatedId", invalid) == ""


def test_workshop_id_preserves_valid_numeric_value() -> None:
    assert call_helper("validatedId", "001234") == "001234"


def test_missing_api_key_is_derived_from_trimmed_configuration() -> None:
    assert call_helper("isMissingApiKey", " \t\n") is True
    assert call_helper("isMissingApiKey", " configured ") is False


def test_cursor_merge_deduplicates_overlap_and_stops_cycles() -> None:
    existing = [{"id": "1", "title": "one"}, {"id": "2", "title": "two"}]
    overlapping = [{"id": "2", "title": "duplicate"}, {"id": "3", "title": "three"}]

    merged = call_helper("mergePage", existing, overlapping, "cursor-a", "cursor-b", ["*", "cursor-a"])
    assert [item["id"] for item in merged["results"]] == ["1", "2", "3"]
    assert [item["id"] for item in merged["added"]] == ["3"]
    assert merged["hasMore"] is True

    repeated = call_helper("mergePage", merged["results"], [{"id": "4"}], "cursor-b", "cursor-a", ["*", "cursor-a", "cursor-b"])
    assert repeated["hasMore"] is False

    unchanged = call_helper("mergePage", existing, [{"id": "3"}], "cursor-a", "cursor-a", ["cursor-a"])
    assert unchanged["hasMore"] is False

    no_new_items = call_helper("mergePage", existing, [{"id": "2"}], "cursor-a", "cursor-b", ["cursor-a"])
    assert no_new_items["hasMore"] is False


@pytest.mark.parametrize(
    ("code", "stdout", "stderr", "expected"),
    [
        (5, "ERROR! Download item 42 failed (Access Denied).", "", "auth"),
        (5, "FAILED (Invalid Password)", "", "auth"),
        (5, "Not logged on", "", "auth"),
        (127, "", "env: steamcmd: No such file or directory", "missing"),
        (1, "", "execvp: steamcmd: command not found", "missing"),
        (5, "ERROR! Download item 42 failed (File Not Found).", "", "item"),
        (5, "Workshop item does not exist", "", "item"),
        (5, "ERROR! Download item 42 failed (Failure).", "", "item"),
        (8, "network unavailable", "", "failure"),
    ],
)
def test_steamcmd_failure_classification_uses_stdout_and_stderr(code, stdout, stderr, expected) -> None:
    assert call_helper("classifySteamcmdFailure", code, stdout, stderr) == expected


@pytest.mark.parametrize("malformed", [{"cursor": "x"}, ["x"], True, 42])
def test_cursor_merge_rejects_non_string_cursors(malformed) -> None:
    merged = call_helper("mergePage", [], [{"id": "1"}], "*", malformed, ["*"])
    assert merged["nextCursor"] == ""
    assert merged["hasMore"] is False


def test_copy_plan_installs_directly_to_final_destination() -> None:
    plan = call_helper("copyPlan", "/walls/steam-42.mp4")
    assert plan == {"destination": "/walls/steam-42.mp4", "requiresSecondMove": False}


def test_media_discovery_handles_delimiters_and_filters_media_type(tmp_path: Path) -> None:
    video = tmp_path / "video\tname\nclip.mp4"
    image = tmp_path / "still.png"
    video.write_bytes(b"v" * 7)
    image.write_bytes(b"i" * 11)

    assert discover(tmp_path, "video") == [(7, video.resolve())]
    assert discover(tmp_path, "image") == [(11, image.resolve())]
    assert {path for _, path in discover(tmp_path)} == {video.resolve(), image.resolve()}


def test_media_discovery_rejects_symlinks_that_escape_item_root(tmp_path: Path) -> None:
    item_root = tmp_path / "item"
    item_root.mkdir()
    outside = tmp_path / "outside.mp4"
    outside.write_bytes(b"outside")
    (item_root / "escape.mp4").symlink_to(outside)

    assert discover(item_root) == []


def test_safe_copy_rechecks_canonical_containment_and_rejects_symlink(tmp_path: Path) -> None:
    item_root = tmp_path / "item"
    item_root.mkdir()
    outside = tmp_path / "outside.mp4"
    outside.write_bytes(b"outside")
    escaping_link = item_root / "escape.mp4"
    escaping_link.symlink_to(outside)
    destination = tmp_path / "destination.tmp"

    result = subprocess.run(
        ["bash", str(MEDIA_TOOL), "safe-copy", str(item_root), str(escaping_link), str(destination)],
        capture_output=True,
        text=True,
    )

    assert result.returncode != 0
    assert not destination.exists()


def test_disabling_service_invalidates_and_clears_active_search_state() -> None:
    state = call_helper("disabledRequestState", 7)
    assert state == {
        "requestGeneration": 8,
        "loading": False,
        "hasMore": False,
        "nextCursor": "",
        "requestedCursors": [],
    }


def test_safe_copy_replaces_temporary_destination_symlink_without_following_it(tmp_path: Path) -> None:
    item_root = tmp_path / "item"
    item_root.mkdir()
    source = item_root / "wallpaper.mp4"
    source.write_bytes(b"wallpaper")
    outside = tmp_path / "outside"
    outside.write_bytes(b"do-not-overwrite")
    destination = tmp_path / "destination.tmp"
    destination.symlink_to(outside)

    subprocess.run(
        ["bash", str(MEDIA_TOOL), "safe-copy", str(item_root), str(source), str(destination)],
        check=True,
    )

    assert destination.is_file()
    assert not destination.is_symlink()
    assert destination.read_bytes() == b"wallpaper"
    assert outside.read_bytes() == b"do-not-overwrite"


def test_metadata_helpers_normalize_tags_and_format_update_date() -> None:
    tags = call_helper("normalizedTags", [{"tag": "Video"}, "Relaxing", {"name": "Ignored"}, {"tag": "Video"}])
    assert tags == ["Video", "Relaxing"]
    assert call_helper("formattedUpdateDate", 0) == ""
    assert call_helper("formattedUpdateDate", 1_700_000_000).startswith("2023-")
