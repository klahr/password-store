#include "passstore.h"
#include "git.h"
#include "jscallback.h"
#include "pgp.h"

#include <QCollator>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTextStream>

#include <algorithm>
#include <cstdio>
#include <memory>

namespace {

const QString Extension = QStringLiteral(".gpg");
const QString GpgId = QStringLiteral(".gpg-id");

QVariantMap failure(const QString &error)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("error"), error);
    return result;
}

// Names of used entries aren't kept in the settings file, only their hashes
QString usedKey(const QString &path)
{
    return QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QVariantMap item(const QString &path, bool isDir)
{
    QVariantMap map;
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    map.insert(QStringLiteral("name"), path.mid(slash + 1));
    map.insert(QStringLiteral("folder"), slash < 0 ? QString() : path.left(slash));
    map.insert(QStringLiteral("path"), path);
    map.insert(QStringLiteral("isDir"), isDir);
    return map;
}

// Higher is better, -1 when query isn't a subsequence of text
int fuzzyScore(const QString &text, const QString &query)
{
    int score = 0;
    int ti = 0;
    int streak = 0;
    for (const QChar q : query) {
        if (q.isSpace())
            continue;
        bool found = false;
        while (ti < text.length()) {
            const QChar t = text.at(ti);
            const bool boundary = ti == 0 || !text.at(ti - 1).isLetterOrNumber();
            ++ti;
            if (t.toLower() == q.toLower()) {
                ++streak;
                score += 1 + streak * 2 + (boundary ? 8 : 0);
                found = true;
                break;
            }
            streak = 0;
        }
        if (!found)
            return -1;
    }
    // Shorter paths with the same matches are closer
    return score * 100 - text.length();
}

}

PassStore::PassStore(const QString &defaultRootPath, Pgp *pgp, Git *git, QObject *parent)
    : QObject(parent)
    , m_defaultRootPath(defaultRootPath)
    , m_rootPath(defaultRootPath)
    , m_pgp(pgp)
    , m_git(git)
    , m_revision(0)
{
    m_git->setStorePath(m_rootPath);
    connect(m_git, &Git::storeChanged, this, &PassStore::refresh);
    // Older versions kept entry names readable here
    QSettings().remove(QStringLiteral("recent"));
}

void PassStore::setRootPath(const QString &path)
{
    const QString root = path.isEmpty() ? m_defaultRootPath : QDir::cleanPath(path);
    if (root == m_rootPath)
        return;
    m_rootPath = root;
    m_git->setStorePath(m_rootPath);
    emit rootPathChanged();
    refresh();
}

bool PassStore::initialized() const
{
    return QFile::exists(m_rootPath + QLatin1Char('/') + GpgId);
}

void PassStore::refresh()
{
    ++m_revision;
    emit changed();
}

QString PassStore::absolute(const QString &path) const
{
    return path.isEmpty() ? m_rootPath : m_rootPath + QLatin1Char('/') + path;
}

QString PassStore::entryFile(const QString &path) const
{
    return absolute(path) + Extension;
}

QString PassStore::join(const QString &dir, const QString &name) const
{
    return dir.isEmpty() ? name : dir + QLatin1Char('/') + name;
}

QString PassStore::folderOf(const QString &path) const
{
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? QString() : path.left(slash);
}

QString PassStore::nameOf(const QString &path) const
{
    return path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
}

bool PassStore::exists(const QString &path) const
{
    return QFile::exists(entryFile(path));
}

bool PassStore::isFolder(const QString &path) const
{
    return QFileInfo(absolute(path)).isDir();
}

