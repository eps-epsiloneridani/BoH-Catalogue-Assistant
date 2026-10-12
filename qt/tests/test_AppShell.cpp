// Remediation follow-up (R4's lesson): the section stack's placeholder-swap
// arithmetic drifted across five tasks and shipped two placeholder sections —
// nothing failed a compile. This test pins the assembled window: five sections,
// each hosting the real screen class, no QLabel placeholders anywhere.
#include <QtTest/QtTest>

#include "AppController.h"
#include "Views/MainWindow.h"

#include <QLabel>

#include <cstring>
#include <QStackedWidget>

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
} // namespace

class AppShellTests final : public QObject {
    Q_OBJECT

private slots:
    void everySectionHostsItsRealScreen()
    {
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const ScopedEnv env("BOH_DB_PATH", work.path() + QStringLiteral("/Boh.db"));
        // cwd-independent: the binary lives in qt/build → repo root is ../.. up.
        const ScopedEnv mig("BOH_MIGRATIONS",
                            QCoreApplication::applicationDirPath()
                                + QStringLiteral("/../../db/migrations"));

        AppController controller;
        controller.bootstrap();
        QVERIFY2(controller.phase() == AppController::Phase::Ready,
                 qPrintable(controller.failureMessage()));

        MainWindow window(controller);
        window.show(); // offscreen platform

        auto* stack = window.findChild<QStackedWidget*>();
        QVERIFY2(stack, "the section stack exists");
        QCOMPARE(stack->count(), 5);

        // (class name per index, description) — in sidebar order.
        const std::vector<std::pair<const char*, const char*>> sections = {
            {"boh::BooksScreen", "Books"},
            {"boh::MemoriesScreen", "Memories"},
            {"boh::SkillsScreen", "Skills"},
            {"boh::JournalScreen", "Journal"},
            {"boh::ReadingHelperScreen", "Reading Helper"},
        };
        for (int i = 0; i < stack->count(); ++i) {
            const QWidget* screen = stack->widget(i);
            QVERIFY2(screen, qPrintable(QStringLiteral("section %1 has a widget").arg(i)));
            QVERIFY2(strcmp(screen->metaObject()->className(), sections[size_t(i)].first) == 0,
                     qPrintable(QStringLiteral("%1's screen is %2, expected %3")
                                    .arg(sections[size_t(i)].second)
                                    .arg(screen->metaObject()->className())
                                    .arg(sections[size_t(i)].first)));
        }

        // And no placeholder QLabel survived anywhere in the stack.
        for (int i = 0; i < stack->count(); ++i) {
            QVERIFY2(!qobject_cast<const QLabel*>(stack->widget(i)),
                     qPrintable(QStringLiteral("section %d is still a placeholder").arg(i)));
        }
    }
};

QTEST_MAIN(AppShellTests)
#include "test_AppShell.moc"