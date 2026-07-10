#include "nmcli.hpp"

#include <algorithm>
#include <qfile.h>
#include <qjsengine.h>
#include <qloggingcategory.h>
#include <qregularexpression.h>
#include <qset.h>
#include <utility>

Q_LOGGING_CATEGORY(lcNmcli, "caelestia.services.nmcli", QtInfoMsg)

namespace {

constexpr int maxConnectRetries = 2;
constexpr int maxImmediateChecks = 6;

QVariantMap errorResult(const QString& message) {
    return {
        { QStringLiteral("success"), false },
        { QStringLiteral("output"), QString() },
        { QStringLiteral("error"), message },
        { QStringLiteral("exitCode"), -1 },
    };
}

QVariantMap connectedResult() {
    return {
        { QStringLiteral("success"), true },
        { QStringLiteral("output"), QStringLiteral("Connected") },
        { QStringLiteral("error"), QString() },
        { QStringLiteral("exitCode"), 0 },
    };
}

} // namespace

namespace caelestia::services {

NmcliCallback::NmcliCallback(NmcliCore* core, const QJSValue& function)
    : m_core(core)
    , m_function(function)
    , m_called(false) {}

void NmcliCallback::invoke(const QVariantMap& result) {
    if (m_called) {
        return;
    }
    m_called = true;

    if (!m_function.isCallable()) {
        return;
    }

    auto* engine = qjsEngine(m_core);
    if (!engine) {
        qCWarning(lcNmcli) << "Unable to invoke callback: no JS engine";
        return;
    }

    const QVariantMap mapped = transform ? transform(result) : result;
    m_function.call({ engine->toScriptValue(mapped) });
}

void NmcliCallback::invokeValue(const QVariant& value) {
    if (m_called) {
        return;
    }
    m_called = true;

    if (!m_function.isCallable()) {
        return;
    }

    auto* engine = qjsEngine(m_core);
    if (!engine) {
        qCWarning(lcNmcli) << "Unable to invoke callback: no JS engine";
        return;
    }

    m_function.call({ engine->toScriptValue(value) });
}

QVariantMap NmcliResult::toMap() const {
    return {
        { QStringLiteral("success"), success },
        { QStringLiteral("output"), output },
        { QStringLiteral("error"), error },
        { QStringLiteral("exitCode"), exitCode },
        { QStringLiteral("needsPassword"), needsPassword },
    };
}

NmcliCore::NmcliCore(QObject* parent)
    : QObject(parent)
    , m_monitorProcess(new QProcess(this))
    , m_rescanProcess(new QProcess(this))
    , m_monitorRestartTimer(new QTimer(this))
    , m_refreshDebounce(new QTimer(this))
    , m_connectionCheckTimer(new QTimer(this))
    , m_immediateCheckTimer(new QTimer(this)) {
    applyEnvironment(m_monitorProcess);
    m_monitorProcess->setStandardErrorFile(QProcess::nullDevice());
    QObject::connect(m_monitorProcess, &QProcess::readyReadStandardOutput, this, [this] {
        m_monitorProcess->readAllStandardOutput();
        m_refreshDebounce->start();
    });
    QObject::connect(m_monitorProcess, &QProcess::finished, this, [this] { m_monitorRestartTimer->start(); });
    // finished() does not fire on FailedToStart; without this the monitor
    // would stay dead for the whole session if nmcli failed to spawn once.
    QObject::connect(m_monitorProcess, &QProcess::errorOccurred, this, [this] { m_monitorRestartTimer->start(); });

    m_monitorRestartTimer->setSingleShot(true);
    m_monitorRestartTimer->setInterval(2000);
    QObject::connect(m_monitorRestartTimer, &QTimer::timeout, this, [this] {
        if (m_monitorProcess->state() == QProcess::NotRunning) {
            m_monitorProcess->start(QStringLiteral("nmcli"), { QStringLiteral("monitor") });
        }
    });

    // Coalesce monitor output bursts into a single refresh
    m_refreshDebounce->setSingleShot(true);
    m_refreshDebounce->setInterval(300);
    QObject::connect(m_refreshDebounce, &QTimer::timeout, this, &NmcliCore::refreshOnConnectionChange);

    applyEnvironment(m_rescanProcess);
    m_rescanProcess->setStandardOutputFile(QProcess::nullDevice());
    m_rescanProcess->setStandardErrorFile(QProcess::nullDevice());
    QObject::connect(m_rescanProcess, &QProcess::finished, this, [this] {
        emit scanningChanged();
        getNetworks();
    });
    // finished() does not fire on FailedToStart; scanning would stick true.
    QObject::connect(m_rescanProcess, &QProcess::errorOccurred, this, [this] { emit scanningChanged(); });

    m_connectionCheckTimer->setSingleShot(true);
    m_connectionCheckTimer->setInterval(4000);
    QObject::connect(m_connectionCheckTimer, &QTimer::timeout, this, &NmcliCore::onConnectionCheckTimeout);

    m_immediateCheckTimer->setInterval(500);
    QObject::connect(m_immediateCheckTimer, &QTimer::timeout, this, &NmcliCore::onImmediateCheckTick);

    m_monitorProcess->start(QStringLiteral("nmcli"), { QStringLiteral("monitor") });

    getWifiStatus();
    getNetworks();
    loadSavedConnections();
    fetchEthernetInterfaces();

    QTimer::singleShot(2000, this, [this] {
        for (const auto& iface : std::as_const(m_ethernetInterfaces)) {
            if (isConnectedState(iface.state) && !iface.device.isEmpty()) {
                fetchEthernetDeviceDetails(iface.device);
                break;
            }
        }
    });
}

NmcliCore::~NmcliCore() {
    for (auto* handle : std::as_const(m_activeProcesses)) {
        handle->process->disconnect(this);
        if (handle->process->state() != QProcess::NotRunning) {
            handle->process->kill();
            handle->process->waitForFinished(100);
        }
        delete handle;
    }
    m_activeProcesses.clear();

    m_monitorProcess->disconnect(this);
    if (m_monitorProcess->state() != QProcess::NotRunning) {
        m_monitorProcess->kill();
        m_monitorProcess->waitForFinished(100);
    }

    m_rescanProcess->disconnect(this);
    if (m_rescanProcess->state() != QProcess::NotRunning) {
        m_rescanProcess->kill();
        m_rescanProcess->waitForFinished(100);
    }
}

QList<NmcliAccessPoint*> NmcliCore::networks() const {
    return m_networks;
}

QList<NmcliEthernetDevice*> NmcliCore::ethernetDevices() const {
    return m_ethernetDevices;
}

NmcliAccessPoint* NmcliCore::active() const {
    return m_active;
}

NmcliEthernetDevice* NmcliCore::activeEthernet() const {
    return m_activeEthernet;
}

bool NmcliCore::wifiEnabled() const {
    return m_wifiEnabled;
}

bool NmcliCore::scanning() const {
    return m_rescanProcess->state() != QProcess::NotRunning;
}

bool NmcliCore::hasAvailableEthernet() const {
    return m_hasAvailableEthernet;
}

QString NmcliCore::connectingSsid() const {
    return m_connectingSsid;
}

QString NmcliCore::ethernetSpeed() const {
    return m_ethernetSpeed;
}

QVariant NmcliCore::ethernetDeviceDetails() const {
    return m_ethernetDeviceDetails;
}

QString NmcliCore::ethernetDataUsage() const {
    return m_ethernetDataUsage;
}

QVariant NmcliCore::pendingConnection() const {
    if (!m_pendingConnection) {
        // Explicit null: an invalid QVariant surfaces as undefined in QML and
        // breaks strict === null checks in consumers.
        return QVariant::fromValue(nullptr);
    }
    return QVariantMap{
        { QStringLiteral("ssid"), m_pendingConnection->ssid },
        { QStringLiteral("bssid"), m_pendingConnection->bssid },
        { QStringLiteral("retryCount"), m_pendingConnection->retryCount },
    };
}

void NmcliCore::rescanWifi() {
    if (m_rescanProcess->state() != QProcess::NotRunning) {
        return;
    }
    m_rescanProcess->start(QStringLiteral("nmcli"),
        { QStringLiteral("dev"), QStringLiteral("wifi"), QStringLiteral("list"), QStringLiteral("--rescan"),
            QStringLiteral("yes") });
    emit scanningChanged();
}

void NmcliCore::connectToNetworkWithPasswordCheck(
    const QString& ssid, bool isSecure, QJSValue callback, const QString& bssid) {
    auto cb = wrap(callback);
    if (isSecure && cb) {
        cb->transform = [](QVariantMap result) {
            if (result.value(QStringLiteral("success")).toBool()) {
                return QVariantMap{
                    { QStringLiteral("success"), true },
                    { QStringLiteral("usedSavedPassword"), true },
                    { QStringLiteral("output"), result.value(QStringLiteral("output")) },
                    { QStringLiteral("error"), QString() },
                    { QStringLiteral("exitCode"), 0 },
                };
            }
            if (result.value(QStringLiteral("needsPassword")).toBool()) {
                return QVariantMap{
                    { QStringLiteral("success"), false },
                    { QStringLiteral("needsPassword"), true },
                    { QStringLiteral("output"), result.value(QStringLiteral("output")) },
                    { QStringLiteral("error"), result.value(QStringLiteral("error")) },
                    { QStringLiteral("exitCode"), result.value(QStringLiteral("exitCode")) },
                };
            }
            return result;
        };
    }
    connectWireless(ssid, QString(), bssid, cb, 0);
}

void NmcliCore::connectToNetwork(const QString& ssid, const QString& password, const QString& bssid, QJSValue callback) {
    connectWireless(ssid, password, bssid, wrap(callback), 0);
}

void NmcliCore::connectWireless(
    const QString& ssid, const QString& password, const QString& bssid, const NmcliCallbackPtr& callback, int retryCount) {
    const bool hasBssid = !bssid.isEmpty();

    if (callback) {
        setPendingConnection(PendingConnection{ ssid, hasBssid ? bssid : QString(), callback, retryCount });
        m_connectionCheckTimer->start();
        m_immediateCheckCount = 0;
        m_immediateCheckTimer->start();
    }

    if (!password.isEmpty() && hasBssid) {
        createConnectionWithPassword(ssid, bssid.toUpper(), password, callback);
        return;
    }

    QStringList cmd{ QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("connect"), ssid };
    if (!password.isEmpty()) {
        cmd << QStringLiteral("password") << password;
    }

    execute(
        cmd,
        [this, ssid, password, bssid, callback, retryCount](const NmcliResult& result) {
            if (result.needsPassword && callback) {
                callback->invoke(result.toMap());
                return;
            }

            if (!result.success && m_pendingConnection && retryCount < maxConnectRetries) {
                qCWarning(lcNmcli) << "Connection failed, retrying... (attempt" << retryCount + 1 << "/"
                                   << maxConnectRetries << ")";
                QTimer::singleShot(1000, this, [this, ssid, password, bssid, callback, retryCount] {
                    connectWireless(ssid, password, bssid, callback, retryCount + 1);
                });
            } else if (!result.success && !m_pendingConnection && callback) {
                callback->invoke(result.toMap());
            }
            // Success (and failure with a pending connection) is resolved by the
            // pending-connection state machine once the SSID shows up as active.
        },
        callback);
}

void NmcliCore::createConnectionWithPassword(
    const QString& ssid, const QString& bssidUpper, const QString& password, const NmcliCallbackPtr& callback) {
    checkAndDeleteConnection(ssid, [this, ssid, bssidUpper, password, callback] {
        const QStringList cmd{ QStringLiteral("connection"), QStringLiteral("add"), QStringLiteral("type"),
            QStringLiteral("wifi"), QStringLiteral("con-name"), ssid, QStringLiteral("ifname"), QStringLiteral("*"),
            QStringLiteral("ssid"), ssid, QStringLiteral("802-11-wireless.bssid"), bssidUpper,
            QStringLiteral("802-11-wireless-security.key-mgmt"), QStringLiteral("wpa-psk"),
            QStringLiteral("802-11-wireless-security.psk"), password };

        execute(
            cmd,
            [this, ssid, password, callback](const NmcliResult& result) {
                if (result.success) {
                    loadSavedConnections();
                    activateConnection(ssid, callback);
                    return;
                }

                const bool hasDuplicateWarning = result.error.contains(QStringLiteral("another connection with the name"))
                    || result.error.contains(QStringLiteral("Reference the connection by its uuid"));

                if (hasDuplicateWarning || (result.exitCode > 0 && result.exitCode < 10)) {
                    loadSavedConnections();
                    activateConnection(ssid, callback);
                } else {
                    qCWarning(lcNmcli) << "Connection profile creation failed, trying fallback...";
                    const QStringList fallbackCmd{ QStringLiteral("device"), QStringLiteral("wifi"),
                        QStringLiteral("connect"), ssid, QStringLiteral("password"), password };
                    execute(
                        fallbackCmd,
                        [callback](const NmcliResult& fallbackResult) {
                            if (callback) {
                                callback->invoke(fallbackResult.toMap());
                            }
                        },
                        callback);
                }
            },
            callback);
    });
}

void NmcliCore::checkAndDeleteConnection(const QString& ssid, std::function<void()> done) {
    execute({ QStringLiteral("connection"), QStringLiteral("show"), ssid },
        [this, ssid, done = std::move(done)](const NmcliResult& result) {
            if (result.success) {
                execute({ QStringLiteral("connection"), QStringLiteral("delete"), ssid },
                    [this, done](const NmcliResult&) {
                        QTimer::singleShot(300, this, [done] {
                            if (done) {
                                done();
                            }
                        });
                    });
            } else if (done) {
                done();
            }
        });
}

void NmcliCore::activateConnection(const QString& connectionName, const NmcliCallbackPtr& callback) {
    execute(
        { QStringLiteral("connection"), QStringLiteral("up"), connectionName },
        [callback](const NmcliResult& result) {
            if (callback) {
                callback->invoke(result.toMap());
            }
        },
        callback);
}

void NmcliCore::disconnectFromNetwork() {
    if (m_active && !m_active->ssid().isEmpty()) {
        execute({ QStringLiteral("connection"), QStringLiteral("down"), m_active->ssid() },
            [this](const NmcliResult& result) {
                if (result.success) {
                    getNetworks();
                }
            });
    } else {
        execute({ QStringLiteral("device"), QStringLiteral("disconnect"), QStringLiteral("wifi") },
            [this](const NmcliResult& result) {
                if (result.success) {
                    getNetworks();
                }
            });
    }
}

void NmcliCore::forgetNetwork(const QString& ssid, QJSValue callback) {
    auto cb = wrap(callback);

    if (ssid.isEmpty()) {
        if (cb) {
            cb->invoke(errorResult(QStringLiteral("No SSID specified")));
        }
        return;
    }

    const QString needle = ssid.toLower().trimmed();
    QString connectionName = ssid;
    for (const auto& conn : std::as_const(m_savedConnections)) {
        if (conn.toLower().trimmed() == needle) {
            connectionName = conn;
            break;
        }
    }

    execute(
        { QStringLiteral("connection"), QStringLiteral("delete"), connectionName },
        [this, cb](const NmcliResult& result) {
            if (result.success) {
                QTimer::singleShot(500, this, [this] { loadSavedConnections(); });
            }
            if (cb) {
                cb->invoke(result.toMap());
            }
        },
        cb);
}

bool NmcliCore::hasSavedProfile(const QString& ssid) const {
    if (ssid.isEmpty()) {
        return false;
    }
    const QString needle = ssid.toLower().trimmed();

    if (m_active && m_active->ssid().toLower().trimmed() == needle) {
        return true;
    }

    for (const auto& saved : m_savedConnectionSsids) {
        if (saved.toLower().trimmed() == needle) {
            return true;
        }
    }

    for (const auto& conn : m_savedConnections) {
        if (conn.toLower().trimmed() == needle) {
            return true;
        }
    }

    return false;
}

void NmcliCore::enableWifi(bool enabled, QJSValue callback) {
    auto cb = wrap(callback);
    execute(
        { QStringLiteral("radio"), QStringLiteral("wifi"), enabled ? QStringLiteral("on") : QStringLiteral("off") },
        [this, cb](const NmcliResult& result) {
            if (result.success) {
                getWifiStatus([cb, result](bool) {
                    if (cb) {
                        cb->invoke(result.toMap());
                    }
                });
            } else if (cb) {
                cb->invoke(result.toMap());
            }
        },
        cb);
}

void NmcliCore::toggleWifi(QJSValue callback) {
    enableWifi(!m_wifiEnabled, callback);
}

void NmcliCore::connectEthernet(const QString& connectionName, const QString& interfaceName, QJSValue callback) {
    auto cb = wrap(callback);

    if (!connectionName.isEmpty()) {
        execute(
            { QStringLiteral("connection"), QStringLiteral("up"), connectionName },
            [this, interfaceName, cb](const NmcliResult& result) {
                if (result.success) {
                    QTimer::singleShot(500, this, [this, interfaceName] {
                        fetchEthernetInterfaces();
                        if (!interfaceName.isEmpty()) {
                            QTimer::singleShot(
                                1000, this, [this, interfaceName] { fetchEthernetDeviceDetails(interfaceName); });
                        }
                    });
                }
                if (cb) {
                    cb->invoke(result.toMap());
                }
            },
            cb);
    } else if (!interfaceName.isEmpty()) {
        execute(
            { QStringLiteral("device"), QStringLiteral("connect"), interfaceName },
            [this, interfaceName, cb](const NmcliResult& result) {
                if (result.success) {
                    QTimer::singleShot(500, this, [this, interfaceName] {
                        fetchEthernetInterfaces();
                        QTimer::singleShot(
                            1000, this, [this, interfaceName] { fetchEthernetDeviceDetails(interfaceName); });
                    });
                }
                if (cb) {
                    cb->invoke(result.toMap());
                }
            },
            cb);
    } else if (cb) {
        cb->invoke(errorResult(QStringLiteral("No connection name or interface specified")));
    }
}

void NmcliCore::disconnectEthernet(const QString& connectionName, QJSValue callback) {
    auto cb = wrap(callback);

    if (connectionName.isEmpty()) {
        if (cb) {
            cb->invoke(errorResult(QStringLiteral("No connection name specified")));
        }
        return;
    }

    execute(
        { QStringLiteral("connection"), QStringLiteral("down"), connectionName },
        [this, cb](const NmcliResult& result) {
            if (result.success) {
                setEthernetDeviceDetails(QVariant());
                QTimer::singleShot(500, this, [this] { fetchEthernetInterfaces(); });
            }
            if (cb) {
                cb->invoke(result.toMap());
            }
        },
        cb);
}

void NmcliCore::getEthernetInterfaces(QJSValue callback) {
    auto cb = wrap(callback);
    fetchEthernetInterfaces([cb](const QVariantList& interfaces) {
        if (cb) {
            cb->invokeValue(interfaces);
        }
    });
}

void NmcliCore::getEthernetDeviceDetails(const QString& interfaceName, QJSValue callback) {
    auto cb = wrap(callback);
    fetchEthernetDeviceDetails(interfaceName, [cb](const QVariant& details) {
        if (cb) {
            cb->invokeValue(details);
        }
    });
}

void NmcliCore::getEthernetSpeed(const QString& interfaceName) {
    if (interfaceName.isEmpty()) {
        setEthernetSpeed(QString());
        return;
    }

    QFile f(QStringLiteral("/sys/class/net/%1/speed").arg(interfaceName));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setEthernetSpeed(QString());
        return;
    }
    const QString text = QString::fromLatin1(f.readAll()).trimmed();
    f.close();

