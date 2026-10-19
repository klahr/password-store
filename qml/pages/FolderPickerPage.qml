import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    property string title: qsTr("Move to")
    // A folder that can't be picked, with everything below it
    property string exclude

    signal selected(string folder)

    allowedOrientations: Orientation.All

    SilicaListView {
        anchors.fill: parent
        header: PageHeader { title: page.title }
        model: store.folders().filter(function(folder) {
            return page.exclude.length === 0
                   || (folder !== page.exclude && folder.indexOf(page.exclude + "/") !== 0)
        })

        delegate: BackgroundItem {
            onClicked: {
                page.selected(modelData)
                pageStack.pop()
            }

            Icon {
                id: icon
                x: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                source: "image://theme/icon-m-file-folder"
            }
            Label {
                anchors {
                    left: icon.right
                    leftMargin: Theme.paddingMedium
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                text: modelData.length > 0 ? modelData : qsTr("Store root")
                truncationMode: TruncationMode.Fade
                color: highlighted ? Theme.highlightColor : Theme.primaryColor
            }
        }

        VerticalScrollDecorator { }
    }
}
