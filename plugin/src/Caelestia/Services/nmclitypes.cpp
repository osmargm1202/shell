#include "nmclitypes.hpp"

namespace caelestia::services {

NmcliAccessPoint::NmcliAccessPoint(const Data& data, QObject* parent)
    : QObject(parent)
    , m_data(data) {}

QString NmcliAccessPoint::ssid() const {
    return m_data.ssid;
}

QString NmcliAccessPoint::bssid() const {
    return m_data.bssid;
}

int NmcliAccessPoint::strength() const {
    return m_data.strength;
}

int NmcliAccessPoint::frequency() const {
    return m_data.frequency;
}

bool NmcliAccessPoint::active() const {
    return m_data.active;
}

QString NmcliAccessPoint::security() const {
    return m_data.security;
}

bool NmcliAccessPoint::isSecure() const {
    return !m_data.security.isEmpty();
}

void NmcliAccessPoint::update(const Data& data) {
    const Data old = m_data;
    m_data = data;

    if (old.ssid != data.ssid) {
        emit ssidChanged();
    }
    if (old.bssid != data.bssid) {
        emit bssidChanged();
    }
    if (old.strength != data.strength) {
        emit strengthChanged();
    }
    if (old.frequency != data.frequency) {
        emit frequencyChanged();
    }
    if (old.active != data.active) {
        emit activeChanged();
    }
    if (old.security != data.security) {
        emit securityChanged();
    }
}

NmcliEthernetDevice::NmcliEthernetDevice(const Data& data, QObject* parent)
    : QObject(parent)
    , m_data(data) {}

QString NmcliEthernetDevice::iface() const {
    return m_data.iface;
}

QString NmcliEthernetDevice::type() const {
    return m_data.type;
}

QString NmcliEthernetDevice::state() const {
    return m_data.state;
}

QString NmcliEthernetDevice::connection() const {
    return m_data.connection;
}

bool NmcliEthernetDevice::connected() const {
    return m_data.connected;
}

QString NmcliEthernetDevice::ipAddress() const {
    return m_data.ipAddress;
}

QString NmcliEthernetDevice::gateway() const {
    return m_data.gateway;
}

QStringList NmcliEthernetDevice::dns() const {
    return m_data.dns;
}

QString NmcliEthernetDevice::subnet() const {
    return m_data.subnet;
}

QString NmcliEthernetDevice::macAddress() const {
    return m_data.macAddress;
}

QString NmcliEthernetDevice::speed() const {
    return m_data.speed;
}

void NmcliEthernetDevice::update(const Data& data) {
    const Data old = m_data;
    m_data = data;

    if (old.iface != data.iface) {
        emit ifaceChanged();
    }
    if (old.type != data.type) {
        emit typeChanged();
    }
    if (old.state != data.state) {
        emit stateChanged();
    }
    if (old.connection != data.connection) {
        emit connectionChanged();
    }
    if (old.connected != data.connected) {
        emit connectedChanged();
    }
    if (old.ipAddress != data.ipAddress) {
        emit ipAddressChanged();
    }
    if (old.gateway != data.gateway) {
        emit gatewayChanged();
    }
    if (old.dns != data.dns) {
        emit dnsChanged();
    }
    if (old.subnet != data.subnet) {
        emit subnetChanged();
    }
    if (old.macAddress != data.macAddress) {
        emit macAddressChanged();
    }
    if (old.speed != data.speed) {
        emit speedChanged();
    }
}

} // namespace caelestia::services
