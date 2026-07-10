#include "vpn.hpp"

#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qloggingcategory.h>
#include <qregularexpression.h>
#include <utility>

Q_LOGGING_CATEGORY(lcVpn, "caelestia.services.vpn", QtInfoMsg)

namespace {

QStringList toStringList(const QVariant& value) {
    QStringList list;
    const auto variants = value.toList();
    for (const auto& v : variants) {
        list << v.toString();
    }
    return list;
}

} // namespace

namespace caelestia::services {

VpnCore::VpnCore(QObject* parent)
    : QObject(parent)
    , m_status(makeStatus(false, QStringLiteral("disconnected")))
    , m_connectProc(new QProcess(this))
    , m_disconnectProc(new QProcess(this))
    , m_statusProc(new QProcess(this))
    , m_warpRegisterProc(new QProcess(this))
    , m_statusCheckTimer(new QTimer(this)) {
    applyEnvironment(m_connectProc);
    applyEnvironment(m_disconnectProc);
    applyEnvironment(m_statusProc);
    applyEnvironment(m_warpRegisterProc);

    m_warpRegisterProc->setStandardOutputFile(QProcess::nullDevice());
    m_warpRegisterProc->setStandardErrorFile(QProcess::nullDevice());

    // Line-buffered stdout so auth URLs (e.g. `tailscale up` printing a login
    // link and then blocking) surface immediately, before the process exits.
    QObject::connect(m_connectProc, &QProcess::readyReadStandardOutput, this, [this] {
        m_connectStdoutBuffer += QString::fromUtf8(m_connectProc->readAllStandardOutput());
        qsizetype idx = -1;
        while ((idx = m_connectStdoutBuffer.indexOf(u'\n')) >= 0) {
            const QString line = m_connectStdoutBuffer.left(idx);
            m_connectStdoutBuffer.remove(0, idx + 1);
            processConnectStdoutLine(line);
        }
    });
    QObject::connect(m_connectProc, &QProcess::finished, this, &VpnCore::onConnectFinished);
    QObject::connect(m_connectProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCWarning(lcVpn) << "Failed to start connect command" << m_config.connectCmd;
        }
    });
    QObject::connect(m_connectProc, &QProcess::stateChanged, this, &VpnCore::updateConnecting);

    QObject::connect(m_disconnectProc, &QProcess::finished, this, [this] {
        const QString error = QString::fromUtf8(m_disconnectProc->readAllStandardError()).trimmed();
        // wg-quick prints its commands to stderr prefixed with "[#]"
        if (!error.isEmpty() && !error.contains(QStringLiteral("[#]"))) {
            qCWarning(lcVpn) << "Disconnection error:" << error;
        }
        m_statusCheckTimer->start();
    });
    QObject::connect(m_disconnectProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCWarning(lcVpn) << "Failed to start disconnect command" << m_config.disconnectCmd;
        }
    });
    QObject::connect(m_disconnectProc, &QProcess::stateChanged, this, &VpnCore::updateConnecting);

    QObject::connect(m_statusProc, &QProcess::finished, this, &VpnCore::onStatusFinished);
    QObject::connect(m_statusProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCWarning(lcVpn) << "Failed to start status command for provider" << m_config.name;
            // Without this the last status (possibly connected) sticks forever.
            updateStatus(parseStatusOutput(QString()));
        }
    });

    QObject::connect(m_warpRegisterProc, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit && exitCode == 0) {
            m_statusCheckTimer->start();
        }
    });

    m_statusCheckTimer->setSingleShot(true);
    m_statusCheckTimer->setInterval(500);
    QObject::connect(m_statusCheckTimer, &QTimer::timeout, this, &VpnCore::checkStatus);
}

VpnCore::~VpnCore() {
    killProcess(m_connectProc);
    killProcess(m_disconnectProc);
    killProcess(m_statusProc);
    killProcess(m_warpRegisterProc);
}

QVariantList VpnCore::providers() const {
    return m_providers;
}

void VpnCore::setProviders(const QVariantList& providers) {
    if (m_hasResolved && providers == m_providers) {
        return;
    }
    m_providers = providers;
    emit providersChanged();
    resolveProvider();
}

QVariantMap VpnCore::status() const {
    return m_status;
}

bool VpnCore::connected() const {
    return m_connected;
}

bool VpnCore::connecting() const {
    return m_connecting;
}

bool VpnCore::enabled() const {
    return m_enabled;
}

QString VpnCore::displayName() const {
    return m_config.displayName;
}

