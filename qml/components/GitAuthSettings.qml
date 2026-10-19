import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0

// How to log in to the git server, shared by cloning and the git settings
Column {
    id: root

    width: parent.width

    ComboBox {
        label: qsTr("Log in with")
        currentIndex: appSettings.gitAuth === "password" ? 1 : appSettings.gitAuth === "none" ? 2 : 0
        menu: ContextMenu {
            MenuItem { text: qsTr("SSH key") }
            MenuItem { text: qsTr("Password or token") }
            MenuItem { text: qsTr("Nothing") }
        }
        onCurrentIndexChanged: appSettings.gitAuth = ["ssh-key", "password", "none"][currentIndex]
    }

    TextField {
        width: parent.width
        visible: appSettings.gitAuth === "password"
        label: qsTr("Username, for https")
        placeholderText: qsTr("Username")
        text: appSettings.gitUsername
        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        onTextChanged: appSettings.gitUsername = text.trim()
    }

    Label {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        visible: appSettings.gitAuth === "password"
        text: qsTr("The password is asked for when needed. For ssh, put the user in the URL.")
        wrapMode: Text.Wrap
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryHighlightColor
    }

    Column {
        width: parent.width
        visible: appSettings.gitAuth === "ssh-key"

        SectionHeader {
            text: qsTr("SSH key")
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            text: git.hasSshKey ? git.publicSshKey
                                : qsTr("No key yet. Pick the private key your git server knows, or generate one and add its public key to the server.")
            wrapMode: git.hasSshKey ? Text.WrapAnywhere : Text.Wrap
            font.family: git.hasSshKey ? "Monospace" : Theme.fontFamily
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.highlightColor
        }

        ButtonLayout {
            Button {
                text: qsTr("Copy public key")
                visible: git.hasSshKey
                onClicked: {
                    Clipboard.text = git.publicSshKey
                    app.notify(qsTr("Public key copied"))
                }
            }
            Button {
                text: git.hasSshKey ? qsTr("Pick other key") : qsTr("Pick key file")
                onClicked: pageStack.push(sshKeyPicker)
            }
            Button {
                text: git.hasSshKey ? qsTr("Replace with new key") : qsTr("Generate key")
                onClicked: {
                    var generate = function() {
                        git.generateSshKey(function(result) {
                            app.notify(result.ok ? qsTr("SSH key generated") : result.error)
                        })
                    }
                    if (git.hasSshKey)
                        remorse.execute(qsTr("Replacing SSH key"), generate)
                    else
                        generate()
                }
            }
        }

        RemorsePopup {
            id: remorse
        }

        Component {
            id: sshKeyPicker

            FilePickerPage {
                title: qsTr("Select SSH private key")
                showSystemFiles: true
                onSelectedContentPropertiesChanged: {
                    git.importSshKey(selectedContentProperties.filePath, function(result) {
                        app.notify(result.ok ? qsTr("SSH key added") : result.error)
                    })
                }
            }
        }
    }
}
