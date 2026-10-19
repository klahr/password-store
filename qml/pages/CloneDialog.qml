import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Dialog {
    id: dialog

    readonly property bool missingKey: appSettings.gitAuth === "ssh-key" && !git.hasSshKey

    canAccept: urlField.text.trim().length > 0 && git.available && !missingKey

    onAccepted: {
        var url = urlField.text.trim()
        app.withGitPassword(function() {
            git.clone(url, function(result) {
                if (!result.ok) {
                    if (git.authMode === "password")
                        git.forgetPassword()
                    app.notify(app.gitError(result.error))
                } else if (!store.initialized) {
                    app.notify(qsTr("Cloned, but the repository has no .gpg-id"))
                } else {
                    app.notify(pgp.secretKeyCount > 0 ? qsTr("Cloned")
                                                      : qsTr("Cloned, now import your private key in Settings"))
                }
            })
        })
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width

            DialogHeader {
                title: qsTr("Clone store")
                acceptText: qsTr("Clone")
            }

            TextField {
                id: urlField

                width: parent.width
                focus: true
                label: qsTr("Repository URL")
                placeholderText: "git@example.com:me/password-store.git"
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                EnterKey.iconSource: "image://theme/icon-m-enter-close"
                EnterKey.onClicked: focus = false
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !git.available
                text: qsTr("git is not installed. Install it with: devel-su pkcon install git openssh-clients")
                wrapMode: Text.Wrap
                color: Theme.errorColor
            }

            GitAuthSettings { }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: dialog.missingKey
                text: qsTr("Pick or generate an SSH key to clone")
                wrapMode: Text.Wrap
                color: Theme.errorColor
            }
        }

        VerticalScrollDecorator { }
    }
}
