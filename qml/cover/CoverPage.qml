import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingMedium

        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            source: "image://theme/icon-m-device-lock"
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Password Store")
            wrapMode: Text.Wrap
        }

        Label {
            width: parent.width
            visible: clipboard.remaining > 0
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("%1 copied, clears in %n s", "", clipboard.remaining).arg(clipboard.label)
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
        }
    }

    CoverActionList {
        enabled: clipboard.remaining > 0

        CoverAction {
            iconSource: "image://theme/icon-cover-cancel"
            onTriggered: clipboard.clear()
        }
    }

    CoverActionList {
        enabled: clipboard.remaining === 0 && git.isRepo && store.initialized

        CoverAction {
            iconSource: "image://theme/icon-cover-sync"
            onTriggered: app.sync()
        }
    }
}