QString PassStore::validatePath(const QString &path) const
{
    if (path.trimmed().isEmpty())
        return tr("Enter a name");
    if (path.startsWith(QLatin1Char('/')) || path.endsWith(QLatin1Char('/')))
        return tr("A name can't start or end with /");
    for (const QString &part : path.split(QLatin1Char('/'))) {
        if (part.isEmpty() || part == QLatin1String(".") || part == QLatin1String(".."))
            return tr("Not a valid name");
        if (part.startsWith(QLatin1String(".git")))
            return tr("Names starting with .git are reserved");
    }
    QString prefix;
    for (const QString &part : path.split(QLatin1Char('/'))) {
        prefix = join(prefix, part);
        if (QFileInfo(absolute(prefix)).isSymLink())
            return tr("%1 is a symbolic link").arg(prefix);
    }
    if (QFileInfo(entryFile(path)).isSymLink())
        return tr("%1 is a symbolic link").arg(path);
    return QString();
}

void PassStore::markUsed(const QString &path) const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("used"));
    settings.setValue(usedKey(path), QDateTime::currentMSecsSinceEpoch());
}

void PassStore::moveUsed(const QString &from, const QString &to) const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("used"));
    const QVariant used = settings.value(usedKey(from));
    settings.remove(usedKey(from));
    if (used.isValid())
        settings.setValue(usedKey(to), used);
}

void PassStore::forgetUsed(const QString &path) const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("used"));
    settings.remove(usedKey(path));
}

qint64 PassStore::lastUsed(const QString &path) const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("used"));
    return settings.value(usedKey(path), 0).toLongLong();
}

void PassStore::sort(QVariantList &items, int sortOrder) const
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);

    QHash<QString, qint64> used;
    if (sortOrder == RecentlyUsed) {
        for (const QVariant &v : items) {
            const QString path = v.toMap().value(QStringLiteral("path")).toString();
            used.insert(path, lastUsed(path));
        }
    }

    std::stable_sort(items.begin(), items.end(), [&](const QVariant &a, const QVariant &b) {
        const QVariantMap ma = a.toMap();
        const QVariantMap mb = b.toMap();
        const bool da = ma.value(QStringLiteral("isDir")).toBool();
        const bool db = mb.value(QStringLiteral("isDir")).toBool();
        if (da != db) {
            if (sortOrder == FolderFirst)
                return da;
            if (sortOrder == FileFirst)
                return db;
        }
        if (sortOrder == RecentlyUsed) {
            const qint64 ua = used.value(ma.value(QStringLiteral("path")).toString());
            const qint64 ub = used.value(mb.value(QStringLiteral("path")).toString());
            if (ua != ub)
                return ua > ub;
        }
        return collator.compare(ma.value(QStringLiteral("name")).toString(),
                                mb.value(QStringLiteral("name")).toString()) < 0;
    });
}

QVariantList PassStore::list(const QString &dir, int sortOrder, bool showHidden) const
{
    QVariantList items;
    QDir::Filters filters = QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot;
    if (showHidden)
        filters |= QDir::Hidden;
    const QFileInfoList infos = QDir(absolute(dir)).entryInfoList(filters);
    for (const QFileInfo &info : infos) {
        const QString name = info.fileName();
        if (name.startsWith(QLatin1String(".git")))
            continue;
        if (info.isDir())
            items << item(join(dir, name), true);
        else if (name.endsWith(Extension) && name.length() > Extension.length())
            items << item(join(dir, name.left(name.length() - Extension.length())), false);
    }
    sort(items, sortOrder);
    return items;
}

