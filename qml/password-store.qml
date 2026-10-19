import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"
import "components"

ApplicationWindow {
    id: app

    property Item rootPage
    // The last opened one-time code for the cover, kept in memory only
    property var coverOtp: null

    // Splits a code in two halves for reading it out
    function formatCode(code, steam) {
        if (code.length < 6 || steam)
            return code
        var half = Math.floor(code.length / 2)
        return code.substring(0, half) + " " + code.substring(half)
    }

    // From the cover: the list with the search field focused, unless an
    // edit is open, which would be lost
    function search() {
        app.activate()
        if (!rootPage || pageStack.find(function(p) { return p.canAccept !== undefined }))
            return
        pageStack.pop(rootPage, PageStackAction.Immediate)
        rootPage.requestSearch()
    }

    function notify(text) {
        // Errors take longer to read
        Notices.show(text, text.length > 40 ? Notice.Long : Notice.Short, Notice.Bottom)
    }

    function copy(text, label) {
        clipboard.copy(text, label, appSettings.showTime)
        notify(appSettings.showTime > 0
               ? qsTr("%1 copied, clears in %n s", "", appSettings.showTime).arg(label)
               : qsTr("%1 copied").arg(label))
    }

    function isAlive(page) {
        return !!page && pageStack.find(function(p) { return p === page }) !== null
    }

    // Asks for the key passphrase a result said is needed, then runs retry,
    // or cancel if the user gives up
    function askPassphrase(result, retry, cancel) {
        pageStack.completeAnimation()
        var dialog = pageStack.push(Qt.resolvedUrl("pages/PassphraseDialog.qml"), {
            keyName: result.keyName,
            errorText: result.error
        })
        dialog.accepted.connect(function() {
            pgp.setPassphrase(dialog.passphrase)
            retry()
        })
        if (cancel)
            dialog.rejected.connect(cancel)
    }

    // Decrypts path, asking for the key passphrase as often as needed.
    // callback gets the store result, with cancelled set when the user gave
    // up. Nothing happens once owner, the page asking, has gone.
    function decrypt(path, callback, owner) {
        store.decrypt(path, function(result) {
            if (owner && !isAlive(owner))
                return
            if (!result.needPassphrase) {
                callback(result)
                return
            }
            askPassphrase(result, function() { decrypt(path, callback, owner) }, function() {
                callback({ ok: false, cancelled: true, error: "" })
            })
        })
    }

    function withGitPassword(action) {
        if (!git.needsPassword) {
            action()
            return
        }
        pageStack.completeAnimation()
        var dialog = pageStack.push(Qt.resolvedUrl("pages/GitPasswordDialog.qml"))
        dialog.accepted.connect(function() {
            git.setPassword(dialog.password)
            action()
        })
    }

    function gitError(error) {
        if (error.indexOf("Permission denied (publickey") >= 0)
            return qsTr("The server didn't accept the SSH key. Add its public key to your account, see Settings, Git server.")
        if (error.indexOf("Host key verification failed") >= 0)
            return qsTr("The server's host key has changed: %1").arg(error)
        return error
    }

    // The one-time code setup among texts read from QR codes: uri, or a
    // message saying why there is none
    function otpFromQr(texts) {
        var message = qsTr("No QR code found")
        for (var i = 0; i < texts.length; ++i) {
            var text = texts[i]
            if (text.indexOf("otpauth://totp/") === 0)
                return { uri: text, message: "" }
            if (text.indexOf("otpauth://hotp/") === 0)
                message = qsTr("Counter based one-time codes (HOTP) aren't supported")
            else if (text.indexOf("otpauth-migration://") === 0)
                message = qsTr("Authenticator app exports aren't supported, scan the code the site shows")
            else
                message = qsTr("Not a one-time code: %1").arg(text.length > 60 ? text.substring(0, 60) + "\u2026" : text)
        }
        return { uri: "", message: message }
    }

    function gitResult(doneText) {
        return function(result) {
            if (result.ok) {
                notify(doneText)
            } else {
                if (git.authMode === "password")
                    git.forgetPassword()
                notify(gitError(result.error))
            }
        }
    }

    function sync() {
        withGitPassword(function() { git.sync(gitResult(qsTr("Synced"))) })
    }

    function pull() {
        withGitPassword(function() { git.pull(gitResult(qsTr("Pulled"))) })
    }

    function push() {
        withGitPassword(function() { git.push(gitResult(qsTr("Pushed"))) })
    }

    // Reports a finished store change, the change is done even if git failed
    function storeResult(doneText) {
        return function(result) {
            if (!result.ok)
                notify(result.error)
            else if (result.gitError)
                notify(qsTr("Saved, but not committed: %1").arg(result.gitError))
            else if (doneText)
                notify(doneText)
        }
    }

    AppSettings {
        id: appSettings

        onOtpOnCoverChanged: {
            if (!otpOnCover)
                app.coverOtp = null
        }
    }

    // The code goes when the key is locked again
    Connections {
        target: pgp
        onHasPassphraseChanged: {
            if (!pgp.hasPassphrase)
                app.coverOtp = null
        }
    }

    Binding { target: store; property: "rootPath"; value: appSettings.storePath }
    Binding { target: pgp; property: "passphraseTimeout"; value: appSettings.passphraseTimeout }
    Binding { target: git; property: "authMode"; value: appSettings.gitAuth }
    Binding { target: git; property: "username"; value: appSettings.gitUsername }
    Binding { target: git; property: "authorName"; value: appSettings.gitAuthorName }
    Binding { target: git; property: "authorEmail"; value: appSettings.gitAuthorEmail }
    Binding { target: git; property: "rebaseOnPull"; value: appSettings.rebaseOnPull }

    initialPage: Component { StorePage { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
}
