import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    // The entry being edited, empty for a new one
    property string path
    // Folder a new entry goes in
    property string dir
    property string content
    property Item entryPage
    // Name typed before, when saving failed
    property string initialName

    readonly property var parts: entryParser.splitForEdit(content)
    readonly property string newPath: nameField.text.trim()
    readonly property string pathError: {
        var error = store.validatePath(newPath)
        if (error.length === 0 && newPath !== path && store.exists(newPath))
            error = qsTr("%1 already exists").arg(newPath)
        return error
    }

    canAccept: pathError.length === 0

    onAccepted: {
        var text = entryParser.joinForEdit(passwordField.text, extraArea.text, parts.line, parts.prefix)
        var savedPath = newPath
        var originalPath = path
        var entryPage = dialog.entryPage
        var folder = dir
        store.save(savedPath, text, originalPath, function(result) {
            if (!result.ok) {
                app.notify(result.error)
                // Give the edits back instead of losing them
                pageStack.completeAnimation()
                pageStack.push(Qt.resolvedUrl("EditDialog.qml"), {
                    path: originalPath,
                    dir: folder,
                    content: text,
                    entryPage: entryPage,
                    initialName: savedPath
                })
                return
            }
            app.storeResult(originalPath.length === 0 ? qsTr("Saved %1").arg(savedPath) : "")(result)
            if (entryPage) {
                entryPage.path = savedPath
                entryPage.show(text)
            }
        })
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width

            DialogHeader {
                title: dialog.path.length > 0 ? qsTr("Edit") : qsTr("New password")
                acceptText: qsTr("Save")
            }

            TextField {
                id: nameField

                width: parent.width
                label: dialog.pathError.length > 0 ? dialog.pathError : qsTr("Name, use / for folders")
                placeholderText: qsTr("Name")
                errorHighlight: dialog.pathError.length > 0 && text.length > 0
                text: dialog.initialName.length > 0 ? dialog.initialName
                                                    : dialog.path.length > 0 ? dialog.path
                                                                             : (dialog.dir.length > 0 ? dialog.dir + "/" : "")
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                focus: dialog.path.length === 0
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: passwordField.focus = true
            }

            Row {
                width: parent.width

                PasswordField {
                    id: passwordField

                    width: parent.width - generateButton.width - Theme.horizontalPageMargin
                    label: qsTr("Password")
                    placeholderText: label
                    text: dialog.parts.password
                    showEchoModeToggle: true
                    EnterKey.iconSource: "image://theme/icon-m-enter-next"
                    EnterKey.onClicked: extraArea.focus = true
                }

                IconButton {
                    id: generateButton

                    anchors.verticalCenter: passwordField.verticalCenter
                    icon.source: "image://theme/icon-m-refresh"
                    onClicked: {
                        var generator = pageStack.push(Qt.resolvedUrl("GeneratorDialog.qml"))
                        generator.accepted.connect(function() {
                            passwordField.text = generator.password
                            passwordField.echoMode = TextInput.Normal
                        })
                    }
                }
            }

            TextArea {
                id: extraArea

                width: parent.width
                label: qsTr("Username, URL, notes")
                placeholderText: qsTr("login: name\nurl: https://example.com\notpauth://totp/...")
                text: dialog.parts.rest
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: store.recipients(dialog.newPath).length === 0 && dialog.pathError.length === 0
                text: qsTr("No .gpg-id covers this folder, so it can't be encrypted")
                color: Theme.errorColor
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        VerticalScrollDecorator { }
    }
}
