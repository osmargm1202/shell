#pragma once

#include "nmclitypes.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <qjsvalue.h>
#include <qprocess.h>
#include <qqmlintegration.h>
#include <qtimer.h>
#include <qvariant.h>

namespace caelestia::services {

class NmcliCore;

// Wraps a QJSValue callback with invoke-once semantics: a user callback must
// never fire twice, even if both the process exit path and the
// pending-connection state machine try to resolve it.
class NmcliCallback {
public:
    NmcliCallback(NmcliCore* core, const QJSValue& function);

    void invoke(const QVariantMap& result);
    void invokeValue(const QVariant& value);

    std::function<QVariantMap(QVariantMap)> transform;

private:
    NmcliCore* m_core;
    QJSValue m_function;
    bool m_called;
};

using NmcliCallbackPtr = std::shared_ptr<NmcliCallback>;

struct NmcliResult {
    bool success = false;
    QString output;
    QString error;
    int exitCode = 0;
    bool needsPassword = false;

    [[nodiscard]] QVariantMap toMap() const;
};

class NmcliCore : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QList<caelestia::services::NmcliAccessPoint*> networks READ networks NOTIFY networksChanged)
    Q_PROPERTY(QList<caelestia::services::NmcliEthernetDevice*> ethernetDevices READ ethernetDevices NOTIFY
            ethernetDevicesChanged)
    Q_PROPERTY(caelestia::services::NmcliAccessPoint* active READ active NOTIFY activeChanged)
    Q_PROPERTY(caelestia::services::NmcliEthernetDevice* activeEthernet READ activeEthernet NOTIFY activeEthernetChanged)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY wifiEnabledChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool hasAvailableEthernet READ hasAvailableEthernet NOTIFY hasAvailableEthernetChanged)
    Q_PROPERTY(QString connectingSsid READ connectingSsid NOTIFY connectingSsidChanged)
    Q_PROPERTY(QString ethernetSpeed READ ethernetSpeed NOTIFY ethernetSpeedChanged)
    Q_PROPERTY(QVariant ethernetDeviceDetails READ ethernetDeviceDetails NOTIFY ethernetDeviceDetailsChanged)
    Q_PROPERTY(QString ethernetDataUsage READ ethernetDataUsage NOTIFY ethernetDataUsageChanged)
    Q_PROPERTY(QVariant pendingConnection READ pendingConnection NOTIFY pendingConnectionChanged)

public:
    explicit NmcliCore(QObject* parent = nullptr);
    ~NmcliCore() override;

    [[nodiscard]] QList<NmcliAccessPoint*> networks() const;
    [[nodiscard]] QList<NmcliEthernetDevice*> ethernetDevices() const;
    [[nodiscard]] NmcliAccessPoint* active() const;
    [[nodiscard]] NmcliEthernetDevice* activeEthernet() const;
    [[nodiscard]] bool wifiEnabled() const;
    [[nodiscard]] bool scanning() const;
    [[nodiscard]] bool hasAvailableEthernet() const;
    [[nodiscard]] QString connectingSsid() const;
    [[nodiscard]] QString ethernetSpeed() const;
    [[nodiscard]] QVariant ethernetDeviceDetails() const;
    [[nodiscard]] QString ethernetDataUsage() const;
    [[nodiscard]] QVariant pendingConnection() const;

    Q_INVOKABLE void rescanWifi();
    Q_INVOKABLE void connectToNetworkWithPasswordCheck(
        const QString& ssid, bool isSecure, QJSValue callback = QJSValue(), const QString& bssid = QString());
    Q_INVOKABLE void connectToNetwork(
        const QString& ssid, const QString& password, const QString& bssid, QJSValue callback = QJSValue());
    Q_INVOKABLE void disconnectFromNetwork();
    Q_INVOKABLE void forgetNetwork(const QString& ssid, QJSValue callback = QJSValue());
    Q_INVOKABLE [[nodiscard]] bool hasSavedProfile(const QString& ssid) const;
    Q_INVOKABLE void enableWifi(bool enabled, QJSValue callback = QJSValue());
    Q_INVOKABLE void toggleWifi(QJSValue callback = QJSValue());
    Q_INVOKABLE void connectEthernet(
        const QString& connectionName, const QString& interfaceName, QJSValue callback = QJSValue());
    Q_INVOKABLE void disconnectEthernet(const QString& connectionName, QJSValue callback = QJSValue());
    Q_INVOKABLE void getEthernetInterfaces(QJSValue callback = QJSValue());
    Q_INVOKABLE void getEthernetDeviceDetails(const QString& interfaceName, QJSValue callback = QJSValue());
    Q_INVOKABLE void getEthernetSpeed(const QString& interfaceName);
    Q_INVOKABLE void getEthernetDataUsage(const QString& interfaceName, QJSValue callback = QJSValue());
    Q_INVOKABLE void getIpv4Config(const QString& connectionName, QJSValue callback = QJSValue());
    Q_INVOKABLE void setIpv4Config(
        const QString& connectionName, const QVariantMap& config, QJSValue callback = QJSValue());

signals:
    void connectionFailed(const QString& ssid);

    void networksChanged();
    void ethernetDevicesChanged();
    void activeChanged();
    void activeEthernetChanged();
    void wifiEnabledChanged();
    void scanningChanged();
    void hasAvailableEthernetChanged();
    void connectingSsidChanged();
    void ethernetSpeedChanged();
    void ethernetDeviceDetailsChanged();
    void ethernetDataUsageChanged();
    void pendingConnectionChanged();

