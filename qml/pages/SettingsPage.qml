import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width

            PageHeader {
                title: qsTr("Settings")
            }

            ValueButton {
                label: qsTr("OpenPGP keys")
                value: pgp.secretKeyCount > 0 ? qsTr("%n private key(s)", "", pgp.secretKeyCount) : qsTr("None")
                onClicked: pageStack.push(Qt.resolvedUrl("KeysPage.qml"))
            }

            ValueButton {
                label: qsTr("Git server")
                value: git.isRepo ? (git.remoteUrlDisplay || qsTr("No remote")) : qsTr("Not used")
                onClicked: pageStack.push(Qt.resolvedUrl("GitPage.qml"))
            }

            SectionHeader {
                text: qsTr("Passwords")
            }

            TextSwitch {
                text: qsTr("Show passwords")
                description: qsTr("Otherwise they are hidden until revealed")
                checked: appSettings.showPassword
                onCheckedChanged: appSettings.showPassword = checked
            }

            TextSwitch {
                text: qsTr("Copy password when opened")
                checked: appSettings.copyOnDecrypt
                onCheckedChanged: appSettings.copyOnDecrypt = checked
            }

            Slider {
                width: parent.width
                label: qsTr("Clear clipboard and close password after")
                minimumValue: 0
                maximumValue: 300
                stepSize: 15
                value: appSettings.showTime
                valueText: value > 0 ? qsTr("%n s", "", value) : qsTr("Never")
                onDownChanged: if (!down) appSettings.showTime = value
            }

            Slider {
                width: parent.width
                label: qsTr("Remember key passphrase for")
                minimumValue: 0
                maximumValue: 60
                stepSize: 1
                value: appSettings.passphraseTimeout
                valueText: value > 0 ? qsTr("%n min", "", value) : qsTr("Until closed")
                onDownChanged: if (!down) appSettings.passphraseTimeout = value
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Forget passphrase now")
                enabled: pgp.hasPassphrase
                onClicked: pgp.forgetPassphrase()
            }

            SectionHeader {
                text: qsTr("Cover")
            }

            TextSwitch {
                text: qsTr("Show one-time code on cover")
                description: qsTr("The last one-time code you opened, until the key passphrase is forgotten or the app closes. Anyone who sees the phone can read it.")
                checked: appSettings.otpOnCover
                onCheckedChanged: appSettings.otpOnCover = checked
            }

            SectionHeader {
                text: qsTr("Browsing")
            }

            ComboBox {
                label: qsTr("Sort")
                currentIndex: appSettings.sortOrder
                menu: ContextMenu {
                    MenuItem { text: qsTr("Folders first") }
                    MenuItem { text: qsTr("Passwords first") }
                    MenuItem { text: qsTr("By name only") }
                    MenuItem { text: qsTr("Recently used") }
                }
                onCurrentIndexChanged: appSettings.sortOrder = currentIndex
            }

            TextSwitch {
                text: qsTr("Search in subfolders")
                checked: appSettings.filterRecursively
                onCheckedChanged: appSettings.filterRecursively = checked
            }

            TextSwitch {
                text: qsTr("Start with search")
                checked: appSettings.searchOnStart
                onCheckedChanged: appSettings.searchOnStart = checked
            }

            TextSwitch {
                text: qsTr("Show hidden files and folders")
                checked: appSettings.showHidden
                onCheckedChanged: appSettings.showHidden = checked
            }

            SectionHeader {
                text: qsTr("Store")
            }

            TextField {
                id: storePathField

                width: parent.width
                label: qsTr("Store folder")
                placeholderText: store.defaultRootPath
                text: appSettings.storePath
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: {
                    appSettings.storePath = text.trim()
                    focus = false
                }
                onActiveFocusChanged: {
                    if (!activeFocus)
                        appSettings.storePath = text.trim()
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Leave empty to keep the store in the app's own folder")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }

            ButtonLayout {
                Button {
                    text: qsTr("Set keys for store")
                    enabled: pgp.secretKeyCount > 0 || store.initialized
                    onClicked: pageStack.push(Qt.resolvedUrl("InitStoreDialog.qml"), { dir: "" })
                }
            }
        }

        VerticalScrollDecorator { }
    }
}