    bool ok = false;
    const int mbit = text.toInt(&ok);
    // Disconnected/virtual interfaces report -1 or nothing.
    if (!ok || mbit <= 0) {
        setEthernetSpeed(QString());
    } else if (mbit >= 1000) {
        const qreal gbps = mbit / 1000.0;
        const QString num =
            mbit % 1000 == 0 ? QString::number(qRound(gbps)) : QString::number(gbps, 'f', 1);
        setEthernetSpeed(num + QStringLiteral(" Gbps"));
    } else {
        setEthernetSpeed(QString::number(mbit) + QStringLiteral(" Mbps"));
    }
}

void NmcliCore::getEthernetDataUsage(const QString& interfaceName, QJSValue callback) {
    auto cb = wrap(callback);

    if (interfaceName.isEmpty()) {
        if (cb) {
            cb->invokeValue(QString());
        }
        return;
    }

    const auto readBytes = [](const QString& path, bool& ok) -> qint64 {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            ok = false;
            return 0;
        }
        return QString::fromLatin1(f.readAll()).trimmed().toLongLong(&ok);
    };

    bool rxOk = true;
    bool txOk = true;
    const qint64 rx = readBytes(QStringLiteral("/sys/class/net/%1/statistics/rx_bytes").arg(interfaceName), rxOk);
    const qint64 tx = readBytes(QStringLiteral("/sys/class/net/%1/statistics/tx_bytes").arg(interfaceName), txOk);

    if (!rxOk || !txOk) {
        if (cb) {
            cb->invokeValue(QString());
        }
        return;
    }

    const QString human = formatBytes(rx + tx);
    setEthernetDataUsage(human);
    if (cb) {
        cb->invokeValue(human);
    }
}

