#pragma once

#include <qobject.h>
#include <qqmlintegration.h>
#include <qstringlist.h>

namespace caelestia::services {

class NmcliAccessPoint : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("NmcliAccessPoint instances can only be retrieved from NmcliCore")

    Q_PROPERTY(QString ssid READ ssid NOTIFY ssidChanged)
    Q_PROPERTY(QString bssid READ bssid NOTIFY bssidChanged)
    Q_PROPERTY(int strength READ strength NOTIFY strengthChanged)
    Q_PROPERTY(int frequency READ frequency NOTIFY frequencyChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString security READ security NOTIFY securityChanged)
    Q_PROPERTY(bool isSecure READ isSecure NOTIFY securityChanged)

public:
    struct Data {
        bool active = false;
        int strength = 0;
        int frequency = 0;
        QString ssid;
        QString bssid;
        QString security;
    };

    explicit NmcliAccessPoint(const Data& data, QObject* parent = nullptr);

    [[nodiscard]] QString ssid() const;
    [[nodiscard]] QString bssid() const;
    [[nodiscard]] int strength() const;
    [[nodiscard]] int frequency() const;
    [[nodiscard]] bool active() const;
    [[nodiscard]] QString security() const;
    [[nodiscard]] bool isSecure() const;

    void update(const Data& data);

signals:
    void ssidChanged();
    void bssidChanged();
    void strengthChanged();
    void frequencyChanged();
    void activeChanged();
    void securityChanged();

private:
    Data m_data;
};

class NmcliEthernetDevice : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("NmcliEthernetDevice instances can only be retrieved from NmcliCore")

    Q_PROPERTY(QString iface READ iface NOTIFY ifaceChanged)
    Q_PROPERTY(QString type READ type NOTIFY typeChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString connection READ connection NOTIFY connectionChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString ipAddress READ ipAddress NOTIFY ipAddressChanged)
    Q_PROPERTY(QString gateway READ gateway NOTIFY gatewayChanged)
    Q_PROPERTY(QStringList dns READ dns NOTIFY dnsChanged)
    Q_PROPERTY(QString subnet READ subnet NOTIFY subnetChanged)
    Q_PROPERTY(QString macAddress READ macAddress NOTIFY macAddressChanged)
    Q_PROPERTY(QString speed READ speed NOTIFY speedChanged)

public:
    struct Data {
        QString iface;
        QString type;
        QString state;
        QString connection;
        bool connected = false;
        QString ipAddress;
        QString gateway;
        QStringList dns;
        QString subnet;
        QString macAddress;
        QString speed;
    };

    explicit NmcliEthernetDevice(const Data& data, QObject* parent = nullptr);

    [[nodiscard]] QString iface() const;
    [[nodiscard]] QString type() const;
    [[nodiscard]] QString state() const;
    [[nodiscard]] QString connection() const;
    [[nodiscard]] bool connected() const;
    [[nodiscard]] QString ipAddress() const;
    [[nodiscard]] QString gateway() const;
    [[nodiscard]] QStringList dns() const;
    [[nodiscard]] QString subnet() const;
    [[nodiscard]] QString macAddress() const;
    [[nodiscard]] QString speed() const;

    void update(const Data& data);

signals:
    void ifaceChanged();
    void typeChanged();
    void stateChanged();
    void connectionChanged();
    void connectedChanged();
    void ipAddressChanged();
    void gatewayChanged();
    void dnsChanged();
    void subnetChanged();
    void macAddressChanged();
    void speedChanged();

private:
    Data m_data;
};

} // namespace caelestia::services
