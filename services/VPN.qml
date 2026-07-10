pragma Singleton

import QtQuick
import Quickshell
import Caelestia
import Caelestia.Config
import Caelestia.Services
import qs.services

Singleton {
    id: root

    readonly property alias connected: core.connected
    readonly property alias status: core.status
    readonly property alias connecting: core.connecting
    readonly property alias enabled: core.enabled

    readonly property VpnCore core: VpnCore {
        id: core

        providers: GlobalConfig.utilities.vpn.provider

        onStateTransition: (previousState, status) => root.emitStatusToast(previousState, status)
    }

    function connect(): void {
        if (core.status.state === "needs-auth" && core.status.authUrl) {
            emitStatusToast(core.status.state, core.status);
            return;
        }
        core.connect();
    }

    function disconnect(): void {
        core.disconnect();
    }

    function toggle(): void {
        connected ? disconnect() : connect();
    }

    function checkStatus(): void {
        core.checkStatus();
    }

    function emitStatusToast(previousState: string, statusObj: var): void {
        if (!GlobalConfig.utilities.toasts.vpnChanged)
            return;

        const displayName = core.displayName || "VPN";

        switch (statusObj.state) {
        case "connected":
            Toaster.toast(qsTr("VPN connected"), qsTr("Connected to %1").arg(displayName), "vpn_key");
            break;
        case "disconnected":
            Toaster.toast(qsTr("VPN disconnected"), qsTr("Disconnected from %1").arg(displayName), "vpn_key_off");
            break;
        case "needs-auth":
            const authMsg = statusObj.reason || "Authentication required";
            Toaster.toast(qsTr("VPN authentication required"), qsTr("%1: %2").arg(displayName).arg(authMsg), "vpn_lock");
            break;
        case "error":
            if (previousState === "connected" || previousState === "connecting" || previousState === "needs-auth") {
                const errMsg = statusObj.reason || "Unknown error";
                Toaster.toast(qsTr("VPN error"), qsTr("%1: %2").arg(displayName).arg(errMsg), "error");
            }
            break;
        }
    }

    Connections {
        target: Nmcli.core

        function onConnectionEvent(): void {
            core.scheduleStatusCheck();
        }
    }
}
