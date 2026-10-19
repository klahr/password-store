import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    property bool loaded

    allowedOrientations: Orientation.All

    Component.onCompleted: {
        git.log(function(result) {
            loaded = true
            if (result.ok)
                listView.model = result.entries
            else
                app.notify(result.error)
        })
    }

    SilicaListView {
        id: listView

        anchors.fill: parent
        header: PageHeader { title: qsTr("History") }

        delegate: ListItem {
            contentHeight: column.height + 2 * Theme.paddingMedium

            Column {
                id: column

                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    width: parent.width
                    text: modelData.message
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeSmall
                }
                Label {
                    width: parent.width
                    text: modelData.hash.substring(0, 8) + " · " + modelData.author + " · "
                          + Format.formatDate(modelData.time, Formatter.DurationElapsed)
                    font.pixelSize: Theme.fontSizeExtraSmall
                    truncationMode: TruncationMode.Fade
                    color: Theme.secondaryColor
                }
            }
        }

        ViewPlaceholder {
            enabled: page.loaded && listView.count === 0
            text: qsTr("No commits")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: !page.loaded
        }

        VerticalScrollDecorator { }
    }
}
