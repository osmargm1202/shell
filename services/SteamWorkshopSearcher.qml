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
    property string apiKey: GlobalConfig.services.steamWorkshopApiKey ?? ""
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
    property string selectedSource: ""
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
                if (response.result && response.result !== 1)
                    throw new Error(response.result_details ?? `Steam result ${response.result}`);
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
                loading = false;
                hasMore = false;
                console.warn("Steam Workshop request failed:", error);
                searchComplete([], {"error": String(error)});
            }
        }, error => {
            if (!enabled || generation !== requestGeneration)
                return;
            requestFailed(String(error));
        });
    }

    function requestFailed(error: string): void {
        loading = false;
        hasMore = false;
        console.warn("Steam Workshop request failed:", error);
        searchComplete([], {"error": error});
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

    function extension(path: string): string {
        const match = path.toLowerCase().match(/\.([a-z0-9]+)$/);
        return match ? match[1] : "";
    }

    function mediaRank(path: string): int {
        const ext = extension(path);
        if (ext === "mp4" || ext === "webm")
            return 0;
        if (ext === "gif")
            return 1;
        if (ext === "jpg" || ext === "jpeg" || ext === "png")
            return 2;
        return -1;
    }

    function beginCopy(source: string): void {
        const ext = extension(source);
        selectedSource = source;
        selectedDestination = `${Paths.wallsdir}/steam-${activeId}.${ext}`;
        const plan = WorkshopHelpers.copyPlan(selectedDestination);
        copyProc.command = ["bash", `${Quickshell.shellDir}/services/steam-workshop-media.sh`,
            "safe-copy", `${Paths.steamWorkshopContentDir}/${activeId}`, source, plan.destination];
        copyProc.running = true;
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
                "discover", `${Paths.steamWorkshopContentDir}/${activeId}`, mediaPreference];
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
            if (code !== 0) {
                root.finishFailure("Failed to inspect Workshop media");
                return;
            }
            const candidates = output.split("\n").map(line => {
                const separator = line.indexOf("\t");
                if (separator < 1)
                    return null;
                const path = Qt.atob(line.slice(separator + 1));
                const rank = root.mediaRank(path);
                if (rank < 0)
                    return null;
                return {
                    "size": Number(line.slice(0, separator)),
                    "path": path,
                    "rank": rank
                };
            }).filter(candidate => candidate !== null);
            candidates.sort((left, right) => left.rank - right.rank || right.size - left.size || left.path.localeCompare(right.path));
            if (!candidates.length) {
                root.finishFailure("No supported wallpaper media found");
                return;
            }
            root.beginCopy(candidates[0].path);
        }
    }

    Process {
        id: copyProc

        onExited: code => {
            if (code !== 0) {
                root.finishFailure("Failed to copy Workshop media");
                return;
            }
            root.downloadProgress(root.activeId, 1);
            root.downloadComplete(root.activeId, root.selectedDestination);
            root.activeId = "";
        }
    }
}
