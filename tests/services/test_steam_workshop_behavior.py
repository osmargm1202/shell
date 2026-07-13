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


def install_media(root: Path, preference: str, walls_dir: Path, item_id: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["bash", str(MEDIA_TOOL), "install", str(root), preference, str(walls_dir), item_id],
        capture_output=True,
        text=True,
    )


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


def test_media_install_keeps_non_ascii_source_filename_inside_helper_contract(tmp_path: Path) -> None:
    item_root = tmp_path / "item"
    walls_dir = tmp_path / "walls"
    item_root.mkdir()
    walls_dir.mkdir()
    source = item_root / "vídeo-壁紙.mp4"
    source.write_bytes(b"wallpaper")

    result = install_media(item_root, "all", walls_dir, "42")

    destination = walls_dir / "steam-42.mp4"
    assert result.returncode == 0
    assert result.stdout == f"OK\t{destination}\n"
    assert source.name not in result.stdout
    assert destination.read_bytes() == b"wallpaper"


def test_media_install_ranks_type_then_size_and_filters_preference(tmp_path: Path) -> None:
    item_root = tmp_path / "item"
    walls_dir = tmp_path / "walls"
    item_root.mkdir()
    walls_dir.mkdir()
    (item_root / "large.png").write_bytes(b"i" * 20)
    preferred = item_root / "small.mp4"
    preferred.write_bytes(b"v" * 4)

    result = install_media(item_root, "all", walls_dir, "42")
    assert result.returncode == 0
    assert (walls_dir / "steam-42.mp4").read_bytes() == preferred.read_bytes()

    result = install_media(item_root, "image", walls_dir, "42")
    assert result.returncode == 0
    assert (walls_dir / "steam-42.png").read_bytes() == b"i" * 20


def test_media_install_rejects_symlinks_that_escape_item_root(tmp_path: Path) -> None:
    item_root = tmp_path / "item"
    walls_dir = tmp_path / "walls"
    item_root.mkdir()
    walls_dir.mkdir()
    outside = tmp_path / "outside.mp4"
    outside.write_bytes(b"outside")
    (item_root / "escape.mp4").symlink_to(outside)

    result = install_media(item_root, "all", walls_dir, "42")

    assert result.returncode == 4
    assert result.stdout == "NONE\n"
    assert not list(walls_dir.iterdir())


def test_disabling_service_invalidates_and_clears_active_search_state() -> None:
    state = call_helper("disabledRequestState", 7)
    assert state == {
        "requestGeneration": 8,
        "loading": False,
        "hasMore": False,
        "nextCursor": "",
        "requestedCursors": [],
    }


def test_media_install_atomically_replaces_destination_symlink_without_following_it(tmp_path: Path) -> None:
    item_root = tmp_path / "item"
    walls_dir = tmp_path / "walls"
    item_root.mkdir()
    walls_dir.mkdir()
    source = item_root / "wallpaper.mp4"
    source.write_bytes(b"wallpaper")
    outside = tmp_path / "outside"
    outside.write_bytes(b"do-not-overwrite")
    destination = walls_dir / "steam-42.mp4"
    destination.symlink_to(outside)

    result = install_media(item_root, "all", walls_dir, "42")

    assert result.returncode == 0
    assert destination.is_file()
    assert not destination.is_symlink()
    assert destination.read_bytes() == b"wallpaper"
    assert outside.read_bytes() == b"do-not-overwrite"


@pytest.mark.parametrize(
    ("source", "status", "api_result", "expected"),
    [
        ("transport", 0, 0, {"kind": "transport", "message": "Unable to reach Steam Workshop. Check your connection and try again.", "retryLater": False}),
        ("http", 500, 0, {"kind": "service", "message": "Steam Workshop is temporarily unavailable. Try again later.", "retryLater": False}),
        ("http", 429, 0, {"kind": "rateLimit", "message": "Steam Workshop rate limit reached. Retry later.", "retryLater": True}),
        ("api", 200, 29, {"kind": "rateLimit", "message": "Steam Workshop rate limit reached. Retry later.", "retryLater": True}),
        ("api", 200, 84, {"kind": "rateLimit", "message": "Steam Workshop rate limit reached. Retry later.", "retryLater": True}),
        ("parse", 200, 0, {"kind": "response", "message": "Steam Workshop returned an invalid response. Try again.", "retryLater": False}),
    ],
)
def test_request_failures_are_classified_into_fixed_safe_messages(source, status, api_result, expected) -> None:
    assert call_helper("classifyRequestFailure", source, status, api_result) == expected


@pytest.mark.parametrize(
    "raw",
    [
        "GET https://api.steampowered.com/query?key=super-secret&search=x failed with HTTP 429",
        "Error transferring https://api.steampowered.com/query?key=super-secret - server replied: Too Many Requests",
    ],
)
def test_key_bearing_transport_error_becomes_safe_fixed_message(raw) -> None:
    failure = call_helper("classifyTransportError", raw)
    assert failure == {
        "kind": "rateLimit",
        "message": "Steam Workshop rate limit reached. Retry later.",
        "retryLater": True,
    }
    assert "super-secret" not in json.dumps(failure)
    assert "key=" not in json.dumps(failure)


def test_requests_error_callback_provides_http_status_without_logging_raw_error() -> None:
    requests = (ROOT / "plugin/src/Caelestia/requests.cpp").read_text()
    assert "QNetworkRequest::HttpStatusCodeAttribute" in requests
    assert "onError.call({ reply->errorString(), status })" in requests
    assert '<< reply->errorString()' not in requests


def test_qml_never_logs_or_displays_raw_key_bearing_request_errors() -> None:
    qml = (ROOT / "services/SteamWorkshopSearcher.qml").read_text()
    assert "error, status" in qml
    assert "requestFailed(String(error))" not in qml
    assert '"error": String(error)' not in qml
    assert 'console.warn("Steam Workshop request failed:", error)' not in qml
    assert "Qt.atob" not in qml
    assert '"discover"' not in qml
    assert '"safe-copy"' not in qml


def test_metadata_helpers_normalize_tags_and_format_update_date() -> None:
    tags = call_helper("normalizedTags", [{"tag": "Video"}, "Relaxing", {"name": "Ignored"}, {"tag": "Video"}])
    assert tags == ["Video", "Relaxing"]
    assert call_helper("formattedUpdateDate", 0) == ""
    assert call_helper("formattedUpdateDate", 1_700_000_000).startswith("2023-")