void NmcliCore::getIpv4Config(const QString& connectionName, QJSValue callback) {
    auto cb = wrap(callback);

    if (connectionName.isEmpty()) {
        if (cb) {
            cb->invokeValue(QVariant());
        }
        return;
    }

    execute(
        { QStringLiteral("-t"), QStringLiteral("-f"),
            QStringLiteral("ipv4.method,ipv4.addresses,ipv4.gateway,ipv4.dns,ipv4.ignore-auto-dns,connection.autoconnect"),
            QStringLiteral("connection"), QStringLiteral("show"), connectionName },
        [cb](const NmcliResult& result) {
            if (!cb) {
                return;
            }
            if (!result.success) {
                cb->invokeValue(QVariant());
                return;
            }

            QVariantMap cfg{
                { QStringLiteral("method"), QStringLiteral("auto") },
                { QStringLiteral("address"), QString() },
                { QStringLiteral("gateway"), QString() },
                { QStringLiteral("dns"), QString() },
                { QStringLiteral("ignoreAutoDns"), false },
                { QStringLiteral("autoconnect"), true },
            };

            const QStringList lines = result.output.trimmed().split(u'\n', Qt::SkipEmptyParts);
            for (const QString& line : lines) {
                const QStringList fields = splitTerseLine(line);
                if (fields.size() < 2) {
                    continue;
                }
                const QString key = fields.at(0).trimmed();
                QString value = fields.mid(1).join(u':').trimmed();

                if (key == QStringLiteral("ipv4.ignore-auto-dns")) {
                    cfg[QStringLiteral("ignoreAutoDns")] = value == QStringLiteral("yes");
                } else if (key == QStringLiteral("connection.autoconnect")) {
                    cfg[QStringLiteral("autoconnect")] = value != QStringLiteral("no");
                }

                if (value.isEmpty() || value == QStringLiteral("--")) {
                    continue;
                }

                if (key == QStringLiteral("ipv4.method")) {
                    cfg[QStringLiteral("method")] = value;
                } else if (key == QStringLiteral("ipv4.addresses")) {
                    cfg[QStringLiteral("address")] = value.split(u',').first().trimmed();
                } else if (key == QStringLiteral("ipv4.gateway")) {
                    cfg[QStringLiteral("gateway")] = value;
                } else if (key == QStringLiteral("ipv4.dns")) {
                    static const QRegularExpression trailing(QStringLiteral(";\\s*$"));
                    static const QRegularExpression separators(QStringLiteral("[;,]"));
                    value.remove(trailing);
                    QStringList servers;
                    const QStringList parts = value.split(separators);
                    for (const QString& part : parts) {
                        const QString trimmed = part.trimmed();
                        if (!trimmed.isEmpty()) {
                            servers << trimmed;
                        }
                    }
                    cfg[QStringLiteral("dns")] = servers.join(QStringLiteral(", "));
                }
            }

            // Distinguish "automatic + custom DNS only" from plain DHCP.
            if (cfg.value(QStringLiteral("method")) == QStringLiteral("auto")
                && cfg.value(QStringLiteral("ignoreAutoDns")).toBool()) {
                cfg[QStringLiteral("method")] = QStringLiteral("auto-dns");
            }

            cb->invokeValue(cfg);
        });
}

