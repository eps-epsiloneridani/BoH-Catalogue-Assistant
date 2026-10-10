// Port of the SaveImport pipeline tests (plan Task 7; the importer tests land
// with Task 8). Fixtures reproduce the game's lenient JSON dialect: UTF-16 with
// BOM, trailing commas, raw control characters.
#include <QtTest/QtTest>

#include "SaveImport.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <memory>

using namespace boh;

namespace {
/// Restores an env var on scope exit (Swift's defer-restore).
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

std::optional<QJsonObject> parse(const QByteArray& data)
{
    return LenientJSON::object(data);
}
} // namespace

class SaveImportTests final : public QObject {
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> m_work;

private slots:
    void init() { m_work = std::make_unique<QTemporaryDir>(); }
    void cleanup() { m_work.reset(); }

    // MARK: - Lenient JSON

    void lenientJSONHandlesTrailingCommasAndControlCharacters()
    {
        const QString text = QStringLiteral(
            "{ \"elements\": [ { \"id\": \"t.x\", \"Label\": \"Line\x01"
            "Break\", \"aspects\": { \"a\": 1, }, },\n], }");
        const auto object = parse(text.toUtf8());
        QVERIFY(object.has_value());
        const auto elements = LenientJSON::dictionaries(object->value(QStringLiteral("elements")));
        QCOMPARE((int) elements.size(), 1);
        QCOMPARE(LenientJSON::string(elements[0].value(QStringLiteral("Label"))),
                 QStringLiteral("Line\x01" "Break"));
        const auto aspects = LenientJSON::dictionary(elements[0].value(QStringLiteral("aspects")));
        QVERIFY(aspects.has_value());
        QCOMPARE(*LenientJSON::intValue(aspects->value(QStringLiteral("a"))), 1);
    }

    void lenientJSONHandlesUTF16WithBOM()
    {
        const QString text = QStringLiteral(
            "{ \"elements\": [ { \"id\": \"t.x\", \"Label\": \"Tête\", }, ], }");
        QByteArray data;
        data.append(char(0xFF));
        data.append(char(0xFE));
        data.append(QStringEncoder(QStringConverter::Utf16LE).encode(text));
        const auto object = parse(data);
        QVERIFY(object.has_value());
        QCOMPARE((int) LenientJSON::dictionaries(object->value(QStringLiteral("elements"))).size(), 1);
    }

    /// Plan pin (Review Focus #2): big-endian BOM files parse too (the Swift
    /// version only handled the little-endian BOM).
    void lenientJSONHandlesUTF16BEWithBOM()
    {
        const QString text = QStringLiteral(
            "{ \"elements\": [ { \"id\": \"t.x\", \"Label\": \"Tête\", }, ], }");
        QByteArray data;
        data.append(char(0xFE));
        data.append(char(0xFF));
        data.append(QStringEncoder(QStringConverter::Utf16BE).encode(text));
        const auto object = parse(data);
        QVERIFY(object.has_value());
        QCOMPARE((int) LenientJSON::dictionaries(object->value(QStringLiteral("elements"))).size(), 1);
    }

    /// The old whole-text regex ate commas INSIDE string values ("... , ] ...");
    /// the comma-stripper is string-aware now (remediation-4 class).
    void trailingCommaStrippingIsStringAware()
    {
        const QString text = QStringLiteral(
            "{ \"a\": [ \"hello , ] world\", ], \"b\": 2, }");
        const auto object = parse(text.toUtf8());
        QVERIFY(object.has_value());
        const auto array = object->value(QStringLiteral("a")).toArray();
        QCOMPARE((int) array.size(), 1);
        QCOMPARE(array.at(0).toString(), QStringLiteral("hello , ] world"));
        QCOMPARE(*LenientJSON::intValue(object->value(QStringLiteral("b"))), 2);
    }

    /// Plan pin (Review Focus #5): duplicate keys — last wins, pinned so a
    /// parser swap can't change it silently.
    void duplicateKeysLastWins()
    {
        const QString text = QStringLiteral("{ \"a\": 1, \"a\": 2 }");
        const auto object = parse(text.toUtf8());
        QVERIFY(object.has_value());
        QCOMPARE(*LenientJSON::intValue(object->value(QStringLiteral("a"))), 2);
    }

    // MARK: - Scanner

    void saveScannerFindsOnlyValidSaves()
    {
        const QString saveDir = m_work->path() + QStringLiteral("/saves");
        QVERIFY(QDir().mkpath(saveDir));
        writeFile(saveDir + QStringLiteral("/AUTOSAVE.json"),
                  QByteArray("{ \"RootPopulationCommand\": { \"Spheres\": [] },\n"
                             "  \"Version\": { \"Version\": \"2026.1.f.3\" } }"));
        writeFile(saveDir + QStringLiteral("/achievements.json"), QByteArray("{ \"not\": \"a save\" }"));

        // A valid-save-shaped stray file, grown past the scanner's cap via
        // resize (sparse — cheap to create, logical size is what counts).
        writeFile(saveDir + QStringLiteral("/junk-64GB.json"),
                  QByteArray("{ \"RootPopulationCommand\": { \"Spheres\": [] } }"));
        QFile junk(saveDir + QStringLiteral("/junk-64GB.json"));
        QVERIFY(junk.resize(SaveScanner::maxSaveFileBytes + 1));

        const ScopedEnv env("BOH_SAVE_DIR", saveDir);

        QCOMPARE(fileNames(SaveScanner::availableSaves()),
                 (QStringList{QStringLiteral("AUTOSAVE.json")}));
        QCOMPARE(SaveScanner::availableSaves().front().gameVersion, QStringLiteral("2026.1.f.3"));
        QVERIFY2(SaveScanner::availableSaves(10).empty(), "size cap skips slurping huge files");
    }

private:
    static QStringList fileNames(const std::vector<SaveGameSummary>& saves)
    {
        QStringList out;
        for (const SaveGameSummary& save : saves)
            out << save.fileName;
        return out;
    }

    static void writeFile(const QString& path, const QByteArray& contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(SaveImportTests)
#include "test_SaveImport.moc"
