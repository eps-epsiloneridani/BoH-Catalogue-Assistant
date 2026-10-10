// Smoke test: proves the Qt Test harness builds, links, and runs under CTest.
// Plan Task 1 (docs/superpowers/plans/2026-10-09-linux-qt-port.md).
#include <QtTest/QtTest>

class SmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void triviallyTrue() { QVERIFY(true); }
};

QTEST_MAIN(SmokeTest)
#include "test_smoke.moc"