void NmcliCore::setIpv4Config(const QString& connectionName, const QVariantMap& config, QJSValue callback) {
    auto cb = wrap(callback);

    if (connectionName.isEmpty()) {
        if (cb) {
            cb->invoke(errorResult(QStringLiteral("No connection specified")));
        }
        return;
    }

    QStringList servers;
    const QStringList dnsParts = config.value(QStringLiteral("dns")).toString().split(u',');
    for (const QString& part : dnsParts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            servers << trimmed;
        }
    }
    const QString dnsList = servers.join(u' ');

    QStringList cmd{ QStringLiteral("connection"), QStringLiteral("modify"), connectionName };
    const QString method = config.value(QStringLiteral("method")).toString();

    if (method == QStringLiteral("manual")) {
        cmd << QStringLiteral("ipv4.method") << QStringLiteral("manual");
        cmd << QStringLiteral("ipv4.addresses") << config.value(QStringLiteral("address")).toString();
        cmd << QStringLiteral("ipv4.gateway") << config.value(QStringLiteral("gateway")).toString();
        cmd << QStringLiteral("ipv4.dns") << dnsList;
        cmd << QStringLiteral("ipv4.ignore-auto-dns") << QStringLiteral("yes");
    } else if (method == QStringLiteral("auto-dns")) {
        // DHCP addressing, custom DNS only.
        cmd << QStringLiteral("ipv4.method") << QStringLiteral("auto");
        cmd << QStringLiteral("ipv4.addresses") << QString();
        cmd << QStringLiteral("ipv4.gateway") << QString();
        cmd << QStringLiteral("ipv4.dns") << dnsList;
        cmd << QStringLiteral("ipv4.ignore-auto-dns") << QStringLiteral("yes");
    } else {
        // Full DHCP: clear manual fields and re-enable auto DNS.
        cmd << QStringLiteral("ipv4.method") << QStringLiteral("auto");
        cmd << QStringLiteral("ipv4.addresses") << QString();
        cmd << QStringLiteral("ipv4.gateway") << QString();
        cmd << QStringLiteral("ipv4.dns") << QString();
        cmd << QStringLiteral("ipv4.ignore-auto-dns") << QStringLiteral("no");
    }

    execute(
        cmd,
        [this, connectionName, cb](const NmcliResult& result) {
            if (!result.success) {
                if (cb) {
                    cb->invoke(result.toMap());
                }
                return;
            }
            // Reactivate so changes take effect immediately.
            execute(
                { QStringLiteral("connection"), QStringLiteral("up"), connectionName },
                [this, cb](const NmcliResult& upResult) {
                    refreshOnConnectionChange();
                    if (cb) {
                        cb->invoke(upResult.toMap());
                    }
                },
                cb);
        },
        cb);
}

