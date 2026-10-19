import QtQuick 2.0
import Nemo.Configuration 1.0

ConfigurationGroup {
    path: "/apps/password-store"

    // Empty means the store folder in the app's data directory
    property string storePath: ""
    property int sortOrder: 0
    property bool showHidden: false
    property bool filterRecursively: true
    property bool searchOnStart: false

    property bool showPassword: false
    property bool copyOnDecrypt: false
    // Seconds until the clipboard is cleared and an entry closed, 0 never
    property int showTime: 60
    // Minutes the key passphrase is remembered, 0 until the app closes
    property int passphraseTimeout: 5

    property int classicLength: 20
    property bool classicDigits: true
    property bool classicUppercase: true
    property bool classicLowercase: true
    property bool classicSymbols: false
    property bool classicAvoidAmbiguous: false

    property string gitAuth: "ssh-key"
    property string gitUsername: ""
    property string gitAuthorName: ""
    property string gitAuthorEmail: ""
    property bool rebaseOnPull: true
}
