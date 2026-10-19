import QtQuick 2.0
import Sailfish.Silica 1.0

// Writes the .gpg-id of a folder, like pass init
Dialog {
    id: dialog

    property string dir
    property var keys: []
    property var selected: ({})
    property int selectedCount
    readonly property var current: store.recipients(dir)

    function toggle(fingerprint, on) {
        // A new object, so bindings on selected update
        var copy = {}
        for (var key in selected)
            copy[key] = true
        if (on)
            copy[fingerprint] = true
        else
            delete copy[fingerprint]
        selected = copy
        selectedCount = Object.keys(copy).length
    }

    canAccept: selectedCount > 0

    Component.onCompleted: {
        pgp.listKeys(function(result) {
            if (!result.ok) {
                app.notify(result.error)
                return
            }
            var keys = result.keys
            for (var i = 0; i < keys.length; ++i) {
                var key = keys[i]
                for (var j = 0; j < current.length; ++j) {
                    var id = current[j].toUpperCase().replace(/^0X/, "")
                    if (key.fingerprint === id || key.keyId === id)
                        toggle(key.fingerprint, true)
                }
            }
            // A new store most likely uses the only private key there is
            if (current.length === 0 && pgp.secretKeyCount === 1) {
                for (i = 0; i < keys.length; ++i) {
                    if (keys[i].secret)
                        toggle(keys[i].fingerprint, true)
                }
            }
            dialog.keys = keys
        })
    }

    onAccepted: {
        var ids = Object.keys(selected)
        var initGit = gitSwitch.visible && gitSwitch.checked
        store.init(dir, ids, function(result) {
            app.storeResult(qsTr("Store keys set"))(result)
            if (result.ok && initGit)
                git.init(app.gitResult(qsTr("Git repository created")))
        })
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width

            DialogHeader {
                title: qsTr("Keys for store")
                acceptText: qsTr("Save")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("New and edited passwords in %1 are encrypted for the selected keys. Existing passwords are not re-encrypted.")
                      .arg(dialog.dir.length > 0 ? dialog.dir : qsTr("the store"))
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.highlightColor
            }

            Repeater {
                model: dialog.keys

                TextSwitch {
                    text: modelData.uid || modelData.keyId
                    description: modelData.keyId + (modelData.secret ? " · " + qsTr("private") : "")
                    enabled: modelData.usable
                    checked: !!dialog.selected[modelData.fingerprint]
                    onClicked: dialog.toggle(modelData.fingerprint, checked)
                }
            }

            ButtonLayout {
                Button {
                    text: qsTr("Manage keys")
                    onClicked: pageStack.push(Qt.resolvedUrl("KeysPage.qml"))
                }
            }

            TextSwitch {
                id: gitSwitch

                visible: !git.isRepo && git.available
                text: qsTr("Track changes with git")
                checked: true
            }
        }

        VerticalScrollDecorator { }
    }

    Connections {
        target: pgp
        onKeysChanged: pgp.listKeys(function(result) { if (result.ok) dialog.keys = result.keys })
    }
}