void NmcliCore::execute(const QStringList& args, Handler handler, const NmcliCallbackPtr& jsCallback) {
    auto* handle = new ProcHandle;
    handle->args = args;
    handle->jsCallback = jsCallback;
    handle->handler = std::move(handler);

    auto* process = new QProcess(this);
    handle->process = process;
    applyEnvironment(process);

    QObject::connect(process, &QProcess::readyReadStandardOutput, this,
        [handle] { handle->stdoutText += QString::fromUtf8(handle->process->readAllStandardOutput()); });
    QObject::connect(process, &QProcess::readyReadStandardError, this, [this, handle] {
        handle->stderrText += QString::fromUtf8(handle->process->readAllStandardError());
        const QString error = handle->stderrText.trimmed();
        if (!error.isEmpty()) {
            handlePasswordRequired(handle, error, handle->stdoutText, -1);
        }
    });
    QObject::connect(process, &QProcess::finished, this,
        [this, handle](int exitCode, QProcess::ExitStatus status) { finishProcess(handle, exitCode, status); });
    QObject::connect(process, &QProcess::errorOccurred, this, [this, handle](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCWarning(lcNmcli) << "Failed to start nmcli" << handle->args;
            finishProcess(handle, -1, QProcess::CrashExit);
        }
    });

    m_activeProcesses.append(handle);
    process->start(QStringLiteral("nmcli"), args);
}

void NmcliCore::finishProcess(ProcHandle* handle, int exitCode, QProcess::ExitStatus status) {
    if (handle->done) {
        return;
    }
    handle->done = true;

    handle->stdoutText += QString::fromUtf8(handle->process->readAllStandardOutput());
    handle->stderrText += QString::fromUtf8(handle->process->readAllStandardError());

    NmcliResult result;
    result.exitCode = exitCode;
    result.success = status == QProcess::NormalExit && exitCode == 0;
    result.output = handle->stdoutText;
    result.error = handle->stderrText;

    if (!handle->callbackCalled) {
        const QString error = result.error.trimmed();
        if (!handlePasswordRequired(handle, error, result.output, exitCode)) {
            const bool cmdIsConnection = isConnectionCommand(handle->args);
            result.needsPassword = cmdIsConnection && detectPasswordRequired(error);

            if (!result.success && cmdIsConnection && m_pendingConnection) {
                emit connectionFailed(m_pendingConnection->ssid);
            }

            handle->callbackCalled = true;
            if (handle->handler) {
                handle->handler(result);
            }
        }
    }

    m_activeProcesses.removeOne(handle);
    handle->process->deleteLater();
    delete handle;
}

NmcliCallbackPtr NmcliCore::wrap(const QJSValue& callback) {
    if (!callback.isCallable()) {
        return nullptr;
    }
    return std::make_shared<NmcliCallback>(this, callback);
}

QStringList NmcliCore::splitTerseLine(const QString& line) {
    QStringList fields;
    QString current;
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == u'\\' && i + 1 < line.size()) {
            current += line.at(++i);
        } else if (c == u':') {
            fields << current;
            current.clear();
        } else {
            current += c;
        }
    }
    fields << current;
    return fields;
}

int NmcliCore::parseLeadingInt(const QString& s) {
    const QString trimmed = s.trimmed();
    qsizetype end = 0;
    if (end < trimmed.size() && (trimmed.at(end) == u'-' || trimmed.at(end) == u'+')) {
        ++end;
    }
    while (end < trimmed.size() && trimmed.at(end).isDigit()) {
        ++end;
    }
    return trimmed.left(end).toInt();
}

bool NmcliCore::isConnectionCommand(const QStringList& args) {
    return args.contains(QStringLiteral("wifi")) || args.contains(QStringLiteral("connection"));
}

bool NmcliCore::isConnectedState(const QString& state) {
    return state == QStringLiteral("100 (connected)") || state.startsWith(QStringLiteral("connected"));
}

bool NmcliCore::isConnectingState(const QString& state) {
    return state.startsWith(QStringLiteral("connecting"));
}

bool NmcliCore::detectPasswordRequired(const QString& error) {
    if (error.isEmpty()) {
        return false;
    }

    const bool activated =
        error.contains(QStringLiteral("Connection activated")) || error.contains(QStringLiteral("successfully"));

    const bool matches = error.contains(QStringLiteral("Secrets were required"))
        || error.contains(QStringLiteral("No secrets provided"))
        || error.contains(QStringLiteral("802-11-wireless-security.psk"))
        || error.contains(QStringLiteral("password for")) || (error.contains(QStringLiteral("password")) && !activated)
        || (error.contains(QStringLiteral("Secrets")) && !activated)
        || (error.contains(QStringLiteral("802.11")) && !activated);

    return matches && !activated;
}

QString NmcliCore::cidrToSubnetMask(const QString& cidr) {
    bool ok = false;
    const int bits = cidr.toInt(&ok);
    if (!ok || bits < 0 || bits > 32) {
        return QString();
    }

    const quint32 mask = bits == 0 ? 0 : 0xffffffffu << (32 - bits);
    return QStringLiteral("%1.%2.%3.%4")
        .arg((mask >> 24) & 0xff)
        .arg((mask >> 16) & 0xff)
        .arg((mask >> 8) & 0xff)
        .arg(mask & 0xff);
}

