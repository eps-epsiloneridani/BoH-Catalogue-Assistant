// Remediation follow-up (R4's lesson): the section stack's placeholder-swap
// arithmetic drifted across five tasks and shipped two placeholder sections —
// nothing failed a compile. This test pins the assembled window: five sections,
// each hosting the real screen class, no QLabel placeholders anywhere.
#include <QtTest/QtTest>

#include "AppController.h"
#include "Repositories/BookRepository.h"
#include "Books/BooksScreen.h"
#include "Views/MainWindow.h"

#include <QLabel>

#include <cstring>
#include <QListWidget>
#include <QSplitter>
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
BooksScreen* booksScreenOf(MainWindow& window)
{
    auto* stack = window.findChild<QStackedWidget*>();
    if (!stack || stack->count() < 1)
        return nullptr;
    return qobject_cast<BooksScreen*>(stack->widget(0));
}
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

    void selectionDoesNotLeakDetailWidgets()
    {
        // R5: switching rows must not grow the detail pane's widget tree.
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const ScopedEnv env("BOH_DB_PATH", work.path() + QStringLiteral("/Boh.db"));
        const ScopedEnv mig("BOH_MIGRATIONS",
                            QCoreApplication::applicationDirPath()
                                + QStringLiteral("/../../db/migrations"));

        AppController controller;
        controller.bootstrap();
        // Seed two books — the list needs rows to select.
        {
            BookRepository books(*controller.db(), controller.activePlaythrough()->id);
            BookDraft first;
            first.title = QStringLiteral("One");
            books.insert(first);
            BookDraft second;
            second.title = QStringLiteral("Two");
            second.difficulty = 4;
            second.readStatus = ReadStatus::Catalogued;
            books.insert(second);
        }
        controller.reloadAll();

        MainWindow window(controller);
        window.show();

        auto* books = booksScreenOf(window);
        QVERIFY(books);
        auto* list = books ? books->findChild<QListWidget*>() : nullptr;
        QVERIFY(list);
        QVERIFY(list->count() >= 2);

        const auto settle = [&] {
            for (int i = 0; i < 10; ++i)
                QCoreApplication::processEvents();
        };
        list->setCurrentRow(0);
        settle();
                QWidget* bookDetail = nullptr;
        for (QWidget* child = list ? list->parentWidget() : nullptr; child;
             child = child->parentWidget()) {
            if (auto* splitter = qobject_cast<QSplitter*>(child)) {
                if (splitter->widget(1) && !qobject_cast<QListWidget*>(splitter->widget(1))) {
                    bookDetail = splitter->widget(1);
                    break;
                }
            }
        }
        QVERIFY(bookDetail);
        const int afterFirst = bookDetail ? bookDetail->findChildren<QWidget*>().count() : 0;
        list->setCurrentRow(1);
        settle();
        list->setCurrentRow(2 % list->count());
        settle();
        const int afterThird = bookDetail ? bookDetail->findChildren<QWidget*>().count() : 0;
        QCOMPARE(afterThird, afterFirst); // no accumulation, no overlay

    }
};

QTEST_MAIN(AppShellTests)
#include "test_AppShell.moc"