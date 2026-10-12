// R5 unit test: clearLayoutWidgets must detach EVERY widget under a layout
// tree — including widgets inside nested layouts, which plain item deletion
// leaks (they stayed drawn forever: the "detail pane overwrites" bug).
#include <QtTest/QtTest>

#include "Shared/WidgetCleanup.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

using boh::clearLayoutWidgets;

class WidgetCleanupTests final : public QObject {
    Q_OBJECT

private slots:
    void clearsNestedLayoutWidgetsCompletely()
    {
        QWidget host;
        auto* tree = new QVBoxLayout(&host); // the layout under test, no parent yet
        // Structure of the R5 leak: direct widget + nested layouts with widgets.
        tree->addWidget(new QLabel(QStringLiteral("direct"), &host));
        auto* nested = new QHBoxLayout();
        nested->addWidget(new QLineEdit(&host));
        nested->addWidget(new QPushButton(QStringLiteral("nested button"), &host));
        auto* nested2 = new QVBoxLayout();
        nested2->addWidget(new QLabel(QStringLiteral("deep"), &host));
        nested->addLayout(nested2);
        tree->addLayout(nested);
        tree->addWidget(new QLabel(QStringLiteral("tail"), &host));

        const int before = host.findChildren<QWidget*>().count();
        QVERIFY(before >= 5);

        clearLayoutWidgets(tree);

        // Every widget detached: no children left on the host, and each widget
        // now parentless (deleteLater is pending; the objects still exist).
        QCOMPARE(host.findChildren<QWidget*>().count(), 0);
        QCOMPARE(tree->count(), 0);
        QCOMPARE(tree->children().count(), 0); // no child layouts left either
    }

    void emptyAndNullAreSafe()
    {
        clearLayoutWidgets(nullptr);
        QVBoxLayout empty;
        clearLayoutWidgets(&empty); // must not warn / crash
        QCOMPARE(empty.count(), 0);
    }
};

QTEST_MAIN(WidgetCleanupTests)
#include "test_WidgetCleanup.moc"