QVariantList PassStore::search(const QString &dir, const QString &query, int sortOrder, bool showHidden,
                               bool recursive) const
{
    QList<QPair<int, QVariant>> scored;
    QDir::Filters filters = QDir::Files;
    if (showHidden)
        filters |= QDir::Hidden;
    const QString base = absolute(dir);
    QDirIterator it(base, QStringList() << QStringLiteral("*.gpg"), filters,
                    recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        it.next();
        QString path = it.filePath().mid(m_rootPath.length() + 1);
        path.chop(Extension.length());
        if (path.startsWith(QLatin1String(".git/")) || (!showHidden && path.contains(QLatin1String("/."))))
            continue;
        const QString relative = dir.isEmpty() ? path : path.mid(dir.length() + 1);
        const int score = fuzzyScore(relative, query);
        if (score >= 0)
            scored << qMakePair(score, QVariant(item(path, false)));
    }

    QVariantList items;
    for (const auto &pair : scored)
        items << pair.second;
    sort(items, sortOrder);
    QHash<QString, int> scores;
    for (const auto &pair : scored)
        scores.insert(pair.second.toMap().value(QStringLiteral("path")).toString(), pair.first);
    std::stable_sort(items.begin(), items.end(), [&](const QVariant &a, const QVariant &b) {
        return scores.value(a.toMap().value(QStringLiteral("path")).toString())
               > scores.value(b.toMap().value(QStringLiteral("path")).toString());
    });
    return items;
}

QStringList PassStore::folders() const
{
    QStringList result;
    result << QString();
    QDirIterator it(m_rootPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next().mid(m_rootPath.length() + 1);
        if (!path.startsWith(QLatin1String(".git")) && !path.contains(QLatin1String("/.")))
            result << path;
    }
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(result.begin(), result.end(), [&](const QString &a, const QString &b) {
        return collator.compare(a, b) < 0;
    });
    return result;
}

bool PassStore::readGpgId(const QString &dir, QStringList *ids) const
{
    QFile file(absolute(dir) + QLatin1Char('/') + GpgId);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    ids->clear();
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString id = stream.readLine().section(QLatin1Char('#'), 0, 0).trimmed();
        if (!id.isEmpty())
            *ids << id;
    }
    return true;
}

QStringList PassStore::recipients(const QString &path) const
{
    QString dir = isFolder(path) ? path : folderOf(path);
    QStringList ids;
    for (;;) {
        if (readGpgId(dir, &ids))
            return ids;
        if (dir.isEmpty())
            return QStringList();
        dir = folderOf(dir);
    }
}

QStringList PassStore::recipientsAfterMove(const QString &newPath, const QString &from, const QString &to) const
{
    QString dir = folderOf(newPath);
    QStringList ids;
    for (;;) {
        // Folders inside the moved one are still at their old place
        QString physical = dir;
        if (dir == to || dir.startsWith(to + QLatin1Char('/')))
            physical = from + dir.mid(to.length());
        if (isFolder(physical) && readGpgId(physical, &ids))
            return ids;
        if (dir.isEmpty())
            return QStringList();
        dir = folderOf(dir);
    }
}

QStringList PassStore::entriesIn(const QString &path) const
{
    if (!isFolder(path))
        return QStringList() << path;
    QStringList entries;
    QDirIterator it(absolute(path), QStringList() << QStringLiteral("*.gpg"), QDir::Files | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString entry = it.next().mid(m_rootPath.length() + 1);
        entry.chop(Extension.length());
        entries << entry;
    }
    return entries;
}

void PassStore::decrypt(const QString &path, QJSValue callback)
{
    markUsed(path);
    m_pgp->decrypt(entryFile(path), [callback](const QVariantMap &result) {
        JsCallback::call(callback, result);
    });
}

void PassStore::commit(const QStringList &paths, const QString &message, QJSValue callback)
{
    refresh();
    m_git->commit(paths, message, [callback](const QVariantMap &result) {
        // The change itself is done, only report the commit failing
        QVariantMap map;
        map.insert(QStringLiteral("ok"), true);
        if (!result.value(QStringLiteral("ok")).toBool())
            map.insert(QStringLiteral("gitError"), result.value(QStringLiteral("error")));
        JsCallback::call(callback, map);
    });
}