QString NmcliCore::formatBytes(qint64 bytes) {
    if (bytes <= 0) {
        return QStringLiteral("0 B");
    }

    static const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    qreal value = static_cast<qreal>(bytes);
    int i = 0;
    while (value >= 1024 && i < 4) {
        value /= 1024;
        ++i;
    }

    const int decimals = value < 10 && i > 0 ? 1 : 0;
    return QString::number(value, 'f', decimals) + u' ' + QLatin1String(units[i]);
}

QList<NmcliCore::DeviceStatus> NmcliCore::parseDeviceStatus(const QString& output, const QString& filterType) {
    QList<DeviceStatus> interfaces;
    const QStringList lines = output.trimmed().split(u'\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        const QStringList fields = splitTerseLine(line);
        if (fields.size() < 2) {
            continue;
        }

        const QString& type = fields.at(1);
        const bool include = type == filterType
            || (filterType == QStringLiteral("both")
                && (type == QStringLiteral("wifi") || type == QStringLiteral("ethernet")));
        if (!include) {
            continue;
        }

        interfaces.append({ fields.value(0), fields.value(1), fields.value(2), fields.value(3) });
    }

    return interfaces;
}

QVariantMap NmcliCore::parseDeviceDetails(const QString& output) {
    QVariantMap details{
        { QStringLiteral("ipAddress"), QString() },
        { QStringLiteral("gateway"), QString() },
        { QStringLiteral("dns"), QStringList() },
        { QStringLiteral("subnet"), QString() },
        { QStringLiteral("macAddress"), QString() },
        { QStringLiteral("speed"), QString() },
    };

    if (output.isEmpty()) {
        return details;
    }

    QStringList dns;
    const QStringList lines = output.trimmed().split(u'\n');

    for (const QString& line : lines) {
        const qsizetype idx = line.indexOf(u':');
        if (idx < 0) {
            continue;
        }
        const QString key = line.left(idx).trimmed();
        const QString value = line.mid(idx + 1).trimmed();

        if (key.startsWith(QStringLiteral("IP4.ADDRESS"))) {
            const QStringList ipParts = value.split(u'/');
            details[QStringLiteral("ipAddress")] = ipParts.value(0);
            details[QStringLiteral("subnet")] =
                ipParts.size() > 1 ? cidrToSubnetMask(ipParts.at(1)) : QString();
        } else if (key == QStringLiteral("IP4.GATEWAY")) {
            if (value != QStringLiteral("--")) {
                details[QStringLiteral("gateway")] = value;
            }
        } else if (key.startsWith(QStringLiteral("IP4.DNS"))) {
            if (value != QStringLiteral("--") && !value.isEmpty()) {
                dns << value;
            }
        } else if (key == QStringLiteral("GENERAL.HWADDR")) {
            details[QStringLiteral("macAddress")] = value;
        }
    }

    details[QStringLiteral("dns")] = dns;
    return details;
}

QList<NmcliAccessPoint::Data> NmcliCore::parseNetworkOutput(const QString& output) {
    QList<NmcliAccessPoint::Data> networks;
    const QStringList lines = output.trimmed().split(u'\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        const QStringList fields = splitTerseLine(line);

        NmcliAccessPoint::Data data;
        data.active = fields.value(0) == QStringLiteral("yes");
        data.strength = parseLeadingInt(fields.value(1));
        data.frequency = parseLeadingInt(fields.value(2));
        data.ssid = fields.value(3).trimmed();
        data.bssid = fields.value(4).trimmed();
        data.security = fields.value(5).trimmed();

        if (!data.ssid.isEmpty()) {
            networks.append(data);
        }
    }

    return networks;
}

QList<NmcliAccessPoint::Data> NmcliCore::deduplicateNetworks(const QList<NmcliAccessPoint::Data>& networks) {
    QList<NmcliAccessPoint::Data> deduped;
    QHash<QString, qsizetype> indices;

    for (const auto& network : networks) {
        const auto it = indices.constFind(network.ssid);
        if (it == indices.constEnd()) {
            indices.insert(network.ssid, deduped.size());
            deduped.append(network);
            continue;
        }

        auto& existing = deduped[it.value()];
        if (network.active && !existing.active) {
            existing = network;
        } else if (!network.active && !existing.active && network.strength > existing.strength) {
            existing = network;
        }
    }

    return deduped;
}

void NmcliCore::getWifiStatus(std::function<void(bool)> done) {
    execute({ QStringLiteral("radio"), QStringLiteral("wifi") }, [this, done = std::move(done)](const NmcliResult& result) {
        if (result.success) {
            setWifiEnabled(result.output.trimmed() == QStringLiteral("enabled"));
        }
        if (done) {
            done(m_wifiEnabled);
        }
    });
}

void NmcliCore::getNetworks(std::function<void()> done) {
    execute({ QStringLiteral("-g"), QStringLiteral("ACTIVE,SIGNAL,FREQ,SSID,BSSID,SECURITY"), QStringLiteral("d"),
                QStringLiteral("w") },
        [this, done = std::move(done)](const NmcliResult& result) {
            if (!result.success) {
                if (done) {
                    done();
                }
                return;
            }

            reconcileNetworks(deduplicateNetworks(parseNetworkOutput(result.output)));

            if (done) {
                done();
            }
            checkPendingConnection();
        });
}

void NmcliCore::getWirelessInterfaces() {
    execute({ QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("DEVICE,TYPE,STATE,CONNECTION"),
                QStringLiteral("device"), QStringLiteral("status") },
        [this](const NmcliResult& result) {
            m_wirelessInterfaces = parseDeviceStatus(result.output, QStringLiteral("wifi"));
            updateConnectingSsid();
        });
}

void NmcliCore::fetchEthernetInterfaces(std::function<void(const QVariantList&)> done) {
    execute({ QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("DEVICE,TYPE,STATE,CONNECTION"),
                QStringLiteral("device"), QStringLiteral("status") },
        [this, done = std::move(done)](const NmcliResult& result) {
            const auto interfaces = parseDeviceStatus(result.output, QStringLiteral("ethernet"));
            m_ethernetInterfaces = interfaces;

            QList<NmcliEthernetDevice::Data> devices;
            QVariantList list;
            for (const auto& iface : interfaces) {
                NmcliEthernetDevice::Data data;
                data.iface = iface.device;
                data.type = iface.type;
                data.state = iface.state;
                data.connection = iface.connection;
                data.connected = isConnectedState(iface.state);
                devices.append(data);

                list.append(QVariantMap{
                    { QStringLiteral("device"), iface.device },
                    { QStringLiteral("type"), iface.type },
                    { QStringLiteral("state"), iface.state },
                    { QStringLiteral("connection"), iface.connection },
                });
            }

            reconcileEthernetDevices(devices);

            if (done) {
                done(list);
            }
        });
}

