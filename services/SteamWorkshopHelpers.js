.pragma library

function validatedId(value) {
    if (value === null || value === undefined)
        return "";
    const id = String(value);
    return /^[0-9]+$/.test(id) ? id : "";
}

function isMissingApiKey(value) {
    return String(value === null || value === undefined ? "" : value).trim().length === 0;
}

function mergePage(existing, page, requestedCursor, nextCursor, requestedCursors) {
    const results = [];
    const added = [];
    const ids = {};

    function appendUnique(item, isNewPage) {
        const id = validatedId(item && item.id);
        if (!id || ids[id])
            return;
        ids[id] = true;
        results.push(item);
        if (isNewPage)
            added.push(item);
    }

    (existing || []).forEach(item => appendUnique(item, false));
    (page || []).forEach(item => appendUnique(item, true));

    const next = typeof nextCursor === "string" ? nextCursor : "";
    const requested = typeof requestedCursor === "string" ? requestedCursor : "";
    const seen = (requestedCursors || []).map(cursor => String(cursor));
    const cycles = !next || next === requested || seen.indexOf(next) >= 0;

    return {
        "results": results,
        "added": added,
        "nextCursor": next,
        "hasMore": added.length > 0 && !cycles
    };
}

function classifyTransportError(error, httpStatus) {
    const objectStatus = Number(httpStatus || error && (error.status || error.statusCode || error.httpStatus) || 0);
    const raw = String(error === null || error === undefined ? "" : error);
    const rateLimited = /\b429\b/.test(raw) || /too many requests/i.test(raw);
    const status = objectStatus || (rateLimited ? 429 : 0);
    return classifyRequestFailure(status === 429 ? "http" : "transport", status, 0);
}

function classifyRequestFailure(source, status, apiResult) {
    const httpStatus = Number(status || 0);
    const result = Number(apiResult || 0);
    if (httpStatus === 429 || (source === "api" && (result === 29 || result === 84))) {
        return {
            "kind": "rateLimit",
            "message": "Steam Workshop rate limit reached. Retry later.",
            "retryLater": true
        };
    }
    if (source === "transport") {
        return {
            "kind": "transport",
            "message": "Unable to reach Steam Workshop. Check your connection and try again.",
            "retryLater": false
        };
    }
    if (source === "parse") {
        return {
            "kind": "response",
            "message": "Steam Workshop returned an invalid response. Try again.",
            "retryLater": false
        };
    }
    return {
        "kind": "service",
        "message": "Steam Workshop is temporarily unavailable. Try again later.",
        "retryLater": false
    };
}

function disabledRequestState(requestGeneration) {
    return {
        "requestGeneration": Number(requestGeneration) + 1,
        "loading": false,
        "hasMore": false,
        "nextCursor": "",
        "requestedCursors": []
    };
}

function classifySteamcmdFailure(code, stdout, stderr) {
    const output = `${stdout || ""}\n${stderr || ""}`.toLowerCase();
    if (Number(code) === 127
            || output.includes("no such file or directory")
            || output.includes("command not found")
            || output.includes("failed to start"))
        return "missing";
    if (output.includes("login failure")
            || output.includes("invalid password")
            || output.includes("two-factor")
            || output.includes("access denied")
            || output.includes("not logged on")
            || output.includes("not logged in")
            || output.includes("account logon denied")
            || output.includes("invalid login auth code")
            || output.includes("session replaced"))
        return "auth";
    if (output.includes("file not found")
            || output.includes("does not exist")
            || output.includes("no subscription")
            || (output.includes("download item") && output.includes("failed")))
        return "item";
    return "failure";
}

function normalizedTags(tags) {
    const normalized = [];
    (tags || []).forEach(value => {
        const tag = typeof value === "string" ? value : String(value && value.tag || "");
        if (tag && normalized.indexOf(tag) < 0)
            normalized.push(tag);
    });
    return normalized;
}

function formattedUpdateDate(timestamp) {
    const seconds = Number(timestamp || 0);
    if (!seconds)
        return "";
    return new Date(seconds * 1000).toISOString().slice(0, 10);
}
