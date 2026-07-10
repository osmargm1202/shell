pragma Singleton

import QtQuick
import Quickshell
import Caelestia.Services

Singleton {
    id: root

    readonly property alias networks: core.networks
    readonly property alias ethernetDevices: core.ethernetDevices
    readonly property alias active: core.active
    readonly property alias activeEthernet: core.activeEthernet
    readonly property alias wifiEnabled: core.wifiEnabled
    readonly property alias scanning: core.scanning
    readonly property alias hasAvailableEthernet: core.hasAvailableEthernet
    readonly property alias ethernetSpeed: core.ethernetSpeed
    readonly property alias ethernetDeviceDetails: core.ethernetDeviceDetails
    readonly property alias ethernetDataUsage: core.ethernetDataUsage
    readonly property alias pendingConnection: core.pendingConnection

    signal connectionFailed(string ssid)

    function connectingSsid(): string {
        return core.connectingSsid;
    }

    function rescanWifi(): void {
        core.rescanWifi();
    }

    function connectToNetworkWithPasswordCheck(ssid: string, isSecure: bool, callback: var, bssid: string): void {
        core.connectToNetworkWithPasswordCheck(ssid, isSecure, callback, bssid);
    }

    function connectToNetwork(ssid: string, password: string, bssid: string, callback: var): void {
        core.connectToNetwork(ssid, password, bssid, callback);
    }

    function disconnectFromNetwork(): void {
        core.disconnectFromNetwork();
    }

    function forgetNetwork(ssid: string, callback: var): void {
        core.forgetNetwork(ssid, callback);
    }

    function hasSavedProfile(ssid: string): bool {
        return core.hasSavedProfile(ssid);
    }

    function enableWifi(enabled: bool, callback: var): void {
        core.enableWifi(enabled, callback);
    }

    function toggleWifi(callback: var): void {
        core.toggleWifi(callback);
    }

    function connectEthernet(connectionName: string, interfaceName: string, callback: var): void {
        core.connectEthernet(connectionName, interfaceName, callback);
    }

    function disconnectEthernet(connectionName: string, callback: var): void {
        core.disconnectEthernet(connectionName, callback);
    }

    function getEthernetInterfaces(callback: var): void {
        core.getEthernetInterfaces(callback);
    }

    function getEthernetDeviceDetails(interfaceName: string, callback: var): void {
        core.getEthernetDeviceDetails(interfaceName, callback);
    }

    function getEthernetSpeed(interfaceName: string): void {
        core.getEthernetSpeed(interfaceName);
    }

    function getEthernetDataUsage(interfaceName: string, callback: var): void {
        core.getEthernetDataUsage(interfaceName, callback);
    }

    function getIpv4Config(connectionName: string, callback: var): void {
        core.getIpv4Config(connectionName, callback);
    }

    function setIpv4Config(connectionName: string, config: var, callback: var): void {
        core.setIpv4Config(connectionName, config, callback);
    }

    readonly property NmcliCore core: NmcliCore {
        id: core

        onConnectionFailed: ssid => root.connectionFailed(ssid)
    }
}
