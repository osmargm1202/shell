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


def test_flake_permits_only_steamcmd_as_unfree() -> None:
    source = text("flake.nix")
    assert "fn (import nixpkgs" in source
    assert 'allowUnfreePredicate = pkg: nixpkgs.lib.getName pkg == "steamcmd";' in source
    assert "steamcmd = pkgs.steamcmd.override {steam-run = pkgs.steam-run-free;};" in source
    assert "allowUnfree = true;" not in source


def test_steam_service_uses_wallpaper_engine_api_without_leaking_key() -> None:
    source = text("services/SteamWorkshopSearcher.qml")
    assert "IPublishedFileService/QueryFiles/v1/" in source
    assert source.count("431960") >= 2
    assert "108600" not in source
    assert 'target: "steamworkshop"' in source
    assert 'console.log("Steam Workshop URL"' not in source
    assert "function requestFailed(failure: var)" in source
    assert "property int requestGeneration" in source
    assert source.count("generation !== requestGeneration") >= 2
    assert "if (missingApiKey) {\n            loading = false;" in source


def test_steam_service_exposes_cursor_search_and_download_contract() -> None:
    source = text("services/SteamWorkshopSearcher.qml")
    for contract in (
        "property string nextCursor",
        "function search(query: string)",
        "function searchNextPage()",
        "function downloadItem(item: var)",
        "signal authRequired(string username)",
        "signal downloadComplete(string id, string path)",
    ):
        assert contract in source
    assert "activeId || cancellingDownload || downloadProc.running" in source
    helper = text("services/steam-workshop-media.sh")
    for extension in ("mp4", "webm", "gif", "jpg", "jpeg", "png"):
        assert extension in helper
    assert '"install"' in source
    assert '"discover"' not in source
    assert '"safe-copy"' not in source


def test_nexus_registers_steam_workshop_page() -> None:
    registry = text("modules/nexus/PageCompRegistry.qml")
    selector = text("modules/nexus/pages/wallandstyle/WallpaperSelect.qml")
    page = text("modules/nexus/pages/wallandstyle/SteamWorkshopPage.qml")
    assert "SteamWorkshopPage {}" in registry
    assert 'text: qsTr("Steam Workshop")' in selector
    assert "GlobalConfig.services.steamWorkshopEnabled" in selector
    assert "SteamWorkshopTab" in page


def test_steam_tab_connects_search_download_and_set() -> None:
    source = text("modules/dashboard/SteamWorkshopTab.qml")
    assert "SteamWorkshopSearcher.search(" in source
    assert "SteamWorkshopSearcher.searchNextPage()" in source
    assert "SteamWorkshopSearcher.downloadItem(" in source
    assert "Wallpapers.setWallpaper(path)" in source
    assert "Steam authentication required" in source


def test_dashboard_registers_both_remote_wallpaper_sources() -> None:
    source = text("modules/dashboard/Content.qml")
    assert "component: wallhavenComponent" in source
    assert "enabled: GlobalConfig.services.wallhavenEnabled" in source
    assert "component: steamWorkshopComponent" in source
    assert "enabled: GlobalConfig.services.steamWorkshopEnabled" in source
    assert "WallhavenTab {}" in source
    assert "SteamWorkshopTab {}" in source
    assert "Math.min(root.screenState.dashboardTab" in source
