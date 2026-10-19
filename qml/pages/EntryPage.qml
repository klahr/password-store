import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: page

    property string path
    property string content
    property var entry
    property bool loading: true
    property string errorText

    function load() {
        loading = true
        errorText = ""
        app.decrypt(path, function(result) {
            loading = false
            // Not unlocking the key goes back to the list
            if (result.cancelled) {
                close()
                return
            }
            if (!result.ok) {
                errorText = result.error
                return
            }
            show(result.text)
            if (appSettings.copyOnDecrypt && entry.hasPassword)
                app.copy(entry.password, qsTr("Password"))
        }, page)
    }

    // pop(page) would pop the pages above this one, not this one. That
    // includes the passphrase dialog, which may still be closing.
    function close() {
        if (!app.isAlive(page))
            return
        pageStack.completeAnimation()
        pageStack.pop(pageStack.previousPage(page))
    }

    function show(text) {
        content = text
        entry = entryParser.parse(text)
        if (appSettings.otpOnCover && entry.hasTotp)
            app.coverOtp = { name: store.nameOf(path), totp: entry.totp }
    }

    allowedOrientations: Orientation.All
    Component.onCompleted: load()

    // Like Android Password Store, close the entry when the clipboard clears
    Timer {
        interval: appSettings.showTime * 1000
        running: appSettings.showTime > 0 && page.entry !== undefined && page.status === PageStatus.Active
        onTriggered: page.close()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            busy: pgp.busy

            MenuItem {
                text: qsTr("Delete")
                onClicked: {
                    var path = page.path
                    remorse.execute(qsTr("Deleting %1").arg(store.nameOf(path)), function() {
                        store.remove(path, app.storeResult(""))
                        page.close()
                    })
                }
            }
            MenuItem {
                text: qsTr("Move or rename")
                onClicked: pageStack.push(Qt.resolvedUrl("MoveDialog.qml"), { path: page.path, entryPage: page })
            }
            MenuItem {
                text: page.entry ? qsTr("Edit") : qsTr("Decrypt")
                onClicked: {
                    if (!page.entry) {
                        page.load()
                        return
                    }
                    pageStack.push(Qt.resolvedUrl("EditDialog.qml"), {
                        path: page.path,
                        content: page.content,
                        entryPage: page
                    })
                }
            }
        }

        Column {
            id: column

            width: parent.width

            PageHeader {
                title: store.nameOf(page.path)
                description: store.folderOf(page.path)
            }

            FieldItem {
                visible: !!page.entry && page.entry.hasPassword
                label: qsTr("Password")
                value: page.entry ? page.entry.password : ""
                secret: true
                revealed: appSettings.showPassword
            }

            OtpItem {
                visible: !!page.entry && page.entry.hasTotp
                totp: page.entry && page.entry.hasTotp ? page.entry.totp : ({})
            }

            FieldItem {
                visible: !!page.entry && page.entry.username.length > 0
                label: qsTr("Username")
                value: page.entry ? page.entry.username : ""
            }

            Repeater {
                model: page.entry ? page.entry.fields : []

                FieldItem {
                    label: modelData.key
                    value: modelData.value
                }
            }

            SectionHeader {
                visible: notes.visible
                text: qsTr("Notes")
            }

            LinkedLabel {
                id: notes

                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !!page.entry && page.entry.extraContent.length > 0
                plainText: page.entry ? page.entry.extraContent : ""
                wrapMode: Text.Wrap
            }
        }

        ViewPlaceholder {
            enabled: !page.loading && !page.entry
            text: qsTr("Could not decrypt")
            hintText: page.errorText
        }

        VerticalScrollDecorator { }
    }

    RemorsePopup {
        id: remorse
    }

    BusyLabel {
        running: page.loading
        text: qsTr("Decrypting")
    }
}