void PassStore::save(const QString &path, const QString &content, const QString &originalPath, QJSValue callback)
{
    const QString error = validatePath(path);
    if (!error.isEmpty()) {
        JsCallback::call(callback, failure(error));
        return;
    }
    const bool renamed = !originalPath.isEmpty() && originalPath != path;
    if ((originalPath.isEmpty() || renamed) && exists(path)) {
        JsCallback::call(callback, failure(tr("%1 already exists").arg(path)));
        return;
    }
    if (isFolder(path)) {
        JsCallback::call(callback, failure(tr("%1 is a folder").arg(path)));
        return;
    }

    m_pgp->encrypt(entryFile(path), recipients(path), content,
                   [this, path, originalPath, renamed, callback](const QVariantMap &result) {
        if (!result.value(QStringLiteral("ok")).toBool()) {
            JsCallback::call(callback, result);
            return;
        }
        markUsed(path);
        QStringList paths;
        paths << path + Extension;
        QString message;
        if (renamed) {
            QFile::remove(entryFile(originalPath));
            forgetUsed(originalPath);
            paths << originalPath + Extension;
            message = tr("Rename %1 to %2.").arg(originalPath, path);
        } else if (originalPath.isEmpty()) {
            message = tr("Add password for %1 using Password Store for Sailfish OS.").arg(path);
        } else {
            message = tr("Edit password for %1 using Password Store for Sailfish OS.").arg(path);
        }
        commit(paths, message, callback);
    });
}

void PassStore::createFolder(const QString &path, QJSValue callback)
{
    const QString error = validatePath(path);
    if (!error.isEmpty()) {
        JsCallback::call(callback, failure(error));
        return;
    }
    if (!QDir().mkpath(absolute(path))) {
        JsCallback::call(callback, failure(tr("Could not create %1").arg(path)));
        return;
    }
    refresh();
    QVariantMap map;
    map.insert(QStringLiteral("ok"), true);
    JsCallback::call(callback, map);
}

void PassStore::move(const QString &from, const QString &to, QJSValue callback)
{
    QString error = validatePath(from);
    if (error.isEmpty())
        error = validatePath(to);
    if (!error.isEmpty()) {
        JsCallback::call(callback, failure(error));
        return;
    }
    const bool folder = isFolder(from);
    const QString target = folder ? absolute(to) : entryFile(to);
    if (to == from || to.startsWith(from + QLatin1Char('/'))) {
        JsCallback::call(callback, failure(tr("Can't move %1 into itself").arg(from)));
        return;
    }
    if (QFileInfo(target).exists()) {
        JsCallback::call(callback, failure(tr("%1 already exists").arg(to)));
        return;
    }

    // Entries whose .gpg-id changes, with their new recipients
    std::shared_ptr<MoveJob> job = std::make_shared<MoveJob>();
    job->from = from;
    job->to = to;
    job->callback = callback;
    for (const QString &entry : entriesIn(from)) {
        const QString newPath = folder ? to + entry.mid(from.length()) : to;
        QStringList before = recipients(entry);
        QStringList after = recipientsAfterMove(newPath, from, to);
        before.sort();
        after.sort();
        if (before != after) {
            job->entries << entry;
            job->recipients << after;
        }
    }
    if (job->entries.isEmpty())
        finishMove(from, to, QStringList(), callback);
    else
        moveDecrypt(job);
}

// Decrypts all first, so nothing is moved if a passphrase is missing
void PassStore::moveDecrypt(const std::shared_ptr<MoveJob> &job)
{
    if (job->texts.size() == job->entries.size()) {
        moveEncrypt(job);
        return;
    }
    m_pgp->decrypt(entryFile(job->entries.at(job->texts.size())), [this, job](const QVariantMap &result) {
        if (!result.value(QStringLiteral("ok")).toBool()) {
            moveAbort(job, result);
            return;
        }
        job->texts << result.value(QStringLiteral("text")).toString();
        moveDecrypt(job);
    });
}

