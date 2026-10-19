# -*- mode: sh -*-
# Sailjail profile of Password Store, applied with the permissions of password-store.desktop.

# git and ssh sync the store and ssh-keygen makes the key. Base.permission blacklists
# ${PATH}/ssh*, which this profile comes before. There is no shell, busybox is blacklisted,
# so git runs ssh through the app binary.
noblacklist ${PATH}/ssh
noblacklist ${PATH}/ssh-keygen
private-bin git,ssh,ssh-keygen
whitelist /usr/share/git-core
read-only /usr/share/git-core
