import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    property string password
    property bool ready

    function generate() {
        if (!ready)
            return
        password = generator.classic(lengthSlider.value, digitsSwitch.checked, upperSwitch.checked,
                                     lowerSwitch.checked, symbolsSwitch.checked, ambiguousSwitch.checked)
    }

    canAccept: password.length > 0
    Component.onCompleted: {
        ready = true
        generate()
    }

    onAccepted: {
        appSettings.classicLength = lengthSlider.value
        appSettings.classicDigits = digitsSwitch.checked
        appSettings.classicUppercase = upperSwitch.checked
        appSettings.classicLowercase = lowerSwitch.checked
        appSettings.classicSymbols = symbolsSwitch.checked
        appSettings.classicAvoidAmbiguous = ambiguousSwitch.checked
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width

            DialogHeader {
                title: qsTr("Generate password")
                acceptText: qsTr("Use")
            }

            BackgroundItem {
                width: parent.width
                height: Math.max(Theme.itemSizeMedium, passwordLabel.height + 2 * Theme.paddingMedium)
                onClicked: dialog.generate()

                Label {
                    id: passwordLabel

                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    text: dialog.password.length > 0 ? dialog.password : qsTr("Select at least one kind of character")
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    horizontalAlignment: Text.AlignHCenter
                    font.family: "Monospace"
                    font.pixelSize: Theme.fontSizeLarge
                    color: dialog.password.length > 0 ? Theme.highlightColor : Theme.errorColor
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Tap to generate another")
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
            }

            Slider {
                id: lengthSlider

                width: parent.width
                label: qsTr("Length")
                minimumValue: 4
                maximumValue: 64
                stepSize: 1
                value: appSettings.classicLength
                valueText: value
                onValueChanged: dialog.generate()
            }

            TextSwitch {
                id: lowerSwitch
                text: qsTr("Lowercase letters")
                checked: appSettings.classicLowercase
                onCheckedChanged: dialog.generate()
            }
            TextSwitch {
                id: upperSwitch
                text: qsTr("Uppercase letters")
                checked: appSettings.classicUppercase
                onCheckedChanged: dialog.generate()
            }
            TextSwitch {
                id: digitsSwitch
                text: qsTr("Digits")
                checked: appSettings.classicDigits
                onCheckedChanged: dialog.generate()
            }
            TextSwitch {
                id: symbolsSwitch
                text: qsTr("Symbols")
                checked: appSettings.classicSymbols
                onCheckedChanged: dialog.generate()
            }
            TextSwitch {
                id: ambiguousSwitch
                text: qsTr("Avoid look-alike characters")
                description: "B8G6I1l0OQDS5Z2"
                checked: appSettings.classicAvoidAmbiguous
                onCheckedChanged: dialog.generate()
            }
        }

        VerticalScrollDecorator { }
    }
}