void VpnCore::connect() {
    if (m_status.value(QStringLiteral("state")).toString() == QStringLiteral("needs-auth")
        && !m_status.value(QStringLiteral("authUrl")).toString().isEmpty()) {
        // The facade re-toasts the auth URL for this case.
        return;
    }
    if (m_connected || m_connecting || m_config.connectCmd.isEmpty()) {
        return;
    }
    m_connectStdoutBuffer.clear();
    startProcess(m_connectProc, m_config.connectCmd);
}

void VpnCore::disconnect() {
    if (!m_connected || m_connecting || m_config.disconnectCmd.isEmpty()) {
        return;
    }
    startProcess(m_disconnectProc, m_config.disconnectCmd);
}

void VpnCore::toggle() {
    if (m_connected) {
        disconnect();
    } else {
        connect();
    }
}

void VpnCore::checkStatus() {
    if (!m_enabled || m_statusProc->state() != QProcess::NotRunning) {
        return;
    }
    startProcess(m_statusProc, statusCommand());
}

void VpnCore::scheduleStatusCheck() {
    m_statusCheckTimer->start();
}

void VpnCore::resolveProvider() {
    // First enabled provider wins; mirrors the old QML which fell back to a
    // (disabled) "wireguard" pseudo provider when nothing is enabled.
    QVariantMap custom;
    bool enabled = false;
    for (const auto& entry : std::as_const(m_providers)) {
        if (entry.metaType().id() != QMetaType::QVariantMap && !entry.canConvert<QVariantMap>()) {
            continue;
        }
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("enabled")).toBool()) {
            custom = map;
            enabled = true;
            break;
        }
    }

    QString name = QStringLiteral("wireguard");
    QString iface;
    if (enabled) {
        name = custom.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = QStringLiteral("custom");
        }
        iface = custom.value(QStringLiteral("interface")).toString();
    }

    ProviderConfig config = builtinDefaults(name, iface);
    if (enabled) {
        // Key-present wins even when empty: an explicit [] disables the
        // command (old behavior) instead of falling back to the default.
        if (custom.contains(QStringLiteral("connectCmd"))) {
            config.connectCmd = toStringList(custom.value(QStringLiteral("connectCmd")));
        }
        if (custom.contains(QStringLiteral("disconnectCmd"))) {
            config.disconnectCmd = toStringList(custom.value(QStringLiteral("disconnectCmd")));
        }
        const QString displayName = custom.value(QStringLiteral("displayName")).toString();
        if (!displayName.isEmpty()) {
            config.displayName = displayName;
        }
    }

    const bool nameChanged = config.name != m_config.name;
    const bool displayNameChangedNow = config.displayName != m_config.displayName;
    m_config = std::move(config);
    if (displayNameChangedNow) {
        emit displayNameChanged();
    }
    setEnabled(enabled);

    if (nameChanged || !m_hasResolved) {
        m_hasResolved = true;
        // Reset silently (no stateTransition => no toast), then re-check.
        resetStatus();
        m_statusCheckTimer->start();
    }
}

VpnCore::ProviderConfig VpnCore::builtinDefaults(const QString& name, const QString& iface) {
    ProviderConfig config;
    config.name = name;

    if (name == QStringLiteral("wireguard")) {
        config.connectCmd = { QStringLiteral("pkexec"), QStringLiteral("wg-quick"), QStringLiteral("up"), iface };
        config.disconnectCmd = { QStringLiteral("pkexec"), QStringLiteral("wg-quick"), QStringLiteral("down"), iface };
        config.interface = iface;
        config.displayName = iface;
    } else if (name == QStringLiteral("warp")) {
        config.connectCmd = { QStringLiteral("warp-cli"), QStringLiteral("connect") };
        config.disconnectCmd = { QStringLiteral("warp-cli"), QStringLiteral("disconnect") };
        config.interface = QStringLiteral("CloudflareWARP");
        config.displayName = QStringLiteral("Warp");
    } else if (name == QStringLiteral("netbird")) {
        config.connectCmd = { QStringLiteral("netbird"), QStringLiteral("up"), QStringLiteral("--no-browser") };
        config.disconnectCmd = { QStringLiteral("netbird"), QStringLiteral("down") };
        config.interface = QStringLiteral("wt0");
        config.displayName = QStringLiteral("NetBird");
    } else if (name == QStringLiteral("tailscale")) {
        config.connectCmd = { QStringLiteral("tailscale"), QStringLiteral("up") };
        config.disconnectCmd = { QStringLiteral("tailscale"), QStringLiteral("down") };
        config.interface = QStringLiteral("tailscale0");
        config.displayName = QStringLiteral("Tailscale");
    } else {
        config.connectCmd = { name, QStringLiteral("up") };
        config.disconnectCmd = { name, QStringLiteral("down") };
        config.interface = iface.isEmpty() ? name : iface;
        config.displayName = name;
    }

    return config;
}

