import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: page

    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            busy: git.busy
            visible: git.available

            MenuItem {
                text: qsTr("Create git repository")
                visible: !git.isRepo
                onClicked: git.init(app.gitResult(qsTr("Git repository created")))
            }
            MenuItem {
                text: qsTr("History")
                visible: git.isRepo
                onClicked: pageStack.push(Qt.resolvedUrl("GitLogPage.qml"))
            }
            MenuItem {
                text: qsTr("Push")
                visible: git.isRepo
                enabled: !git.busy
                onClicked: app.push()
            }
            MenuItem {
                text: qsTr("Pull")
                visible: git.isRepo
                enabled: !git.busy
                onClicked: app.pull()
            }
            MenuItem {
                text: qsTr("Sync")
                visible: git.isRepo
                enabled: !git.busy
                onClicked: app.sync()
            }
        }

        Column {
            id: column

            width: parent.width

            PageHeader {
                title: qsTr("Git server")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: !git.available
                text: qsTr("git is not installed. Install it with: devel-su pkcon install git openssh-clients")
                wrapMode: Text.Wrap
                color: Theme.errorColor
            }

            TextField {
                id: urlField

                width: parent.width
                visible: git.isRepo
                label: qsTr("Remote URL, enter to save")
                placeholderText: qsTr("Remote URL")
                text: git.remoteUrl
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                EnterKey.enabled: text.trim().length > 0
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: {
                    focus = false
                    git.setRemoteUrl(text.trim(), app.gitResult(qsTr("Remote saved")))
                }
            }

            GitAuthSettings { }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: appSettings.gitAuth === "password"
                enabled: !git.needsPassword
                text: qsTr("Forget password")
                onClicked: git.forgetPassword()
            }

            SectionHeader {
                text: qsTr("Commits")
            }

            TextField {
                width: parent.width
                label: qsTr("Author name")
                placeholderText: label
                text: appSettings.gitAuthorName
                onTextChanged: appSettings.gitAuthorName = text.trim()
            }

            TextField {
                width: parent.width
                label: qsTr("Author email")
                placeholderText: label
                text: appSettings.gitAuthorEmail
                inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoPredictiveText
                onTextChanged: appSettings.gitAuthorEmail = text.trim()
            }

            TextSwitch {
                text: qsTr("Rebase when pulling")
                description: qsTr("Otherwise pulling merges")
                checked: appSettings.rebaseOnPull
                onCheckedChanged: appSettings.rebaseOnPull = checked
            }
        }

        VerticalScrollDecorator { }
    }
}
