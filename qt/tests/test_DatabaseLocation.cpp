// Port of the DatabaseLocation contract (mac: DatabaseLocation.swift) to the
// Linux path policy — plan Task 9:
// BOH_DB_PATH → ./Boh.db → ../Boh.db → $XDG_DATA_HOME/BoH Librarian/Boh.db
// (dir 700, db file 600 per D11).
#include <QtTest/QtTest>

#include "DatabaseLocation.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <sys/stat.h>

using boh::DatabaseLocation;

namespace {
class ScopedEnv {
public:
    ScopedEnv(const char* key, const QString& value)
        : m_key(key), m_old(qEnvironmentVariable(key))
    {
        qputenv(key, value.toUtf8());
    }
    ~ScopedEnv()
    {
        if (m_old.isEmpty())
            qunsetenv(m_key);
        else
            qputenv(m_key, m_old.toUtf8());
    }
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;

private:
    const char* m_key;
    QString m_old;
};

bool modeIs(const QString& path, mode_t expected)
{
    struct stat st;
    return ::stat(path.toUtf8().constData(), &st) == 0 && (st.st_mode & 0777) == expected;
}
} // namespace

class DatabaseLocationTests final : public QObject {
    Q_OBJECT

private slots:
    void envOverrideWins()
    {
        const ScopedEnv env("BOH_DB_PATH", QStringLiteral("/tmp/boh-env-override.db"));
        QCOMPARE(DatabaseLocation::resolvePath(), QStringLiteral("/tmp/boh-env-override.db"));
    }

    void currentDirectoryDbWinsOverXdg()
    {
        QTemporaryDir work;
        QVERIFY(work.isValid());
        QFile local(work.path() + QStringLiteral("/Boh.db"));
        QVERIFY(local.open(QIODevice::WriteOnly));
        const ScopedEnv env("BOH_DB_PATH", QString());
        const ScopedEnv xdg("XDG_DATA_HOME", work.path() + QStringLiteral("/xdg-data"));
        const ScopedCwd cwd(work.path());
        QCOMPARE(DatabaseLocation::resolvePath(), work.path() + QStringLiteral("/Boh.db"));
    }

    void xdgDataHomeIsHonoredAndCreatedWithTightPerms()
    {
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString xdgData = work.path() + QStringLiteral("/xdg-data");
        const ScopedEnv env("BOH_DB_PATH", QString());
        const ScopedEnv xdg("XDG_DATA_HOME", xdgData);
        const ScopedCwd cwd(work.path() + QStringLiteral("/empty-cwd"));
        QVERIFY(QDir().mkpath(work.path() + QStringLiteral("/empty-cwd")));

        const QString resolved = DatabaseLocation::resolvePath();
        QCOMPARE(resolved,
                 xdgData + QStringLiteral("/BoH Librarian/Boh.db"));
        QVERIFY2(modeIs(xdgData + QStringLiteral("/BoH Librarian"), 0700),
                 "the data dir must be 700 (D11)");
    }

    void xdgFallsBackToLocalShare()
    {
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const ScopedEnv env("BOH_DB_PATH", QString());
        const ScopedEnv xdg("XDG_DATA_HOME", QString());
        qputenv("HOME", work.path().toUtf8());
        const ScopedCwd cwd(work.path());
        QCOMPARE(DatabaseLocation::resolvePath(),
                 work.path() + QStringLiteral("/.local/share/BoH Librarian/Boh.db"));
    }

private:
    class ScopedCwd {
    public:
        explicit ScopedCwd(const QString& path)
            : m_old(QDir::currentPath())
        {
            QDir::setCurrent(path);
        }
        ~ScopedCwd() { QDir::setCurrent(m_old); }
        ScopedCwd(const ScopedCwd&) = delete;
        ScopedCwd& operator=(const ScopedCwd&) = delete;

    private:
        QString m_old;
    };
};

QTEST_MAIN(DatabaseLocationTests)
#include "test_DatabaseLocation.moc"
