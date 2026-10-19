import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0

Dialog {
    id: dialog

    // The entry being edited, empty for a new one
    property string path
    // Folder a new entry goes in
    property string dir
    property string content
    property Item entryPage
    // Path typed before, when saving failed
    property string initialName

    readonly property string startPath: initialName.length > 0 ? initialName : path
    property string folder: startPath.length > 0 ? store.folderOf(startPath) : dir
    readonly property var parts: entryParser.splitForEdit(content)
    readonly property string newPath: store.join(folder, nameField.text.trim())
    readonly property string pathError: {
        var error = store.validatePath(newPath)
        if (error.length === 0 && newPath !== path && store.exists(newPath))
            error = qsTr("%1 already exists").arg(newPath)
        return error
    }

    canAccept: pathError.length === 0

    // Puts uri in place of the one-time code line, or adds it
    function setOtp(uri) {
        var lines = extraArea.text.length > 0 ? extraArea.text.split("\n") : []
        for (var i = 0; i < lines.length; ++i) {
            if (lines[i].indexOf("otpauth://") === 0 || /^totp:/i.test(lines[i])) {
                lines[i] = uri
                extraArea.text = lines.join("\n")
                app.notify(qsTr("One-time code replaced"))
                return
            }
        }
        lines.push(uri)
        extraArea.text = lines.join("\n")
        app.notify(qsTr("One-time code added"))
    }

    function scanned(result) {
        if (!result.ok) {
            app.notify(result.error)
            return
        }
        var otp = app.otpFromQr(result.texts)
        if (otp.uri.length > 0)
            setOtp(otp.uri)
        else
            app.notify(otp.message)
    }

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

    Component {
        id: imagePicker

        ImagePickerPage {
            onSelectedContentPropertiesChanged: qrScanner.scanFile(selectedContentProperties.filePath, dialog.scanned)
        }
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

            ValueButton {
                label: qsTr("Folder")
                value: dialog.folder.length > 0 ? dialog.folder : qsTr("Store root")
                onClicked: {
                    var picker = pageStack.push(Qt.resolvedUrl("FolderPickerPage.qml"), { title: qsTr("Folder") })
                    picker.selected.connect(function(folder) { dialog.folder = folder })
                }
            }

            TextField {
                id: nameField

                width: parent.width
                label: dialog.pathError.length > 0 && text.length > 0 ? dialog.pathError : qsTr("Name")
                placeholderText: qsTr("Name")
                errorHighlight: dialog.pathError.length > 0 && text.length > 0
                text: dialog.startPath.length > 0 ? store.nameOf(dialog.startPath) : ""
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                focus: dialog.path.length === 0
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: passwordField.focus = true
            }

            PasswordField {
                id: passwordField

                width: parent.width
                label: qsTr("Password")
                placeholderText: label
                text: dialog.parts.password
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: extraArea.focus = true
            }

            Item {
                width: 1
                height: Theme.paddingMedium
            }

            ButtonLayout {
                Button {
                    text: qsTr("Generate password")
                    onClicked: {
                        var generator = pageStack.push(Qt.resolvedUrl("GeneratorDialog.qml"))
                        generator.accepted.connect(function() {
                            passwordField.text = generator.password
                            passwordField.echoMode = TextInput.Normal
                        })
                    }
                }
            }

            Item {
                width: 1
                height: Theme.paddingLarge
            }

            TextArea {
                id: extraArea

                width: parent.width
                label: qsTr("Username, URL and notes")
                placeholderText: label
                text: dialog.parts.rest
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("One per line, like login: name or url: https://example.com")
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }

            SectionHeader {
                text: qsTr("One-time code")
            }

            ButtonLayout {
                Button {
                    text: qsTr("Scan QR code")
                    onClicked: {
                        var scan = pageStack.push(Qt.resolvedUrl("ScanPage.qml"))
                        scan.found.connect(dialog.setOtp)
                    }
                }
                Button {
                    text: qsTr("QR code from picture")
                    onClicked: pageStack.push(imagePicker)
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                topPadding: Theme.paddingLarge
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