// Encrypts each next to its old file, so nothing is moved if a key is missing
void PassStore::moveEncrypt(const std::shared_ptr<MoveJob> &job)
{
    if (job->encrypted == job->entries.size()) {
        for (QString &text : job->texts)
            text.fill(QLatin1Char('\0'));
        job->texts.clear();
        finishMove(job->from, job->to, job->entries, job->callback);
        return;
    }
    const int i = job->encrypted;
    m_pgp->encrypt(entryFile(job->entries.at(i)) + QStringLiteral(".reencrypt"), job->recipients.at(i),
                   job->texts.at(i), [this, job](const QVariantMap &result) {
        if (!result.value(QStringLiteral("ok")).toBool()) {
            moveAbort(job, result);
            return;
        }
        ++job->encrypted;
        moveEncrypt(job);
    });
}

void PassStore::moveAbort(const std::shared_ptr<MoveJob> &job, const QVariantMap &result)
{
    for (QString &text : job->texts)
        text.fill(QLatin1Char('\0'));
    job->texts.clear();
    for (const QString &entry : job->entries)
        QFile::remove(entryFile(entry) + QStringLiteral(".reencrypt"));
    JsCallback::call(job->callback, result);
}

void PassStore::finishMove(const QString &from, const QString &to, const QStringList &reencrypted,
                           QJSValue callback)
{
    const bool folder = isFolder(from);
    const QString source = folder ? absolute(from) : entryFile(from);
    const QString target = folder ? absolute(to) : entryFile(to);
    QDir().mkpath(QFileInfo(target).absolutePath());
    if (!QDir().rename(source, target)) {
        for (const QString &entry : reencrypted)
            QFile::remove(entryFile(entry) + QStringLiteral(".reencrypt"));
        JsCallback::call(callback, failure(tr("Could not move %1").arg(from)));
        return;
    }
    // The re-encrypted copies moved along, put them in place
    for (const QString &entry : reencrypted) {
        const QString newPath = folder ? to + entry.mid(from.length()) : to;
        const QString copy = folder ? entryFile(newPath) + QStringLiteral(".reencrypt")
                                    : entryFile(entry) + QStringLiteral(".reencrypt");
        ::rename(QFile::encodeName(copy).constData(), QFile::encodeName(entryFile(newPath)).constData());
    }
    for (const QString &entry : entriesIn(to))
        moveUsed(folder ? from + entry.mid(to.length()) : from, entry);
    const QString suffix = folder ? QString() : Extension;
    commit(QStringList() << from + suffix << to + suffix, tr("Rename %1 to %2.").arg(from, to), callback);
}

void PassStore::remove(const QString &path, QJSValue callback)
{
    const QString error = validatePath(path);
    if (!error.isEmpty()) {
        JsCallback::call(callback, failure(error));
        return;
    }
    const bool folder = isFolder(path);
    const QStringList entries = entriesIn(path);
    bool removed;
    if (folder)
        removed = !path.isEmpty() && QDir(absolute(path)).removeRecursively();
    else
        removed = QFile::remove(entryFile(path));
    if (!removed) {
        JsCallback::call(callback, failure(tr("Could not remove %1").arg(path)));
        return;
    }
    for (const QString &entry : entries)
        forgetUsed(entry);
    commit(QStringList() << (folder ? path : path + Extension), tr("Remove %1 from store.").arg(path), callback);
}

void PassStore::init(const QString &dir, const QStringList &keyIds, QJSValue callback)
{
    if (keyIds.isEmpty()) {
        JsCallback::call(callback, failure(tr("Select at least one key")));
        return;
    }
    if (!dir.isEmpty()) {
        const QString error = validatePath(dir);
        if (!error.isEmpty()) {
            JsCallback::call(callback, failure(error));
            return;
        }
    }
    QDir().mkpath(absolute(dir));
    QFile file(absolute(dir) + QLatin1Char('/') + GpgId);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        JsCallback::call(callback, failure(tr("Could not write %1").arg(file.fileName())));
        return;
    }
    file.write(keyIds.join(QLatin1Char('\n')).toUtf8() + '\n');
    file.close();
    commit(QStringList() << join(dir, GpgId),
           tr("Initialize GPG IDs in %1.").arg(dir.isEmpty() ? QStringLiteral("/") : dir), callback);
}
