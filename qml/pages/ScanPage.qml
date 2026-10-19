import QtQuick 2.0
import QtMultimedia 5.5
import Sailfish.Silica 1.0

// Scans QR codes with the camera until one holds a one-time code setup
Page {
    id: page

    property bool done
    property string message

    signal found(string uri)

    function handle(texts) {
        if (done || texts.length === 0)
            return
        var otp = app.otpFromQr(texts)
        if (otp.uri.length === 0) {
            message = otp.message
            return
        }
        done = true
        found(otp.uri)
        pageStack.pop()
    }

    allowedOrientations: Orientation.Portrait

    // Turns the viewfinder upright on a portrait page, as in Peaked
    readonly property int viewfinderRotation: (270 - camera.orientation + 360) % 360

    Camera {
        id: camera

        captureMode: Camera.CaptureViewfinder
        focus.focusMode: Camera.FocusContinuous
    }

    VideoOutput {
        id: videoOutput

        anchors.fill: parent
        source: camera
        orientation: page.viewfinderRotation
        fillMode: VideoOutput.PreserveAspectFit
    }

    // A frame is decoded while the next is taken, quirc doesn't mind the turn
    Timer {
        interval: 300
        repeat: true
        running: camera.cameraStatus === Camera.ActiveStatus && page.status === PageStatus.Active
                 && !page.done && !qrScanner.busy
        onTriggered: {
            videoOutput.grabToImage(function(result) {
                qrScanner.scanImage(result.image, function(scanned) { page.handle(scanned.texts) })
            })
        }
    }

    PageHeader {
        title: qsTr("Scan QR code")
    }

    BusyIndicator {
        anchors.centerIn: parent
        size: BusyIndicatorSize.Large
        running: camera.cameraStatus !== Camera.ActiveStatus && camera.errorCode === Camera.NoError
    }

    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: hint.height + 4 * Theme.paddingLarge
        color: Theme.rgba("black", 0.6)

        Label {
            id: hint

            anchors.centerIn: parent
            width: parent.width - 2 * Theme.horizontalPageMargin
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: camera.errorCode !== Camera.NoError ? camera.errorString
                                                      : page.message.length > 0 ? page.message
                                                                                : qsTr("Point the camera at the QR code for the one-time code")
            color: camera.errorCode !== Camera.NoError || page.message.length > 0 ? Theme.errorColor
                                                                                  : Theme.highlightColor
        }
    }
}
