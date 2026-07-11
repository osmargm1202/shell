from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    return (ROOT / path).read_text()


def test_service_config_declares_wallpaper_sources() -> None:
    source = text("plugin/src/Caelestia/Config/serviceconfig.hpp")
    assert "CONFIG_GLOBAL_PROPERTY(QString, wallhavenApiKey)" in source
    assert "CONFIG_GLOBAL_PROPERTY(bool, wallhavenEnabled, true)" in source
    assert "CONFIG_GLOBAL_PROPERTY(QString, steamWorkshopApiKey)" in source
    assert "CONFIG_GLOBAL_PROPERTY(bool, steamWorkshopEnabled, true)" in source
    assert "CONFIG_GLOBAL_PROPERTY(QString, steamWorkshopSteamRoot" in source
    assert "CONFIG_GLOBAL_PROPERTY(QString, steamWorkshopUsername)" in source


def test_paths_expose_wallpaper_engine_directories() -> None:
    source = text("utils/Paths.qml")
    assert "readonly property string steamRoot" in source
    assert "readonly property string steamWorkshopContentDir" in source
    assert "readonly property string steamWorkshopDownloadDir" in source
    assert source.count("431960") == 2


def test_wallhaven_obeys_enabled_flag_and_does_not_log_secret_url() -> None:
    source = text("services/WallhavenSearcher.qml")
    assert "property bool enabled: GlobalConfig.services.wallhavenEnabled" in source
    assert source.count("if (!enabled)") >= 4
    assert 'console.log("Wallhaven search:", url)' not in source
    assert 'console.log("Wallhaven random:", url)' not in source


def test_steamcmd_is_an_unconditional_runtime_dependency() -> None:
    source = text("nix/default.nix")
    assert "  steamcmd," in source
    runtime_block = source.split("runtimeDeps =", 1)[1].split("]", 1)[0]
    assert "steamcmd" in runtime_block