QStringList VpnCore::statusCommand() const {
    if (m_config.name == QStringLiteral("tailscale")) {
        return { QStringLiteral("tailscale"), QStringLiteral("status"), QStringLiteral("--json") };
    }
    if (m_config.name == QStringLiteral("netbird")) {
        return { QStringLiteral("netbird"), QStringLiteral("status"), QStringLiteral("--json") };
    }
    if (m_config.name == QStringLiteral("warp")) {
        return { QStringLiteral("warp-cli"), QStringLiteral("status") };
    }
    return { QStringLiteral("ip"), QStringLiteral("link"), QStringLiteral("show") };
}

QVariantMap VpnCore::parseStatusOutput(const QString& output) const {
    if (m_config.name == QStringLiteral("tailscale")) {
        return parseTailscaleStatus(output);
    }
    if (m_config.name == QStringLiteral("netbird")) {
        return parseNetBirdStatus(output);
    }
    if (m_config.name == QStringLiteral("warp")) {
        return parseWarpStatus(output);
    }
    return parseWireGuardStatus(output);
}

QVariantMap VpnCore::parseTailscaleStatus(const QString& output) {
    QVariantMap status = makeStatus(false, QStringLiteral("disconnected"));

    if (output.trimmed().isEmpty()) {
        return status;
    }

    // Common non-JSON states first
    if (output.contains(QStringLiteral("Logged out")) || output.contains(QStringLiteral("Stopped"))
        || output.contains(QStringLiteral("not running"))
        || output.contains(QStringLiteral("Tailscale is not running"))) {
        return status;
    }

    QJsonParseError parseError{};
    const auto doc = QJsonDocument::fromJson(output.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (output.contains(QStringLiteral("error")) || output.contains(QStringLiteral("Error"))
            || output.contains(QStringLiteral("failed"))) {
            status[QStringLiteral("reason")] = QStringLiteral("Tailscale may not be running");
        }
        return status;
    }

    const auto data = doc.object();
    const QString backendState = data.value(QStringLiteral("BackendState")).toString();

    if (backendState == QStringLiteral("Running")) {
        status = makeStatus(true, QStringLiteral("connected"));
    } else if (backendState == QStringLiteral("Starting")) {
        status[QStringLiteral("state")] = QStringLiteral("connecting");
    } else if (backendState == QStringLiteral("NeedsLogin") || backendState == QStringLiteral("NeedsMachineAuth")) {
        status = makeStatus(false, QStringLiteral("needs-auth"),
            backendState == QStringLiteral("NeedsLogin") ? QStringLiteral("Login required")
                                                         : QStringLiteral("Machine authorization required"),
            data.value(QStringLiteral("AuthURL")).toString());
    }
    return status;
}

QVariantMap VpnCore::parseNetBirdStatus(const QString& output) {
    QJsonParseError parseError{};
    const auto doc = QJsonDocument::fromJson(output.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return makeStatus(false, QStringLiteral("error"), QStringLiteral("Failed to parse status"));
    }

    QVariantMap status = makeStatus(false, QStringLiteral("disconnected"));
    const auto data = doc.object();
    const auto management = data.value(QStringLiteral("management")).toObject();
    const auto signal = data.value(QStringLiteral("signal")).toObject();

    if (management.value(QStringLiteral("connected")).toBool()
        && signal.value(QStringLiteral("connected")).toBool()) {
        status = makeStatus(true, QStringLiteral("connected"));
    } else {
        const QString error = management.value(QStringLiteral("error")).toString();
        if (!error.isEmpty()) {
            if (error.contains(QStringLiteral("auth")) || error.contains(QStringLiteral("login"))) {
                status = makeStatus(false, QStringLiteral("needs-auth"), QStringLiteral("Authentication required"));
            } else {
                status[QStringLiteral("reason")] = error;
            }
        }
    }
    return status;
}

