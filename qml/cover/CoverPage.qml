import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    id: cover

    readonly property bool showOtp: appSettings.otpOnCover && !!app.coverOtp
    readonly property bool copied: clipboard.label.length > 0
    property string code
    property int remaining

    function updateOtp() {
        if (!showOtp)
            return
        code = entryParser.totpCode(app.coverOtp.totp)
        remaining = entryParser.totpRemaining(app.coverOtp.totp)
    }

    onShowOtpChanged: updateOtp()

    Timer {
        interval: 1000
        repeat: true
        running: cover.showOtp && cover.status === Cover.Active
        triggeredOnStart: true
        onTriggered: cover.updateOtp()
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingMedium

        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: !cover.showOtp
            source: "image://theme/icon-m-device-lock"
        }

        Label {
            width: parent.width
            visible: !cover.showOtp
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Password Store")
            wrapMode: Text.Wrap
        }

        Label {
            width: parent.width
            visible: cover.showOtp
            horizontalAlignment: Text.AlignHCenter
            text: cover.showOtp ? (app.coverOtp.totp.issuer || app.coverOtp.name) : ""
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryColor
        }

        Label {
            width: parent.width
            visible: cover.showOtp
            horizontalAlignment: Text.AlignHCenter
            text: cover.showOtp ? app.formatCode(cover.code, app.coverOtp.totp.steam) : ""
            font.family: "Monospace"
            font.pixelSize: Theme.fontSizeExtraLarge
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: Theme.fontSizeSmall
        }

        // ProgressCircle swaps its colours whenever the value goes down
        ProgressCircleBase {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: cover.showOtp
            width: Theme.iconSizeMedium
            height: width
            value: cover.showOtp && app.coverOtp.totp.period > 0 ? cover.remaining / app.coverOtp.totp.period : 0
            progressColor: cover.remaining <= 5 ? Theme.errorColor : Theme.highlightColor
            backgroundColor: Theme.highlightDimmerColor

            Label {
                anchors.centerIn: parent
                text: cover.remaining
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
            }
        }

        Label {
            width: parent.width
            visible: cover.copied
            horizontalAlignment: Text.AlignHCenter
            text: clipboard.remaining > 0
                  ? qsTr("%1 copied, clears in %n s", "", clipboard.remaining).arg(clipboard.label)
                  : qsTr("%1 copied").arg(clipboard.label)
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
        }
    }

    CoverActionList {
        enabled: cover.copied

        CoverAction {
            iconSource: "image://theme/icon-cover-cancel"
            onTriggered: clipboard.clear()
        }
        CoverAction {
            iconSource: "image://theme/icon-cover-search"
            onTriggered: app.search()
        }
    }

    CoverActionList {
        enabled: !cover.copied && store.initialized && git.isRepo

        CoverAction {
            iconSource: "image://theme/icon-cover-search"
            onTriggered: app.search()
        }
        CoverAction {
            iconSource: "image://theme/icon-cover-sync"
            onTriggered: app.sync()
        }
    }

    CoverActionList {
        enabled: !cover.copied && store.initialized && !git.isRepo

        CoverAction {
            iconSource: "image://theme/icon-cover-search"
            onTriggered: app.search()
        }
    }
}
