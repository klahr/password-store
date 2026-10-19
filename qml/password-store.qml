import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"
import "components"

ApplicationWindow {
    id: app

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
