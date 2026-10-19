import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0

Page {
    id: page

    property var keys: []
    property bool loaded

    function refresh() {
        pgp.listKeys(function(result) {
            loaded = true
            if (result.ok)
                keys = result.keys
            else
                app.notify(result.error)
        })
    }

    function importKeys(source, passphrase, fromClipboard) {
        pgp.importKeys(source, passphrase || "", function(result) {
            // Don't leave a private key in the clipboard
            if (result.ok && fromClipboard && Clipboard.text === source)
                Clipboard.text = ""
            if (result.needPassphrase) {
                // A key file encrypted with gpg --symmetric
                pageStack.completeAnimation()
                var dialog = pageStack.push(Qt.resolvedUrl("PassphraseDialog.qml"), {
                    headerTitle: qsTr("Decrypt key file"),
                    keyName: source.indexOf("\n") < 0 ? source.substring(source.lastIndexOf("/") + 1)
                                                       : qsTr("Key from clipboard"),
                    errorText: result.error,
                    remembered: false
                })
                dialog.accepted.connect(function() { page.importKeys(source, dialog.passphrase, fromClipboard) })
            } else if (result.ok) {
                app.notify(qsTr("Imported %n key(s)", "", result.imported))
            } else {
                app.notify(result.error)
            }
        })
    }

    allowedOrientations: Orientation.All
    Component.onCompleted: refresh()

    Connections {
        target: pgp
        onKeysChanged: page.refresh()
    }

    Component {
        id: filePicker

        FilePickerPage {
            title: qsTr("Select key file")
            nameFilters: ["*.asc", "*.gpg", "*.key", "*.pgp", "*.txt"]
            onSelectedContentPropertiesChanged: page.importKeys(selectedContentProperties.filePath)
        }
    }

    SilicaListView {
        id: listView

        anchors.fill: parent
        model: page.keys

        header: PageHeader {
            title: qsTr("OpenPGP keys")
        }

        PullDownMenu {
            busy: pgp.busy

            MenuItem {
                text: qsTr("Generate key")
                onClicked: pageStack.push(Qt.resolvedUrl("GenerateKeyDialog.qml"))
            }
            MenuItem {
                text: qsTr("Import from clipboard")
                enabled: Clipboard.hasText && Clipboard.text.indexOf("-----BEGIN PGP") >= 0
                onClicked: page.importKeys(Clipboard.text, "", true)
            }
            MenuItem {
                text: qsTr("Import from file")
                onClicked: pageStack.push(filePicker)
            }
        }

        delegate: ListItem {
            id: listItem

            contentHeight: column.height + 2 * Theme.paddingMedium
            menu: contextMenu

            Column {
                id: column

                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    width: parent.width
                    text: modelData.uid || modelData.keyId
                    truncationMode: TruncationMode.Fade
                    color: listItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    text: modelData.keyId + " · " + modelData.algorithm
                          + (modelData.secret ? " · " + qsTr("private") : "")
                          + (modelData.usable ? "" : " · " + qsTr("expired or revoked"))
                    font.pixelSize: Theme.fontSizeExtraSmall
                    font.family: "Monospace"
                    truncationMode: TruncationMode.Fade
                    color: listItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                }
            }

            Component {
                id: contextMenu

                ContextMenu {
                    MenuItem {
                        text: qsTr("Copy public key")
                        onClicked: pgp.exportPublicKey(modelData.fingerprint, function(result) {
                            if (result.ok) {
                                Clipboard.text = result.armor
                                app.notify(qsTr("Public key copied"))
                            } else {
                                app.notify(result.error)
                            }
                        })
                    }
                    MenuItem {
                        text: qsTr("Copy fingerprint")
                        onClicked: {
                            Clipboard.text = modelData.fingerprint
                            app.notify(qsTr("Fingerprint copied"))
                        }
                    }
                    MenuItem {
                        text: qsTr("Delete")
                        onClicked: {
                            var fingerprint = modelData.fingerprint
                            listItem.remorseDelete(function() {
                                pgp.removeKey(fingerprint, function(result) {
                                    if (!result.ok)
                                        app.notify(result.error)
                                })
                            })
                        }
                    }
                }
            }
        }

        ViewPlaceholder {
            enabled: page.loaded && listView.count === 0
            text: qsTr("No keys")
            hintText: qsTr("Import the private key your password store is encrypted for, or generate a new one")
        }

        VerticalScrollDecorator { }
    }
}
