import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    // Folder shown, relative to the store root
    property string dir: ""
    property bool isRoot: dir === ""
    property string query: ""
    property Item searchField
    // Start with search only focuses once, when the app opens
    property bool startSearchHandled

    function refresh() {
        var items = query.trim().length > 0
                ? store.search(dir, query.trim(), appSettings.sortOrder, appSettings.showHidden,
                               appSettings.filterRecursively)
                : store.list(dir, appSettings.sortOrder, appSettings.showHidden)

        // Change the model in place: replacing it resets the view, which
        // takes the focus from the search field and closes the keyboard
        var wanted = {}
        for (var i = 0; i < items.length; ++i)
            wanted[items[i].path] = true
        for (i = entries.count - 1; i >= 0; --i) {
            if (!wanted[entries.get(i).path])
                entries.remove(i)
        }
        for (i = 0; i < items.length; ++i) {
            if (i < entries.count && entries.get(i).path === items[i].path)
                continue
            var found = -1
            for (var j = i + 1; j < entries.count; ++j) {
                if (entries.get(j).path === items[i].path) {
                    found = j
                    break
                }
            }
            if (found >= 0)
                entries.move(found, i, 1)
            else
                entries.insert(i, items[i])
        }
    }

    function open(item) {
        if (item.isDir)
            pageStack.push(Qt.resolvedUrl("StorePage.qml"), { dir: item.path })
        else
            pageStack.push(Qt.resolvedUrl("EntryPage.qml"), { path: item.path })
    }

    function copyPassword(path) {
        app.decrypt(path, function(result) {
            if (!result.ok) {
                if (!result.cancelled)
                    app.notify(result.error)
                return
            }
            var entry = entryParser.parse(result.text)
            if (entry.hasPassword)
                app.copy(entry.password, qsTr("Password"))
            else
                app.notify(qsTr("%1 has no password").arg(store.nameOf(path)))
        }, page)
    }

    allowedOrientations: Orientation.All

    onQueryChanged: refresh()
    Component.onCompleted: refresh()

    // Focus taken before the window is active is lost, so wait for both
    // the page and the window
    property bool startSearchPending

    function focusStartSearch() {
        if (!startSearchPending || status !== PageStatus.Active
                || Qt.application.state !== Qt.ApplicationActive || !searchField)
            return
        startSearchPending = false
        searchField.forceActiveFocus()
    }

    onStatusChanged: {
        if (status !== PageStatus.Active || startSearchHandled)
            return
        startSearchHandled = true
        startSearchPending = isRoot && appSettings.searchOnStart && store.initialized
        focusStartSearch()
    }

    Connections {
        target: Qt.application
        onStateChanged: page.focusStartSearch()
    }

    Connections {
        target: store
        onChanged: page.refresh()
    }

    Connections {
        target: appSettings
        onSortOrderChanged: page.refresh()
        onShowHiddenChanged: page.refresh()
        onFilterRecursivelyChanged: page.refresh()
    }

    ListModel {
        id: entries
    }

    SilicaListView {
        id: listView

        anchors.fill: parent
        model: entries

        header: Column {
            width: listView.width

            PageHeader {
                title: page.isRoot ? qsTr("Password Store") : store.nameOf(page.dir)
                description: page.isRoot ? "" : page.dir
            }

            SearchField {
                id: searchField

                width: parent.width
                visible: store.initialized
                placeholderText: page.isRoot ? qsTr("Search") : qsTr("Search in %1").arg(store.nameOf(page.dir))
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                onTextChanged: page.query = text
                EnterKey.iconSource: "image://theme/icon-m-enter-close"
                EnterKey.onClicked: focus = false

                Component.onCompleted: page.searchField = searchField
            }
        }

        PullDownMenu {
            busy: git.busy || pgp.busy

            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                text: qsTr("Clone from git server")
                visible: page.isRoot && !store.initialized
                onClicked: pageStack.push(Qt.resolvedUrl("CloneDialog.qml"))
            }
            MenuItem {
                text: qsTr("Create new store")
                visible: page.isRoot && !store.initialized
                onClicked: pageStack.push(Qt.resolvedUrl("InitStoreDialog.qml"), { dir: "" })
            }
            MenuItem {
                text: qsTr("Sync")
                visible: store.initialized && git.isRepo
                enabled: !git.busy
                onClicked: app.sync()
            }
            MenuItem {
                text: qsTr("New folder")
                visible: store.initialized
                onClicked: pageStack.push(Qt.resolvedUrl("FolderDialog.qml"), { dir: page.dir })
            }
            MenuItem {
                text: qsTr("New password")
                visible: store.initialized
                onClicked: pageStack.push(Qt.resolvedUrl("EditDialog.qml"), { dir: page.dir })
            }
        }

        delegate: ListItem {
            id: listItem

            property var item: model

            contentHeight: Theme.itemSizeMedium
            menu: contextMenu
            onClicked: page.open(item)

            function remove() {
                remorseDelete(function() {
                    store.remove(item.path, app.storeResult(qsTr("Deleted %1").arg(item.name)))
                })
            }

            Icon {
                id: icon

                x: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                source: listItem.item.isDir ? "image://theme/icon-m-file-folder" : "image://theme/icon-m-device-lock"
                highlighted: listItem.highlighted
            }

            Column {
                anchors {
                    left: icon.right
                    leftMargin: Theme.paddingMedium
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }

                Label {
                    width: parent.width
                    text: listItem.item.name
                    truncationMode: TruncationMode.Fade
                    color: listItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    visible: text.length > 0 && listItem.item.folder !== page.dir
                    text: listItem.item.folder
                    font.pixelSize: Theme.fontSizeExtraSmall
                    truncationMode: TruncationMode.Fade
                    color: listItem.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                }
            }

            Component {
                id: contextMenu

                ContextMenu {
                    MenuItem {
                        text: qsTr("Copy password")
                        visible: !listItem.item.isDir
                        onClicked: page.copyPassword(listItem.item.path)
                    }
                    MenuItem {
                        text: qsTr("Move or rename")
                        onClicked: pageStack.push(Qt.resolvedUrl("MoveDialog.qml"), { path: listItem.item.path })
                    }
                    MenuItem {
                        text: qsTr("Delete")
                        onClicked: listItem.remove()
                    }
                }
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0
            text: !store.initialized && page.isRoot
                  ? qsTr("No password store")
                  : page.query.length > 0 ? qsTr("No matches") : qsTr("Empty folder")
            hintText: !store.initialized && page.isRoot
                      ? qsTr("Pull down to clone one from a git server or create a new one")
                      : pgp.secretKeyCount === 0 && page.isRoot
                        ? qsTr("Add your OpenPGP key in Settings to decrypt passwords")
                        : page.query.length > 0 ? "" : qsTr("Pull down to add a password")
        }

        VerticalScrollDecorator { }
    }
}
