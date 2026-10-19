# Password Store

A Sailfish OS client for [pass](https://www.passwordstore.org/), the standard
unix password manager.

## Getting started

1. Export your private key on your computer:
   `gpg --export-secret-keys --armor <your key id> > key.asc`
2. Copy `key.asc` to the phone's Downloads folder.
3. In the app, open Settings → OpenPGP keys, pull down and choose Import from
   file. Delete `key.asc` from Downloads afterwards.
4. On the main page, pull down and choose Clone from git server. For an ssh
   URL, pick the private key your server knows, or generate a key and add its
   public key to the server.

To start a new store instead, pull down on the main page and choose Create
new store.

## Using it

- Tap a password to open it, tap a field to copy it. The clipboard is
  cleared, and the password closed, after 60 seconds.
- Pull down for new passwords and folders, sync and settings. Long press for
  copy, move and delete.
- Like pass, the first line of an entry is the password. `user:` lines give
  the username, and `otpauth://totp/...` lines give one-time codes.
- Every change is committed to git. Sync pulls, then pushes.

## Requirements

Sailfish OS 5.1 or newer, aarch64 or armv7hl, with `git` and
`openssh-clients`, which the package pulls in.

## Building

rnp is a git submodule, with one of its own, so clone with:

```
git clone --recurse-submodules git@github.com:klahr/password-store.git
```

or run `git submodule update --init --recursive` in an existing clone. Then:

```
sfdk config target=SailfishOS-5.1.0.11-aarch64
sfdk build
```

Host unit tests: `qmake tests/tests.pro && make && ./tst_core`

## License

GPLv3, see [LICENSE](LICENSE). The bundled
[rnp](https://github.com/rnpgp/rnp) is BSD-2-Clause, see
[3rdparty/rnp](3rdparty/rnp).
