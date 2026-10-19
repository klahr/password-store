import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    property string headerTitle: qsTr("Unlock key")
    property string keyName
    property string errorText
    property bool remembered: true
    property alias passphrase: passphraseField.text

    canAccept: passphrase.length > 0

    Column {
        width: parent.width

        DialogHeader {
            title: dialog.headerTitle
            acceptText: qsTr("Unlock")
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            text: dialog.keyName.length > 0 ? dialog.keyName : qsTr("OpenPGP key")
            wrapMode: Text.Wrap
            color: Theme.highlightColor
        }

        PasswordField {
            id: passphraseField

            width: parent.width
            focus: true
            label: dialog.errorText.length > 0 ? dialog.errorText : qsTr("Passphrase")
            placeholderText: qsTr("Passphrase")
            errorHighlight: dialog.errorText.length > 0 && text.length === 0
            EnterKey.enabled: text.length > 0
            EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            EnterKey.onClicked: dialog.accept()
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            visible: dialog.remembered
            text: appSettings.passphraseTimeout > 0
                  ? qsTr("Remembered for %n minute(s)", "", appSettings.passphraseTimeout)
                  : qsTr("Remembered until the app closes")
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryHighlightColor
        }
    }
}
