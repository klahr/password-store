import QtQuick 2.0
import Sailfish.Silica 1.0

// A labelled value that copies itself when tapped. Secret values are
// hidden until revealed.
BackgroundItem {
    id: root

    property string label
    property string value
    property bool secret
    property bool revealed: !secret
    property bool monospace: secret

    width: parent ? parent.width : implicitWidth
    height: Math.max(Theme.itemSizeMedium, column.height + 2 * Theme.paddingMedium)
    onClicked: app.copy(value, label)

    Column {
        id: column

        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            right: revealButton.visible ? revealButton.left : parent.right
            rightMargin: revealButton.visible ? 0 : Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }

        Label {
            width: parent.width
            text: root.label
            font.pixelSize: Theme.fontSizeExtraSmall
            color: root.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
            truncationMode: TruncationMode.Fade
        }
        Label {
            width: parent.width
            text: root.revealed ? root.value : "••••••••"
            textFormat: Text.PlainText
            wrapMode: Text.WrapAnywhere
            font.family: root.monospace ? "Monospace" : Theme.fontFamily
            color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
        }
    }

    IconButton {
        id: revealButton

        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin - Theme.paddingMedium
            verticalCenter: parent.verticalCenter
        }
        visible: root.secret
        icon.source: "image://theme/icon-splus-" + (root.revealed ? "hide-password" : "show-password")
        onClicked: root.revealed = !root.revealed
    }
}
