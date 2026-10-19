import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    readonly property string userId: emailField.text.trim().length > 0
                                     ? nameField.text.trim() + " <" + emailField.text.trim() + ">"
                                     : nameField.text.trim()

    canAccept: nameField.text.trim().length > 0 && passphraseField.text === repeatField.text

    onAccepted: {
        pgp.generateKey(userId, passphraseField.text, function(result) {
            app.notify(result.ok ? qsTr("Key generated") : result.error)
        })
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width

            DialogHeader {
                title: qsTr("Generate key")
                acceptText: qsTr("Generate")
            }

            TextField {
                id: nameField
                width: parent.width
                focus: true
                label: qsTr("Name")
                placeholderText: label
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: emailField.focus = true
            }

            TextField {
                id: emailField
                width: parent.width
                label: qsTr("Email")
                placeholderText: label
                inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoPredictiveText
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: passphraseField.focus = true
            }

            PasswordField {
                id: passphraseField
                width: parent.width
                label: qsTr("Passphrase")
                placeholderText: label
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: repeatField.focus = true
            }

            PasswordField {
                id: repeatField
                width: parent.width
                label: text === passphraseField.text ? qsTr("Repeat passphrase") : qsTr("Passphrases differ")
                placeholderText: qsTr("Repeat passphrase")
                errorHighlight: text !== passphraseField.text
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Makes an ed25519 key with a cv25519 encryption subkey. Without a passphrase, anyone with this phone's files can read your passwords.")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }
        }
    }
}
