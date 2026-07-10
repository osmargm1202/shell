#pragma once

#include <qprocess.h>
#include <qqmlintegration.h>
#include <qtimer.h>
#include <qvariant.h>

namespace caelestia::services {

// UI-agnostic VPN orchestration: provider config comes in via the `providers`
// property (bound from GlobalConfig in the facade), toasts stay in QML driven
// by the stateTransition() signal. Status checks are debounced and triggered
// externally via scheduleStatusCheck() (wired to NmcliCore::connectionEvent()
// so no second `nmcli monitor` process is spawned).
class VpnCore : public QObject {
    Q_OBJECT
    QML_ELEMENT

    // Raw GlobalConfig.utilities.vpn.provider list; first enabled entry wins.
    Q_PROPERTY(QVariantList providers READ providers WRITE setProviders NOTIFY providersChanged)
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayNameChanged)

public:
    explicit VpnCore(QObject* parent = nullptr);
    ~VpnCore() override;

    [[nodiscard]] QVariantList providers() const;
    void setProviders(const QVariantList& providers);
    [[nodiscard]] QVariantMap status() const;
    [[nodiscard]] bool connected() const;
    [[nodiscard]] bool connecting() const;
    [[nodiscard]] bool enabled() const;
    [[nodiscard]] QString displayName() const;

    Q_INVOKABLE void connect();
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void checkStatus();

public slots:
    void scheduleStatusCheck();

signals:
    void providersChanged();
    void statusChanged();
    void connectedChanged();
    void connectingChanged();
    void enabledChanged();
    void displayNameChanged();
    // Emitted only when status.state actually changes; the facade uses this
    // to drive toasts. Also re-usable for the connect()-while-needs-auth path.
    void stateTransition(const QString& previousState, const QVariantMap& status);

private:
    struct ProviderConfig {
        QString name;
        QString interface;
        QString displayName;
        QStringList connectCmd;
        QStringList disconnectCmd;
    };

    void resolveProvider();
    [[nodiscard]] static ProviderConfig builtinDefaults(const QString& name, const QString& iface);
    [[nodiscard]] QStringList statusCommand() const;

    [[nodiscard]] QVariantMap parseStatusOutput(const QString& output) const;
    [[nodiscard]] static QVariantMap parseTailscaleStatus(const QString& output);
    [[nodiscard]] static QVariantMap parseNetBirdStatus(const QString& output);
    [[nodiscard]] static QVariantMap parseWarpStatus(const QString& output);
    [[nodiscard]] QVariantMap parseWireGuardStatus(const QString& output) const;

    [[nodiscard]] static QString extractAuthUrl(const QString& text);
    [[nodiscard]] static QVariantMap makeStatus(
        bool connected, const QString& state, const QString& reason = QString(), const QString& authUrl = QString());

    void updateStatus(QVariantMap newStatus);
    void resetStatus();
    void setConnected(bool connected);
    void setEnabled(bool enabled);
    void updateConnecting();

    void processConnectStdoutLine(const QString& line);
    void handleConnectStderr(const QString& error);
    void handleStatusStderr(const QString& text);
    void onConnectFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onStatusFinished();

    static void applyEnvironment(QProcess* process);
    void startProcess(QProcess* process, const QStringList& cmd);
    static void killProcess(QProcess* process);

    QVariantList m_providers;
    ProviderConfig m_config;
    bool m_enabled = false;
    bool m_hasResolved = false;

    QVariantMap m_status;
    bool m_connected = false;
    bool m_connecting = false;

    QString m_connectStdoutBuffer;

    QProcess* m_connectProc;
    QProcess* m_disconnectProc;
    QProcess* m_statusProc;
    QProcess* m_warpRegisterProc;
    QTimer* m_statusCheckTimer;
};

} // namespace caelestia::services
