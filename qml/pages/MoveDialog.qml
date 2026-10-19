import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    property string path
    property Item entryPage
    readonly property bool isFolder: store.isFolder(path)
    readonly property string newPath: store.join(folderBox.folder, nameField.text.trim())
    readonly property string pathError: {
        var error = store.validatePath(nameField.text.trim())
        if (error.length === 0 && newPath !== path && (store.exists(newPath) || store.isFolder(newPath)))
            error = qsTr("%1 already exists").arg(newPath)
        return error
    }

    canAccept: pathError.length === 0 && newPath !== path

    onAccepted: {
        var from = path
        var target = newPath
        var entryPage = dialog.entryPage
        var move = function() {
            store.move(from, target, function(result) {
                // Re-encrypting for the new folder's keys needs them decrypted
                if (result.needPassphrase) {
                    app.askPassphrase(result, move)
                    return
                }
                app.storeResult("")(result)
                if (result.ok && app.isAlive(entryPage))
                    entryPage.path = target
            })
        }
        move()
    }

    Column {
        width: parent.width

        DialogHeader {
            title: dialog.isFolder ? qsTr("Move folder") : qsTr("Move password")
            acceptText: qsTr("Move")
        }

        TextField {
            id: nameField

            width: parent.width
            label: dialog.pathError.length > 0 ? dialog.pathError : qsTr("Name")
            placeholderText: qsTr("Name")
            errorHighlight: dialog.pathError.length > 0
            text: store.nameOf(dialog.path)
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            EnterKey.iconSource: "image://theme/icon-m-enter-close"
            EnterKey.onClicked: focus = false
        }

        ValueButton {
            id: folderBox

            property string folder: store.folderOf(dialog.path)

            label: qsTr("Folder")
            value: folder.length > 0 ? folder : qsTr("Store root")
            onClicked: {
                var picker = pageStack.push(Qt.resolvedUrl("FolderPickerPage.qml"), { exclude: dialog.isFolder ? dialog.path : "" })
                picker.selected.connect(function(folder) { folderBox.folder = folder })
            }
        }
    }
}