private:
    using Handler = std::function<void(const NmcliResult&)>;

    struct DeviceStatus {
        QString device;
        QString type;
        QString state;
        QString connection;
    };

    struct ProcHandle {
        QProcess* process = nullptr;
        QStringList args;
        QString stdoutText;
        QString stderrText;
        bool callbackCalled = false;
        bool done = false;
        NmcliCallbackPtr jsCallback;
        Handler handler;
    };

    struct PendingConnection {
        QString ssid;
        QString bssid;
        NmcliCallbackPtr callback;
        int retryCount = 0;
    };

    // Process runner
    void execute(const QStringList& args, Handler handler = nullptr, const NmcliCallbackPtr& jsCallback = nullptr);
    void finishProcess(ProcHandle* handle, int exitCode, QProcess::ExitStatus status);
    NmcliCallbackPtr wrap(const QJSValue& callback);

    // Parsing helpers
    [[nodiscard]] static QStringList splitTerseLine(const QString& line);
    [[nodiscard]] static int parseLeadingInt(const QString& s);
    [[nodiscard]] static bool isConnectionCommand(const QStringList& args);
    [[nodiscard]] static bool isConnectedState(const QString& state);
    [[nodiscard]] static bool isConnectingState(const QString& state);
    [[nodiscard]] static bool detectPasswordRequired(const QString& error);
    [[nodiscard]] static QString cidrToSubnetMask(const QString& cidr);
    [[nodiscard]] static QString formatBytes(qint64 bytes);
    [[nodiscard]] static QList<DeviceStatus> parseDeviceStatus(const QString& output, const QString& filterType);
    [[nodiscard]] static QVariantMap parseDeviceDetails(const QString& output);
    [[nodiscard]] static QList<NmcliAccessPoint::Data> parseNetworkOutput(const QString& output);
    [[nodiscard]] static QList<NmcliAccessPoint::Data> deduplicateNetworks(const QList<NmcliAccessPoint::Data>& networks);

    // Read paths
    void getWifiStatus(std::function<void(bool)> done = nullptr);
    void getNetworks(std::function<void()> done = nullptr);
    void getWirelessInterfaces();
    void fetchEthernetInterfaces(std::function<void(const QVariantList&)> done = nullptr);
    void fetchEthernetDeviceDetails(const QString& interfaceName, std::function<void(const QVariant&)> done = nullptr);
    void loadSavedConnections(std::function<void()> done = nullptr);
    void queryNextSsid(const QStringList& queue, qsizetype index, std::function<void()> done);
    void processSsidOutput(const QString& output);
    void refreshOnConnectionChange();

    // Reconcile + derived state
    void reconcileNetworks(const QList<NmcliAccessPoint::Data>& networks);
    void reconcileEthernetDevices(const QList<NmcliEthernetDevice::Data>& devices);
    void updateActiveNetwork();
    void updateDerivedEthernet();
    void updateConnectingSsid();

    // Mutations + pending-connection state machine
    void connectWireless(
        const QString& ssid, const QString& password, const QString& bssid, const NmcliCallbackPtr& callback, int retryCount);
    void createConnectionWithPassword(
        const QString& ssid, const QString& bssidUpper, const QString& password, const NmcliCallbackPtr& callback);
    void checkAndDeleteConnection(const QString& ssid, std::function<void()> done);
    void activateConnection(const QString& connectionName, const NmcliCallbackPtr& callback);
    bool handlePasswordRequired(ProcHandle* handle, const QString& error, const QString& output, int exitCode);
    void checkPendingConnection();
    void onConnectionCheckTimeout();
    void onImmediateCheckTick();
    void stopPendingTimers();
    void setPendingConnection(const std::optional<PendingConnection>& pending);

    // Property setters
    void setWifiEnabled(bool enabled);
    void setEthernetSpeed(const QString& speed);
    void setEthernetDataUsage(const QString& usage);
    void setEthernetDeviceDetails(const QVariant& details);

    static void applyEnvironment(QProcess* process);

    QList<NmcliAccessPoint*> m_networks;
    QList<NmcliEthernetDevice*> m_ethernetDevices;
    NmcliAccessPoint* m_active = nullptr;
    NmcliEthernetDevice* m_activeEthernet = nullptr;
    bool m_wifiEnabled = true;
    bool m_hasAvailableEthernet = false;
    QString m_connectingSsid;
    QString m_ethernetSpeed;
    QVariant m_ethernetDeviceDetails;
    QString m_ethernetDataUsage;

    QList<DeviceStatus> m_wirelessInterfaces;
    QList<DeviceStatus> m_ethernetInterfaces;
    QStringList m_savedConnections;
    QStringList m_savedConnectionSsids;

    QList<ProcHandle*> m_activeProcesses;
    std::optional<PendingConnection> m_pendingConnection;

    QProcess* m_monitorProcess;
    QProcess* m_rescanProcess;
    QTimer* m_monitorRestartTimer;
    QTimer* m_refreshDebounce;
    QTimer* m_connectionCheckTimer;
    QTimer* m_immediateCheckTimer;
    int m_immediateCheckCount = 0;
};

} // namespace caelestia::services
