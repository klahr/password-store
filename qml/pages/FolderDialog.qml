import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    property string dir
    readonly property string newPath: store.join(dir, nameField.text.trim())
    readonly property string pathError: {
        var error = store.validatePath(nameField.text.trim())
        if (error.length === 0 && store.isFolder(newPath))
            error = qsTr("%1 already exists").arg(newPath)
        return error
    }

    canAccept: pathError.length === 0
    onAccepted: store.createFolder(newPath, app.storeResult(""))

    Column {
        width: parent.width

        DialogHeader {
            title: qsTr("New folder")
            acceptText: qsTr("Create")
        }

        TextField {
            id: nameField

            width: parent.width
            focus: true
            label: dialog.pathError.length > 0 && text.length > 0 ? dialog.pathError
                                                                  : dialog.dir.length > 0 ? qsTr("Folder in %1").arg(dialog.dir)
                                                                                          : qsTr("Folder")
            placeholderText: qsTr("Name")
            errorHighlight: dialog.pathError.length > 0 && text.length > 0
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            EnterKey.enabled: dialog.canAccept
            EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            EnterKey.onClicked: dialog.accept()
        }
    }
}
