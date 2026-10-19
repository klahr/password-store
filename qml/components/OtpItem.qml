import QtQuick 2.0
import Sailfish.Silica 1.0

// The current TOTP code with the time left until it changes
BackgroundItem {
    id: root

    property var totp
    property string code
    property int remaining

    function update() {
        code = entryParser.totpCode(totp)
        remaining = entryParser.totpRemaining(totp)
    }

    width: parent ? parent.width : implicitWidth
    height: Theme.itemSizeLarge
    onClicked: app.copy(code, qsTr("One-time code"))
    onTotpChanged: update()

    Timer {
        interval: 1000
        repeat: true
        running: root.visible && Qt.application.active
        triggeredOnStart: true
        onTriggered: root.update()
    }

    Column {
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            right: progress.left
            rightMargin: Theme.paddingLarge
            verticalCenter: parent.verticalCenter
        }

        Label {
            width: parent.width
            text: root.totp && root.totp.issuer ? qsTr("One-time code for %1").arg(root.totp.issuer)
                                                : qsTr("One-time code")
            font.pixelSize: Theme.fontSizeExtraSmall
            color: root.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
            truncationMode: TruncationMode.Fade
        }
        Label {
            text: app.formatCode(root.code, root.totp && root.totp.steam)
            font.pixelSize: Theme.fontSizeExtraLarge
            font.family: "Monospace"
            color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
        }
    }

    // ProgressCircle swaps its colours whenever the value goes down, which a
    // countdown does every second
    ProgressCircleBase {
        id: progress

        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        width: Theme.iconSizeMedium
        height: width
        value: root.totp && root.totp.period > 0 ? root.remaining / root.totp.period : 0
        progressColor: root.remaining <= 5 ? Theme.errorColor : Theme.highlightColor
        backgroundColor: Theme.highlightDimmerColor

        Label {
            anchors.centerIn: parent
            text: root.remaining
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
        }
    }
}
