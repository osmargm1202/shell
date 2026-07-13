pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Widgets
import Caelestia.Config
import qs.components
import qs.components.containers
import qs.components.controls
import qs.components.effects
import qs.components.images
import qs.services
import qs.utils

Item {
    id: root

    property string searchQuery: ""
    property var currentResults: []
    property bool isLoading: SteamWorkshopSearcher.loading
    property var selectedWallpaper: null
    property bool detailPanelOpen: false
    property int selectedIndex: -1
    property string requestStatusMessage: ""
    readonly property string statusMessage: SteamWorkshopSearcher.missingApiKey
        ? qsTr("Add a Steam Web API key in services.steamWorkshopApiKey")
        : requestStatusMessage

    function selectWallpaper(index) {
        if (index >= 0 && index < root.currentResults.length) {
            root.selectedWallpaper = root.currentResults[index];
            root.selectedIndex = index;
            root.detailPanelOpen = true;
        }
    }

    function selectNext() {
        if (root.selectedIndex < root.currentResults.length - 1)
            selectWallpaper(root.selectedIndex + 1);
    }

    function selectPrev() {
        if (root.selectedIndex > 0)
            selectWallpaper(root.selectedIndex - 1);
    }

    anchors.fill: parent

    onDetailPanelOpenChanged: {
        if (!detailPanelOpen)
            clearWallpaperTimer.restart();
    }

    ClippingRectangle {
        id: mainClippingRect

        anchors.fill: parent
        anchors.margins: Tokens.padding.medium
        anchors.leftMargin: 0
        anchors.rightMargin: Tokens.padding.medium

        radius: Tokens.rounding.large
        color: "transparent"

        Loader {
            id: mainLoader

            anchors.fill: parent
            anchors.margins: Tokens.padding.large + Tokens.padding.medium
            anchors.leftMargin: Tokens.padding.large
            anchors.rightMargin: Tokens.padding.large

            asynchronous: true
            sourceComponent: mainContentComponent
        }
    }

    Component {
        id: mainContentComponent

        StyledFlickable {
            id: mainFlickable

            flickableDirection: Flickable.VerticalFlick
            contentHeight: mainLayout.height

            StyledScrollBar.vertical: StyledScrollBar {
                flickable: mainFlickable
            }

            ColumnLayout {
                id: mainLayout

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top

                spacing: Tokens.spacing.medium

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.spacing.small

                    StyledTextField {
                        id: searchField

                        Layout.fillWidth: true
                        enabled: !root.isLoading
                        placeholderText: qsTr("Search Steam Workshop...")
                        onTextChanged: root.searchQuery = text

                        Keys.onReturnPressed: {
                            if (root.searchQuery.trim())
                                SteamWorkshopSearcher.search(root.searchQuery);
                        }
                    }

                    CircularIndicator {
                        implicitSize: 20
                        visible: root.isLoading
                    }

                    IconButton {
                        enabled: !root.isLoading
                        icon: "refresh"
                        onClicked: {
                            if (root.searchQuery.trim())
                                SteamWorkshopSearcher.search(root.searchQuery);
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    StyledText {
                        text: root.currentResults.length > 0
                            ? qsTr("Found %1 Workshop items").arg(root.currentResults.length)
                            : qsTr("No results")
                        font: Tokens.font.body.small
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    SplitButton {
                        enabled: !root.isLoading
                        fallbackIcon: "sort"
                        fallbackText: qsTr("Relevance")
                        minLeftWidth: 110
                        menuItems: [
                            MenuItem {
                                text: qsTr("Relevance")
                                onClicked: {
                                    SteamWorkshopSearcher.setQueryType(12);
                                    if (root.searchQuery.trim())
                                        SteamWorkshopSearcher.search(root.searchQuery);
                                }
                            },
                            MenuItem {
                                text: qsTr("Trending")
                                onClicked: {
                                    SteamWorkshopSearcher.setQueryType(3);
                                    if (root.searchQuery.trim())
                                        SteamWorkshopSearcher.search(root.searchQuery);
                                }
                            },
                            MenuItem {
                                text: qsTr("Newest")
                                onClicked: {
                                    SteamWorkshopSearcher.setQueryType(1);
                                    if (root.searchQuery.trim())
                                        SteamWorkshopSearcher.search(root.searchQuery);
                                }
                            },
                            MenuItem {
                                text: qsTr("Popular")
                                onClicked: {
                                    SteamWorkshopSearcher.setQueryType(11);
                                    if (root.searchQuery.trim())
                                        SteamWorkshopSearcher.search(root.searchQuery);
                                }
                            }
                        ]
                    }

                    SplitButton {
                        enabled: !root.isLoading
                        fallbackIcon: "movie"
                        fallbackText: {
                            const labels = {
                                "all": qsTr("Any media"),
                                "video": qsTr("Video"),
                                "gif": qsTr("GIF"),
                                "image": qsTr("Still image")
                            };
                            return labels[SteamWorkshopSearcher.mediaPreference] || labels.all;
                        }
                        minLeftWidth: 110
                        menuItems: [
                            MenuItem {
                                text: qsTr("Any media")
                                onClicked: SteamWorkshopSearcher.setMediaPreference("all")
                            },
                            MenuItem {
                                text: qsTr("Video")
                                onClicked: SteamWorkshopSearcher.setMediaPreference("video")
                            },
                            MenuItem {
                                text: qsTr("GIF")
                                onClicked: SteamWorkshopSearcher.setMediaPreference("gif")
                            },
                            MenuItem {
                                text: qsTr("Still image")
                                onClicked: SteamWorkshopSearcher.setMediaPreference("image")
                            }
                        ]
                    }

                    TextButton {
                        text: qsTr("Load more")
                        visible: SteamWorkshopSearcher.hasMore && !root.isLoading
                        onClicked: SteamWorkshopSearcher.searchNextPage()
                    }
                }

                StyledText {
                    Layout.fillWidth: true
                    text: root.statusMessage
                    visible: text.length > 0
                    wrapMode: Text.Wrap
                    color: Colours.palette.m3error
                    font: Tokens.font.body.small
                }

                GridView {
                    id: resultsGrid

                    Layout.fillWidth: true
                    implicitHeight: 400

                    cellWidth: 180
                    cellHeight: 140

                    model: root.currentResults
                    clip: true

                    populate: Transition {
                        SequentialAnimation {
                            PropertyAction {
                                property: "scale"
                                value: 0.8
                            }
                            PropertyAction {
                                property: "opacity"
                                value: 0
                            }
                            NumberAnimation {
                                properties: "scale,opacity"
                                from: 0.8
                                to: 1
                                duration: Tokens.anim.durations.expressiveDefaultEffects
                            }
                        }
                    }

                    delegate: Item {
                        required property var modelData
                        required property int index
                        readonly property real itemMargin: Tokens.spacing.small / 2

                        width: resultsGrid.cellWidth
                        height: resultsGrid.cellHeight

                        StateLayer {
                            onClicked: root.selectWallpaper(index)

                            anchors.fill: parent
                            anchors.leftMargin: itemMargin
                            anchors.rightMargin: itemMargin
                            anchors.topMargin: itemMargin
                            anchors.bottomMargin: itemMargin

                            radius: Tokens.rounding.medium

                            CachingImage {
                                anchors.fill: parent
                                source: modelData.previewUrl || ""
                                asynchronous: true
                                fillMode: Image.PreserveAspectCrop
                            }
                        }
                    }
                }
            }
        }
    }

    StyledRect {
        id: detailPanel

        color: Colours.tPalette.m3surfaceContainer
        radius: Tokens.rounding.large

        anchors.fill: parent
        anchors.margins: Tokens.padding.large
        opacity: root.detailPanelOpen ? 1 : 0
        scale: root.detailPanelOpen ? 1 : 0.95
        clip: true
        enabled: root.detailPanelOpen

        Behavior on opacity {
            Anim {}
        }

        Behavior on scale {
            Anim {}
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Tokens.spacing.medium

            RowLayout {
                Layout.fillWidth: true

                IconButton {
                    icon: "close"
                    onClicked: root.detailPanelOpen = false
                }

                Item {
                    Layout.fillWidth: true
                }

                TextButton {
                    text: qsTr("Download & Set")
                    onClicked: {
                        if (root.selectedWallpaper)
                            SteamWorkshopSearcher.downloadItem(root.selectedWallpaper);
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                RowLayout {
                    anchors.fill: parent

                    IconButton {
                        icon: "chevron_left"
                        enabled: root.selectedIndex > 0
                        onClicked: root.selectPrev()
                    }

                    CachingImage {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        anchors.margins: Tokens.padding.large
                        source: root.selectedWallpaper?.previewUrl || ""
                        asynchronous: true
                        fillMode: Image.PreserveAspectFit
                    }

                    IconButton {
                        icon: "chevron_right"
                        enabled: root.selectedIndex < root.currentResults.length - 1
                        onClicked: root.selectNext()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Tokens.spacing.small

                RowLayout {
                    Layout.fillWidth: true

                    StyledText {
                        text: root.selectedWallpaper?.title || ""
                        font: Tokens.font.body.small
                        color: Colours.palette.m3outline
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StyledText {
                        text: "%1 / %2".arg(root.selectedIndex + 1).arg(root.currentResults.length)
                        font: Tokens.font.body.small
                        color: Colours.palette.m3outline
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    StyledText {
                        Layout.fillWidth: true
                        text: root.selectedWallpaper?.tags?.length
                            ? qsTr("Tags: %1").arg(root.selectedWallpaper.tags.join(", "))
                            : qsTr("Tags: none")
                        elide: Text.ElideRight
                        font: Tokens.font.body.small
                        color: Colours.palette.m3outline
                    }

                    StyledText {
                        text: root.selectedWallpaper?.timeUpdated
                            ? qsTr("Updated %1").arg(SteamWorkshopSearcher.formattedUpdateDate(root.selectedWallpaper.timeUpdated))
                            : ""
                        font: Tokens.font.body.small
                        color: Colours.palette.m3outline
                    }

                    StyledText {
                        text: root.selectedWallpaper
                            ? qsTr("%1 MiB").arg((root.selectedWallpaper.fileSize / 1048576).toFixed(1))
                            : ""
                        font: Tokens.font.body.small
                        color: Colours.palette.m3outline
                    }
                }
            }
        }
    }

    Timer {
        id: clearWallpaperTimer

        interval: Tokens.anim.durations.expressiveDefaultEffects
        onTriggered: {
            if (!detailPanelOpen)
                root.selectedWallpaper = null;
        }
    }

    Connections {
        function onSearchComplete(results, meta) {
            root.currentResults = SteamWorkshopSearcher.results;
            root.requestStatusMessage = meta.missingApiKey
                ? qsTr("Add a Steam Web API key in services.steamWorkshopApiKey")
                : (meta.error ?? "");
        }

        function onDownloadProgress(id, progress) {
            if (root.selectedWallpaper?.id === id)
                root.requestStatusMessage = qsTr("Downloading… %1%").arg(Math.round(progress * 100));
        }

        function onAuthRequired(username) {
            root.requestStatusMessage = qsTr("Steam authentication required. Run: steamcmd +login %1").arg(username || "<username>");
        }

        function onDownloadComplete(id, path) {
            if (root.selectedWallpaper?.id === id) {
                Wallpapers.setWallpaper(path);
                root.detailPanelOpen = false;
                root.requestStatusMessage = "";
            }
        }

        function onDownloadFailed(id, error) {
            root.requestStatusMessage = error;
        }

        target: SteamWorkshopSearcher
    }
}
