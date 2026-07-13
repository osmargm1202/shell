pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io
import Caelestia.Config
import qs.utils
import "SteamWorkshopHelpers.js" as WorkshopHelpers

Singleton {
    id: root

    readonly property string apiBase: "https://api.steampowered.com/IPublishedFileService/QueryFiles/v1/"
    readonly property string appId: "431960" // Wallpaper Engine app ID 431960 is used for API queries and steamcmd.
    property string apiKey: WorkshopHelpers.effectiveApiKey(GlobalConfig.services.steamWorkshopApiKey ?? "", Quickshell.env("STEAM_WEB_API_KEY") ?? "")
    property bool enabled: GlobalConfig.services.steamWorkshopEnabled
    property string username: GlobalConfig.services.steamWorkshopUsername
    property bool loading: false
    readonly property bool missingApiKey: WorkshopHelpers.isMissingApiKey(apiKey)
    property string lastQuery: ""
    property string nextCursor: "*"
    property bool hasMore: false
    property int queryType: 12
    property string mediaPreference: "all"
    property int requestGeneration: 0
    property var requestedCursors: []
    property list<var> results
    property var currentItem: null
    property string activeId: ""
    property string expectedBytes: "0"
    property string selectedDestination: ""
    property string stdoutBuffer: ""
    property string stderrBuffer: ""
    property bool cancellingDownload: false

    signal searchComplete(var results, var meta)
    signal downloadProgress(string id, real progress)
    signal downloadComplete(string id, string path)
    signal downloadFailed(string id, string error)
    signal authRequired(string username)

    function normalizeItem(item: var): var {
        return {
            "id": String(item.publishedfileid ?? ""),
            "title": String(item.title ?? ""),
            "description": String(item.file_description ?? item.description ?? ""),
            "previewUrl": String(item.preview_url ?? (item.previews && item.previews.length ? item.previews[0].url : "")),
            "fileSize": Number(item.file_size ?? 0),
            "timeUpdated": Number(item.time_updated ?? 0),
            "tags": WorkshopHelpers.normalizedTags(item.tags),
            "raw": item
        };
    }

    function buildUrl(query: string, cursor: string): string {
        const params = {
            "key": apiKey,
            "query_type": queryType.toString(),
            "cursor": cursor || "*",
            "numperpage": "20",
            "creator_appid": appId,
            "appid": appId,
            "search_text": query,
            "filetype": "0",
            "return_tags": "true",
            "return_previews": "true"
        };
        return apiBase + "?" + Object.keys(params)
            .map(key => `${key}=${encodeURIComponent(params[key])}`)
            .join("&");
    }

    function search(query: string): void {
        if (!enabled || !query.trim())
            return;
        requestGeneration++;
        if (missingApiKey) {
            loading = false;
            hasMore = false;
            searchComplete([], {"missingApiKey": true});
            return;
        }
        lastQuery = query.trim();
        nextCursor = "*";
        hasMore = false;
        requestedCursors = [];
        results = [];
        requestPage(nextCursor, false);
    }

    function searchNextPage(): void {
        if (!enabled || loading || !hasMore || !lastQuery)
            return;
        requestPage(nextCursor, true);
    }

    function setQueryType(type: int): void {
        queryType = type;
    }

    function setMediaPreference(preference: string): void {
        if (["all", "video", "gif", "image"].indexOf(preference) >= 0)
            mediaPreference = preference;
    }

    function formattedUpdateDate(timestamp: var): string {
        return WorkshopHelpers.formattedUpdateDate(timestamp);
    }

    function requestPage(cursor: string, append: bool): void {
        const generation = requestGeneration;
        const requestedCursor = String(cursor || "*");
        requestedCursors = [...requestedCursors, requestedCursor];
        loading = true;
        console.log("Steam Workshop search:", lastQuery, "cursor", cursor === "*" ? "initial" : "next");
        Requests.get(buildUrl(lastQuery, cursor), text => {
            if (!enabled || generation !== requestGeneration)
                return;
            try {
                const json = JSON.parse(text);
                const response = json.response ?? {};
                if (response.result && response.result !== 1) {
                    requestFailed(WorkshopHelpers.classifyRequestFailure("api", 200, response.result));
                    return;
                }
                const page = (response.publishedfiledetails ?? []).map(item => normalizeItem(item));
                const merged = WorkshopHelpers.mergePage(append ? results : [], page,
                    requestedCursor, response.next_cursor, requestedCursors);
                results = merged.results;
                nextCursor = merged.nextCursor;
                hasMore = merged.hasMore;
                loading = false;
                searchComplete(merged.added, {
                    "nextCursor": nextCursor,
                    "hasMore": hasMore,
                    "total": Number(response.total ?? results.length)
                });
            } catch (error) {
                requestFailed(WorkshopHelpers.classifyRequestFailure("parse", 200, 0));
            }
        }, (error, status) => {
            if (!enabled || generation !== requestGeneration)
                return;
            requestFailed(WorkshopHelpers.classifyTransportError(error, status));
        });
    }

    function requestFailed(failure: var): void {
        loading = false;
        hasMore = false;
        console.warn("Steam Workshop request failed:", failure.kind);
        searchComplete([], {
            "error": failure.message,
            "errorKind": failure.kind,
            "retryLater": failure.retryLater
        });
    }

    function downloadItem(item: var): void {
        if (!enabled || !item)
            return;
        if (activeId || cancellingDownload || downloadProc.running) {
            downloadFailed(String(item.id ?? item.publishedfileid ?? ""), "A Workshop download is already active");
            return;
        }
        const id = WorkshopHelpers.validatedId(item.id ?? item.publishedfileid);
        if (!id) {
            downloadFailed("", "Invalid Workshop item ID");
            return;
        }
        activeId = id;
        expectedBytes = String(Number(item.fileSize ?? item.file_size ?? 0));
        stdoutBuffer = "";
        stderrBuffer = "";
        downloadProc.command = [
            "env", "steamcmd",
            "+force_install_dir", Paths.steamRoot,
            "+login", username || "anonymous",
            "+workshop_download_item", appId, id,
            "+quit"
        ];
        watchdog.restart();
        progressPoller.start();
        downloadProc.running = true;
    }

    function finishFailure(error: string): void {
        watchdog.stop();
        progressPoller.stop();
        downloadFailed(activeId, error);
        activeId = "";
    }

    onEnabledChanged: {
        if (!enabled) {
            const state = WorkshopHelpers.disabledRequestState(requestGeneration);
            requestGeneration = state.requestGeneration;
            loading = state.loading;
            hasMore = state.hasMore;
            nextCursor = state.nextCursor;
            requestedCursors = state.requestedCursors;
        }
    }

    IpcHandler {
        function doSearch(query: string): void {
            root.search(query);
        }

        target: "steamworkshop"
    }

    Process {
        id: downloadProc

        stdout: SplitParser {
            splitMarker: ""
            onRead: data => root.stdoutBuffer += data
        }
        stderr: SplitParser {
            splitMarker: ""
            onRead: data => root.stderrBuffer += data
        }
        onExited: code => {
            watchdog.stop();
            progressPoller.stop();
            if (root.cancellingDownload) {
                root.cancellingDownload = false;
                return;
            }
            if (code !== 0) {
                const failure = WorkshopHelpers.classifySteamcmdFailure(code, stdoutBuffer, stderrBuffer);
                if (failure === "auth") {
                    authRequired(username);
                    finishFailure("Steam authentication required. Run steamcmd +login <username> once.");
                } else if (failure === "missing") {
                    finishFailure("steamcmd must be installed and available on PATH");
                } else if (failure === "item") {
                    finishFailure(`Workshop item ${activeId} is unavailable. Check that it exists and is visible, or authenticate an account with access.`);
                } else {
                    finishFailure(`steamcmd failed for Workshop item ${activeId} with exit code ${code}`);
                }
                return;
            }
            mediaFinder.command = ["bash", `${Quickshell.shellDir}/services/steam-workshop-media.sh`,
                "install", `${Paths.steamWorkshopContentDir}/${activeId}`, mediaPreference, Paths.wallsdir, activeId];
            mediaFinder.running = true;
        }
    }

    Timer {
        id: progressPoller

        interval: 800
        repeat: true
        onTriggered: {
            if (!root.activeId || Number(root.expectedBytes) <= 0 || sizeProc.running)
                return;
            sizeProc.command = ["du", "-sb", `${Paths.steamWorkshopDownloadDir}/${root.activeId}`];
            sizeProc.running = true;
        }
    }

    Process {
        id: sizeProc

        property string output: ""

        stdout: SplitParser {
            splitMarker: ""
            onRead: data => sizeProc.output += data
        }
        onRunningChanged: if (running) output = ""
        onExited: code => {
            if (code !== 0)
                return;
            const bytes = Number(output.trim().split(/\s+/)[0] ?? 0);
            downloadProgress(root.activeId, Math.min(bytes / Number(root.expectedBytes), 0.99));
        }
    }

    Timer {
        id: watchdog

        interval: 600000
        onTriggered: {
            root.cancellingDownload = true;
            downloadProc.running = false;
            root.finishFailure("Steam Workshop download timed out");
        }
    }

    Process {
        id: mediaFinder

        property string output: ""

        stdout: SplitParser {
            splitMarker: ""
            onRead: data => mediaFinder.output += data
        }
        onRunningChanged: if (running) output = ""
        onExited: code => {
            if (code === 4) {
                root.finishFailure("No supported wallpaper media found");
                return;
            }
            if (code !== 0) {
                root.finishFailure("Failed to install Workshop media");
                return;
            }
            const separator = output.indexOf("\t");
            if (separator < 1 || output.slice(0, separator) !== "OK") {
                root.finishFailure("Failed to install Workshop media");
                return;
            }
            root.selectedDestination = output.slice(separator + 1).replace(/\n$/, "");
            root.downloadProgress(root.activeId, 1);
            root.downloadComplete(root.activeId, root.selectedDestination);
            root.activeId = "";
        }
    }
}