void NmcliCore::fetchEthernetDeviceDetails(const QString& interfaceName, std::function<void(const QVariant&)> done) {
    QString name = interfaceName;
    if (name.isEmpty()) {
        for (const auto& iface : std::as_const(m_ethernetInterfaces)) {
            if (isConnectedState(iface.state)) {
                name = iface.device;
                break;
            }
        }
        if (name.isEmpty()) {
            if (done) {
                done(QVariant());
            }
            return;
        }
    }

    execute({ QStringLiteral("device"), QStringLiteral("show"), name },
        [this, done = std::move(done)](const NmcliResult& result) {
            if (!result.success || result.output.isEmpty()) {
                // Transient failure (e.g. nmcli busy during a toggle). Keep the
                // previous details so dependent UI doesn't blink out and back.
                if (done) {
                    done(m_ethernetDeviceDetails);
                }
                return;
            }

            setEthernetDeviceDetails(parseDeviceDetails(result.output));
            if (done) {
                done(m_ethernetDeviceDetails);
            }
        });
}

void NmcliCore::loadSavedConnections(std::function<void()> done) {
    execute({ QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME,TYPE"), QStringLiteral("connection"),
                QStringLiteral("show") },
        [this, done = std::move(done)](const NmcliResult& result) {
            if (!result.success) {
                m_savedConnections.clear();
                m_savedConnectionSsids.clear();
                if (done) {
                    done();
                }
                return;
            }

            QStringList connections;
            QStringList wifiConnections;
            const QStringList lines = result.output.trimmed().split(u'\n', Qt::SkipEmptyParts);
            for (const QString& line : lines) {
                const QStringList fields = splitTerseLine(line);
                if (fields.size() < 2) {
                    continue;
                }
                connections << fields.at(0);
                if (fields.at(1) == QStringLiteral("802-11-wireless")) {
                    wifiConnections << fields.at(0);
                }
            }

            m_savedConnections = connections;
            m_savedConnectionSsids.clear();
            queryNextSsid(wifiConnections, 0, done);
        });
}

void NmcliCore::queryNextSsid(const QStringList& queue, qsizetype index, std::function<void()> done) {
    if (index >= queue.size()) {
        if (done) {
            done();
        }
        return;
    }

    execute({ QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("802-11-wireless.ssid"),
                QStringLiteral("connection"), QStringLiteral("show"), queue.at(index) },
        [this, queue, index, done = std::move(done)](const NmcliResult& result) {
            if (result.success) {
                processSsidOutput(result.output);
            }
            queryNextSsid(queue, index + 1, done);
        });
}

void NmcliCore::processSsidOutput(const QString& output) {
    const QStringList lines = output.trimmed().split(u'\n');
    for (const QString& line : lines) {
        const QStringList fields = splitTerseLine(line);
        if (fields.size() < 2 || fields.at(0) != QStringLiteral("802-11-wireless.ssid")) {
            continue;
        }

        const QString ssid = fields.mid(1).join(u':').trimmed();
        if (ssid.isEmpty()) {
            continue;
        }

        const QString ssidLower = ssid.toLower();
        const bool exists = std::any_of(m_savedConnectionSsids.cbegin(), m_savedConnectionSsids.cend(),
            [&ssidLower](const QString& s) { return s.toLower() == ssidLower; });
        if (!exists) {
            m_savedConnectionSsids << ssid;
        }
    }
}

void NmcliCore::refreshOnConnectionChange() {
    getNetworks([this] {
        if (m_active) {
            QTimer::singleShot(500, this, [this] {
                for (const auto& iface : std::as_const(m_ethernetInterfaces)) {
                    if (isConnectedState(iface.state) && !iface.device.isEmpty()) {
                        fetchEthernetDeviceDetails(iface.device);
                        break;
                    }
                }
            });
        } else {
            setEthernetDeviceDetails(QVariant());
        }

        getWirelessInterfaces();
        fetchEthernetInterfaces([this](const QVariantList&) {
            if (m_activeEthernet && m_activeEthernet->connected()) {
                const QString iface = m_activeEthernet->iface();
                QTimer::singleShot(500, this, [this, iface] { fetchEthernetDeviceDetails(iface); });
            }
        });
    });
}

void NmcliCore::reconcileNetworks(const QList<NmcliAccessPoint::Data>& networks) {
    const auto keyOf = [](const QString& ssid, const QString& bssid, int frequency) {
        return QString::number(frequency) + u':' + ssid + u':' + bssid;
    };

    QSet<QString> newKeys;
    for (const auto& network : networks) {
        newKeys.insert(keyOf(network.ssid, network.bssid, network.frequency));
    }

    bool membershipChanged = false;
    for (qsizetype i = m_networks.size() - 1; i >= 0; --i) {
        auto* entry = m_networks.at(i);
        if (!newKeys.contains(keyOf(entry->ssid(), entry->bssid(), entry->frequency()))) {
            m_networks.removeAt(i);
            entry->deleteLater();
            membershipChanged = true;
        }
    }

    QHash<QString, NmcliAccessPoint*> existing;
    for (auto* entry : std::as_const(m_networks)) {
        existing.insert(keyOf(entry->ssid(), entry->bssid(), entry->frequency()), entry);
    }

    for (const auto& network : networks) {
        if (auto* entry = existing.value(keyOf(network.ssid, network.bssid, network.frequency))) {
            entry->update(network);
        } else {
            m_networks.append(new NmcliAccessPoint(network, this));
            membershipChanged = true;
        }
    }

    if (membershipChanged) {
        emit networksChanged();
    }
    updateActiveNetwork();
}

void NmcliCore::reconcileEthernetDevices(const QList<NmcliEthernetDevice::Data>& devices) {
    QSet<QString> newKeys;
    for (const auto& device : devices) {
        newKeys.insert(device.iface);
    }

    bool membershipChanged = false;
    for (qsizetype i = m_ethernetDevices.size() - 1; i >= 0; --i) {
        auto* entry = m_ethernetDevices.at(i);
        if (!newKeys.contains(entry->iface())) {
            m_ethernetDevices.removeAt(i);
            entry->deleteLater();
            membershipChanged = true;
        }
    }

    QHash<QString, NmcliEthernetDevice*> existing;
    for (auto* entry : std::as_const(m_ethernetDevices)) {
        existing.insert(entry->iface(), entry);
    }

    for (const auto& device : devices) {
        if (auto* entry = existing.value(device.iface)) {
            entry->update(device);
        } else {
            m_ethernetDevices.append(new NmcliEthernetDevice(device, this));
            membershipChanged = true;
        }
    }

    if (membershipChanged) {
        emit ethernetDevicesChanged();
    }
    updateDerivedEthernet();
}

