import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    property alias password: passwordField.text

    canAccept: password.length > 0

    Column {
        width: parent.width

        DialogHeader {
            title: qsTr("Git server password")
            acceptText: qsTr("Connect")
        }

        PasswordField {
            id: passwordField

            width: parent.width
            focus: true
            label: appSettings.gitUsername.length > 0 ? qsTr("Password for %1").arg(appSettings.gitUsername)
                                                      : qsTr("Password or access token")
            placeholderText: qsTr("Password")
            EnterKey.enabled: text.length > 0
            EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            EnterKey.onClicked: dialog.accept()
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            text: qsTr("Kept in memory until the app closes")
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryHighlightColor
        }
    }
}