QVariantMap VpnCore::parseWarpStatus(const QString& output) {
    QVariantMap status = makeStatus(false, QStringLiteral("disconnected"));

    // Word-boundary matching, ORIGINAL precedence (Connected > Connecting >
    // registration > Disconnected): warp prints reasons alongside the state
    // ("Status update: Disconnected. Reason: Registration Missing.") and the
    // registration keywords must win over "Disconnected" or auto-registration
    // never triggers. \bConnected\b cannot match inside "Disconnected".
    static const QRegularExpression connectedRe(QStringLiteral("\\bConnected\\b"));
    static const QRegularExpression connectingRe(QStringLiteral("\\bConnecting\\b"));
    static const QRegularExpression disconnectedRe(QStringLiteral("\\bDisconnected\\b"));

    if (connectedRe.match(output).hasMatch()) {
        return makeStatus(true, QStringLiteral("connected"));
    }
    if (connectingRe.match(output).hasMatch()) {
        status[QStringLiteral("state")] = QStringLiteral("connecting");
        return status;
    }
    if (output.contains(QStringLiteral("Unable")) || output.contains(QStringLiteral("Registration Missing"))
        || output.contains(QStringLiteral("registration")) || output.contains(QStringLiteral("register"))) {
        return makeStatus(false, QStringLiteral("needs-auth"), QStringLiteral("WARP registration required"));
    }
    if (disconnectedRe.match(output).hasMatch()) {
        return status;
    }
    return makeStatus(false, QStringLiteral("error"), QStringLiteral("Unknown WARP status"));
}

QVariantMap VpnCore::parseWireGuardStatus(const QString& output) const {
    if (!m_config.interface.isEmpty() && output.contains(m_config.interface + u':')) {
        return makeStatus(true, QStringLiteral("connected"));
    }
    return makeStatus(false, QStringLiteral("disconnected"));
}

QString VpnCore::extractAuthUrl(const QString& text) {
    static const QRegularExpression urlRe(QStringLiteral("(https?://\\S+)"));
    const auto match = urlRe.match(text);
    return match.hasMatch() ? match.captured(1) : QString();
}

QVariantMap VpnCore::makeStatus(bool connected, const QString& state, const QString& reason, const QString& authUrl) {
    return {
        { QStringLiteral("connected"), connected },
        { QStringLiteral("state"), state },
        { QStringLiteral("reason"), reason },
        { QStringLiteral("authUrl"), authUrl },
    };
}

void VpnCore::updateStatus(QVariantMap newStatus) {
    const QString oldState = m_status.value(QStringLiteral("state")).toString();
    const QString newState = newStatus.value(QStringLiteral("state")).toString();

    // authUrl stickiness: keep the previous URL when a fresh needs-auth
    // status (e.g. from a poll) doesn't carry one.
    if (newState == QStringLiteral("needs-auth") && newStatus.value(QStringLiteral("authUrl")).toString().isEmpty()) {
        const QString previousUrl = m_status.value(QStringLiteral("authUrl")).toString();
        if (!previousUrl.isEmpty()) {
            newStatus[QStringLiteral("authUrl")] = previousUrl;
        }
    }

    m_status = newStatus;
    emit statusChanged();
    setConnected(newStatus.value(QStringLiteral("connected")).toBool());

    if (oldState != newState) {
        emit stateTransition(oldState, m_status);

        // Warp auto-register: only on the transition into needs-auth (the old
        // QML re-spawned this on every poll) and never while it is running.
        if (m_config.name == QStringLiteral("warp") && newState == QStringLiteral("needs-auth")
            && m_status.value(QStringLiteral("reason")).toString().contains(QStringLiteral("registration"))
            && m_warpRegisterProc->state() == QProcess::NotRunning) {
            qCInfo(lcVpn) << "Starting WARP registration";
            startProcess(m_warpRegisterProc, { QStringLiteral("warp-cli"), QStringLiteral("registration"), QStringLiteral("new") });
        }
    }
}

void VpnCore::resetStatus() {
    m_status = makeStatus(false, QStringLiteral("disconnected"));
    emit statusChanged();
    setConnected(false);
}

void VpnCore::setConnected(bool connected) {
    if (connected != m_connected) {
        m_connected = connected;
        emit connectedChanged();
    }
}

void VpnCore::setEnabled(bool enabled) {
    if (enabled != m_enabled) {
        m_enabled = enabled;
        emit enabledChanged();
    }
}

void VpnCore::updateConnecting() {
    const bool connecting =
        m_connectProc->state() != QProcess::NotRunning || m_disconnectProc->state() != QProcess::NotRunning;
    if (connecting != m_connecting) {
        m_connecting = connecting;
        emit connectingChanged();
    }
}

