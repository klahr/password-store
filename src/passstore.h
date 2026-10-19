#ifndef PASSSTORE_H
#define PASSSTORE_H

#include <QJSValue>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <memory>

class Git;
class Pgp;

// The password store on disk. Entry paths are relative to the store root
// and without the .gpg extension, like pass shows them.
class PassStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString rootPath READ rootPath WRITE setRootPath NOTIFY rootPathChanged)
    Q_PROPERTY(QString defaultRootPath READ defaultRootPath CONSTANT)
    Q_PROPERTY(bool initialized READ initialized NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    enum SortOrder {
        FolderFirst,
        FileFirst,
        Independent,
        RecentlyUsed
    };
    Q_ENUM(SortOrder)

    PassStore(const QString &defaultRootPath, Pgp *pgp, Git *git, QObject *parent = nullptr);

    QString rootPath() const { return m_rootPath; }
    void setRootPath(const QString &path);
    QString defaultRootPath() const { return m_defaultRootPath; }
    // True once a .gpg-id exists at the root
    bool initialized() const;
    // Bumped whenever entries change, for views to refresh
    int revision() const { return m_revision; }

    // Items: name, path, isDir
    Q_INVOKABLE QVariantList list(const QString &dir, int sortOrder, bool showHidden) const;
    // Fuzzy matches in dir, or below it when recursive, best first.
    // Items: name, path, folder, isDir
    Q_INVOKABLE QVariantList search(const QString &dir, const QString &query, int sortOrder,
                                    bool showHidden, bool recursive) const;
    Q_INVOKABLE bool exists(const QString &path) const;
    Q_INVOKABLE bool isFolder(const QString &path) const;
    Q_INVOKABLE QString folderOf(const QString &path) const;
    Q_INVOKABLE QString nameOf(const QString &path) const;
    Q_INVOKABLE QString join(const QString &dir, const QString &name) const;
    // An error message, or an empty string for a usable entry or folder path.
    // Symbolic links on the way are refused, they could lead out of the store.
    Q_INVOKABLE QString validatePath(const QString &path) const;
    // Recipients from the nearest .gpg-id at or above the folder of path
    Q_INVOKABLE QStringList recipients(const QString &path) const;
    // All folders, for picking a move target
    Q_INVOKABLE QStringList folders() const;

    // All callback results have ok and error, and needPassphrase and keyName
    // when the key passphrase is needed
    // Result also: text
    Q_INVOKABLE void decrypt(const QString &path, QJSValue callback);
    // Saves content to path. originalPath is the entry being edited, or empty
    // for a new one; a different path renames it.
    Q_INVOKABLE void save(const QString &path, const QString &content, const QString &originalPath,
                          QJSValue callback);
    Q_INVOKABLE void createFolder(const QString &path, QJSValue callback);
    // Like pass mv, re-encrypts entries for the .gpg-id of their new folder
    Q_INVOKABLE void move(const QString &from, const QString &to, QJSValue callback);
    Q_INVOKABLE void remove(const QString &path, QJSValue callback);
    // Writes keyIds to the .gpg-id of dir
    Q_INVOKABLE void init(const QString &dir, const QStringList &keyIds, QJSValue callback);
    Q_INVOKABLE void refresh();

signals:
    void rootPathChanged();
    void changed();

private:
    QString absolute(const QString &path) const;
    QString entryFile(const QString &path) const;
    void markUsed(const QString &path) const;
    qint64 lastUsed(const QString &path) const;
    void moveUsed(const QString &from, const QString &to) const;
    void forgetUsed(const QString &path) const;
    // Entry paths in folder path, or path itself for an entry
    QStringList entriesIn(const QString &path) const;
    // Recipients from the .gpg-id in relative folder dir, false if it has none
    bool readGpgId(const QString &dir, QStringList *ids) const;
    // Recipients of newPath once folder or entry from is moved to to
    QStringList recipientsAfterMove(const QString &newPath, const QString &from, const QString &to) const;
    // A move that re-encrypts entries
    struct MoveJob {
        QString from;
        QString to;
        QJSValue callback;
        QStringList entries;
        QList<QStringList> recipients;
        QStringList texts;
        int encrypted = 0;
    };
    void moveDecrypt(const std::shared_ptr<MoveJob> &job);
    void moveEncrypt(const std::shared_ptr<MoveJob> &job);
    void moveAbort(const std::shared_ptr<MoveJob> &job, const QVariantMap &result);
    void finishMove(const QString &from, const QString &to, const QStringList &reencrypted, QJSValue callback);
    void sort(QVariantList &items, int sortOrder) const;
    void commit(const QStringList &paths, const QString &message, QJSValue callback);

    QString m_defaultRootPath;
    QString m_rootPath;
    Pgp *m_pgp;
    Git *m_git;
    int m_revision;
};

#endif // PASSSTORE_H
