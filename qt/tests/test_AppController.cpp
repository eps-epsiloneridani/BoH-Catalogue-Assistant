// Plan Task 9: the app controller's bootstrap contract — resolve/create the db,
// migrate to v9, seed/keep a default playthrough, expose counts, and keep ONE
// reload choke point. Run offscreen (set in CMake test properties).
#include <QtTest/QtTest>

#include "AppController.h"
#include "Migrator.h"
#include "Repositories/BookRepository.h"

#include <QFile>
#include <QTemporaryDir>

#include <sys/stat.h>

using namespace boh;

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

class AppControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void bootstrapCreatesMigratesAndSeeds()
    {
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString dbPath = work.path() + QStringLiteral("/Boh.db");
        const ScopedEnv env("BOH_DB_PATH", dbPath);
        // Migrations must resolve from the repo (cwd-independent test):
        const ScopedEnv mig("BOH_MIGRATIONS", QStringLiteral("../../db/migrations"));

        AppController controller;
        controller.bootstrap();

        QCOMPARE(controller.phase(), AppController::Phase::Ready);
        QCOMPARE(controller.dbPath(), dbPath);
        QCOMPARE(controller.schemaVersion(), 9);
        QVERIFY(QFile::exists(dbPath));
        QVERIFY2(modeIs(dbPath, 0600), "the db file must be 600 (D11)");
        QCOMPARE((int) controller.principles().size(), 13);
        QCOMPARE((int) controller.languages().size(), 15);
        QVERIFY(controller.activePlaythrough().has_value());
        QCOMPARE(controller.activePlaythrough()->name, QStringLiteral("First playthrough"));
        QCOMPARE(controller.playthroughs().size(), 1);
    }

    void bootstrapFailsLoudWhenEnvPathDirIsMissing()
    {
        // Env-override paths don't get parent dirs created (Swift parity: only
        // the XDG branch creates its directory) — a missing dir fails loud.
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString dbPath = work.path() + QStringLiteral("/nested/deeper/Boh.db");
        const ScopedEnv env("BOH_DB_PATH", dbPath);
        const ScopedEnv mig("BOH_MIGRATIONS", QStringLiteral("../../db/migrations"));

        AppController controller;
        controller.bootstrap();
        QCOMPARE(controller.phase(), AppController::Phase::Failed);
        QVERIFY(!controller.failureMessage().isEmpty());
    }

    void bootstrapFailsLoudOnABrokenSetup()
    {
        // A db NEWER than the app must fail into the Failed phase (refuse silently
        // running against an unknown schema).
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString dbPath = work.path() + QStringLiteral("/Boh.db");
        {
            SQLiteDatabase db(dbPath);
            db.executeScript(QStringLiteral("PRAGMA user_version = 99;"));
        }
        const ScopedEnv env("BOH_DB_PATH", dbPath);
        const ScopedEnv mig("BOH_MIGRATIONS", QStringLiteral("../../db/migrations"));

        AppController controller;
        controller.bootstrap();
        QCOMPARE(controller.phase(), AppController::Phase::Failed);
        QVERIFY(!controller.failureMessage().isEmpty());
    }

    void switchPlaythroughRemountsAndCountsFollow()
    {
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const ScopedEnv env("BOH_DB_PATH", work.path() + QStringLiteral("/Boh.db"));
        const ScopedEnv mig("BOH_MIGRATIONS", QStringLiteral("../../db/migrations"));

        AppController controller;
        controller.bootstrap();
        const qint64 firstID = controller.activePlaythrough()->id;
        QCOMPARE(controller.counts().books, 0);

        // Data lands in the first playthrough via the controller's db handle.
        BookRepository books(*controller.db(), firstID);
        BookDraft draft;
        draft.title = QStringLiteral("Counted");
        books.insert(draft);
        controller.reloadAll();
        QCOMPARE(controller.counts().books, 1);

        Playthrough second = PlaythroughRepository(*controller.db()).insert(QStringLiteral("Second"));
        const bool switched = controller.switchPlaythrough(second.id);
        QVERIFY(switched);
        QCOMPARE(controller.activePlaythrough()->name, QStringLiteral("Second"));
        QVERIFY2(controller.counts().books == 0, "the second run is empty — scoping holds");
    }
};

QTEST_MAIN(AppControllerTests)
#include "test_AppController.moc"