void VpnCore::processConnectStdoutLine(const QString& line) {
    const QString authUrl = extractAuthUrl(line);
    if (!authUrl.isEmpty()) {
        updateStatus(makeStatus(false, QStringLiteral("needs-auth"), QStringLiteral("Authentication required"), authUrl));
    }
}

void VpnCore::handleConnectStderr(const QString& error) {
    if (error.contains(QStringLiteral("Access denied")) || error.contains(QStringLiteral("checkprefs access denied"))) {
        updateStatus(makeStatus(false, QStringLiteral("disconnected"),
            QStringLiteral("Permission denied. Run in terminal: sudo tailscale set --operator=$USER")));
        return;
    }

    if (error.contains(QStringLiteral("Unknown device type")) || error.contains(QStringLiteral("Protocol not supported"))) {
        updateStatus(makeStatus(false, QStringLiteral("disconnected"),
            QStringLiteral("WireGuard module not loaded. Run: sudo modprobe wireguard")));
        return;
    }

    const QString authUrl = extractAuthUrl(error);
    if (!authUrl.isEmpty()) {
        updateStatus(makeStatus(false, QStringLiteral("needs-auth"), QStringLiteral("Authentication required"), authUrl));
    } else if (error.contains(QStringLiteral("already exists"))) {
        // The interface is already up: normalize through the status path so
        // `connected` and `status.connected` cannot diverge (old QML only set
        // the bare `connected` bool here).
        updateStatus(makeStatus(true, QStringLiteral("connected")));
    }
}

void VpnCore::onConnectFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    // Flush any trailing stdout that didn't end in a newline.
    m_connectStdoutBuffer += QString::fromUtf8(m_connectProc->readAllStandardOutput());
    const QStringList lines = m_connectStdoutBuffer.split(u'\n', Qt::SkipEmptyParts);
    m_connectStdoutBuffer.clear();
    for (const QString& line : lines) {
        processConnectStdoutLine(line);
    }

    const QString error = QString::fromUtf8(m_connectProc->readAllStandardError()).trimmed();
    if (!error.isEmpty()) {
        handleConnectStderr(error);
    }

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        return;
    }

    // Queued so any auth-URL stdout/stderr delivered around exit (tailscale)
    // is processed before deciding whether to poll.
    QMetaObject::invokeMethod(
        this,
        [this] {
            if (m_status.value(QStringLiteral("state")).toString() != QStringLiteral("needs-auth")) {
                m_statusCheckTimer->start();
            }
        },
        Qt::QueuedConnection);
}

void VpnCore::onStatusFinished() {
    const QString output = QString::fromUtf8(m_statusProc->readAllStandardOutput());
    const QString error = QString::fromUtf8(m_statusProc->readAllStandardError());

    updateStatus(parseStatusOutput(output));
    handleStatusStderr(error);
}

void VpnCore::handleStatusStderr(const QString& text) {
    if (text.trimmed().isEmpty()) {
        return;
    }

    const bool serviceNotRunning = text.contains(QStringLiteral("doesn't appear to be running"))
        || text.contains(QStringLiteral("failed to connect to local tailscaled"))
        || text.contains(QStringLiteral("daemon is not running"))
        || (text.contains(QStringLiteral("not running"))
            && (text.contains(QStringLiteral("netbird")) || text.contains(QStringLiteral("warp"))));
    if (!serviceNotRunning) {
        return;
    }

    QString unit;
    if (m_config.name == QStringLiteral("tailscale")) {
        unit = QStringLiteral("tailscaled");
    } else if (m_config.name == QStringLiteral("netbird")) {
        unit = QStringLiteral("netbird");
    } else if (m_config.name == QStringLiteral("warp")) {
        unit = QStringLiteral("warp-svc");
    } else {
        unit = m_config.name + u'd';
    }

    updateStatus(makeStatus(false, QStringLiteral("disconnected"),
        QStringLiteral("Service not running (run: sudo systemctl start %1)").arg(unit)));
}

void VpnCore::applyEnvironment(QProcess* process) {
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
    process->setProcessEnvironment(env);
}

void VpnCore::startProcess(QProcess* process, const QStringList& cmd) {
    if (cmd.isEmpty() || cmd.first().isEmpty()) {
        qCWarning(lcVpn) << "Refusing to start empty command for provider" << m_config.name;
        return;
    }
    process->start(cmd.first(), cmd.mid(1));
}

void VpnCore::killProcess(QProcess* process) {
    process->disconnect();
    if (process->state() != QProcess::NotRunning) {
        process->kill();
        process->waitForFinished(100);
    }
}

} // namespace caelestia::services
