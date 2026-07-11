import QtQuick
import qs.modules.dashboard
import qs.modules.nexus.common

PageBase {
    title: qsTr("Steam Workshop")
    isSubPage: true
    scrollable: false

    SteamWorkshopTab {
        anchors.fill: parent
    }
}