void NmcliCore::updateActiveNetwork() {
    NmcliAccessPoint* newActive = nullptr;
    for (auto* network : std::as_const(m_networks)) {
        if (network->active()) {
            newActive = network;
            break;
        }
    }

    if (newActive != m_active) {
        m_active = newActive;
        emit activeChanged();
    }
}

void NmcliCore::updateDerivedEthernet() {
    NmcliEthernetDevice* newActive = nullptr;
    bool hasAvailable = false;
    for (auto* device : std::as_const(m_ethernetDevices)) {
        if (!newActive && device->connected()) {
            newActive = device;
        }
        if (device->state() != QStringLiteral("unavailable")) {
            hasAvailable = true;
        }
    }

    if (newActive != m_activeEthernet) {
        m_activeEthernet = newActive;
        emit activeEthernetChanged();
    }
    if (hasAvailable != m_hasAvailableEthernet) {
        m_hasAvailableEthernet = hasAvailable;
        emit hasAvailableEthernetChanged();
    }
}

void NmcliCore::updateConnectingSsid() {
    QString ssid;
    for (const auto& iface : std::as_const(m_wirelessInterfaces)) {
        if (isConnectingState(iface.state)) {
            ssid = iface.connection;
            break;
        }
    }

    if (ssid != m_connectingSsid) {
        m_connectingSsid = ssid;
        emit connectingSsidChanged();
    }
}

bool NmcliCore::handlePasswordRequired(ProcHandle* handle, const QString& error, const QString& output, int exitCode) {
    if (error.isEmpty() || handle->callbackCalled) {
        return false;
    }
    if (!isConnectionCommand(handle->args) || !m_pendingConnection || !m_pendingConnection->callback) {
        return false;
    }
    if (!detectPasswordRequired(error)) {
        return false;
    }

    stopPendingTimers();
    const PendingConnection pending = *m_pendingConnection;
    setPendingConnection(std::nullopt);
    handle->callbackCalled = true;

    const QVariantMap result{
        { QStringLiteral("success"), false },
        { QStringLiteral("output"), output },
        { QStringLiteral("error"), error },
        { QStringLiteral("exitCode"), exitCode },
        { QStringLiteral("needsPassword"), true },
    };
    pending.callback->invoke(result);
    if (handle->jsCallback && handle->jsCallback != pending.callback) {
        handle->jsCallback->invoke(result);
    }
    return true;
}

void NmcliCore::checkPendingConnection() {
    if (!m_pendingConnection) {
        return;
    }

    if (m_active && m_active->ssid() == m_pendingConnection->ssid) {
        stopPendingTimers();
        const PendingConnection pending = *m_pendingConnection;
        setPendingConnection(std::nullopt);
        if (pending.callback) {
            pending.callback->invoke(connectedResult());
        }
    } else if (!m_immediateCheckTimer->isActive()) {
        m_immediateCheckTimer->start();
    }
}

void NmcliCore::onConnectionCheckTimeout() {
    if (!m_pendingConnection) {
        return;
    }

    if (m_active && m_active->ssid() == m_pendingConnection->ssid) {
        setPendingConnection(std::nullopt);
        m_immediateCheckTimer->stop();
        m_immediateCheckCount = 0;
        return;
    }

    if (!m_pendingConnection->callback) {
        return;
    }

    for (auto* handle : std::as_const(m_activeProcesses)) {
        const QString error = handle->stderrText.trimmed();
        if (!error.isEmpty() && handlePasswordRequired(handle, error, handle->stdoutText, -1)) {
            return;
        }
    }

    const PendingConnection pending = *m_pendingConnection;
    setPendingConnection(std::nullopt);
    m_immediateCheckTimer->stop();
    m_immediateCheckCount = 0;

    emit connectionFailed(pending.ssid);
    pending.callback->invoke(QVariantMap{
        { QStringLiteral("success"), false },
        { QStringLiteral("output"), QString() },
        { QStringLiteral("error"), QStringLiteral("Connection timeout") },
        { QStringLiteral("exitCode"), -1 },
        { QStringLiteral("needsPassword"), false },
    });
}

void NmcliCore::onImmediateCheckTick() {
    if (!m_pendingConnection) {
        m_immediateCheckTimer->stop();
        m_immediateCheckCount = 0;
        return;
    }

    ++m_immediateCheckCount;

    if (m_active && m_active->ssid() == m_pendingConnection->ssid) {
        stopPendingTimers();
        const PendingConnection pending = *m_pendingConnection;
        setPendingConnection(std::nullopt);
        if (pending.callback) {
            pending.callback->invoke(connectedResult());
        }
        return;
    }

    for (auto* handle : std::as_const(m_activeProcesses)) {
        const QString error = handle->stderrText.trimmed();
        if (!error.isEmpty() && handlePasswordRequired(handle, error, handle->stdoutText, -1)) {
            return;
        }
    }

    if (m_immediateCheckCount >= maxImmediateChecks) {
        m_immediateCheckTimer->stop();
        m_immediateCheckCount = 0;
    }
}

void NmcliCore::stopPendingTimers() {
    m_connectionCheckTimer->stop();
    m_immediateCheckTimer->stop();
    m_immediateCheckCount = 0;
}

void NmcliCore::setPendingConnection(const std::optional<PendingConnection>& pending) {
    m_pendingConnection = pending;
    emit pendingConnectionChanged();
}

void NmcliCore::setWifiEnabled(bool enabled) {
    if (enabled != m_wifiEnabled) {
        m_wifiEnabled = enabled;
        emit wifiEnabledChanged();
    }
}

void NmcliCore::setEthernetSpeed(const QString& speed) {
    if (speed != m_ethernetSpeed) {
        m_ethernetSpeed = speed;
        emit ethernetSpeedChanged();
    }
}

void NmcliCore::setEthernetDataUsage(const QString& usage) {
    if (usage != m_ethernetDataUsage) {
        m_ethernetDataUsage = usage;
        emit ethernetDataUsageChanged();
    }
}

void NmcliCore::setEthernetDeviceDetails(const QVariant& details) {
    if (details != m_ethernetDeviceDetails) {
        m_ethernetDeviceDetails = details;
        emit ethernetDeviceDetailsChanged();
    }
}

void NmcliCore::applyEnvironment(QProcess* process) {
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
    process->setProcessEnvironment(env);
}

} // namespace caelestia::services
