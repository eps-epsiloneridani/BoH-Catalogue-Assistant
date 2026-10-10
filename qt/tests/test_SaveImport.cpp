// Port of the SaveImport pipeline tests (plan Task 7; the importer tests land
// with Task 8). Fixtures reproduce the game's lenient JSON dialect: UTF-16 with
// BOM, trailing commas, raw control characters.
#include <QtTest/QtTest>

#include "SaveImport.h"

#include "Migrator.h"
#include "Repositories/PlaythroughRepository.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <memory>
#include <stdexcept>

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

template <typename List>
std::vector<QString> titles(const List& books)
{
    std::vector<QString> out;
    for (const auto& b : books)
        out.push_back(b.title);
    return out;
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


    // MARK: - Import (Task 8)

    void lenientJSONHelpers_feelOwnProperty()
    {
        // The game's own tomes.json uses "ID" (capital); skills use "id".
        const auto object = parse(QByteArray("{ \"elements\": [ { \"ID\": \"t.a\" }, { \"id\": \"s.b\" } ] }"));
        QVERIFY(object.has_value());
        QCOMPARE((int) LenientJSON::dictionaries(object->value(QStringLiteral("elements"))).size(), 2);
    }

    void importCreatesBooksSkillsMemoriesAndJournal()
    {
        auto fixture = makeMigratedDB();
        SQLiteDatabase& db = *fixture.db;
        const QString elements = makeElementsDirectory();
        const QString save = makeSave();

        const ImportReport report =
            SaveImporter::run(save, db, fixture.playthroughID, elements);

        QCOMPARE(report.booksCreated, 4);
        QCOMPARE(report.booksUpdated, 0);
        QCOMPARE(report.skillsCreated, 2);
        QCOMPARE(report.memoriesCreated, 2);
        QVERIFY2(report.uncataloguedSkipped == 1, "uncatbooks are skipped");

        BookRepository books(db, fixture.playthroughID);
        const auto all = books.all();
        QSet<QString> bookTitles;
        for (const Book& b : all)
            bookTitles.insert(b.title);
        QCOMPARE(bookTitles,
                 (QSet<QString>{QStringLiteral("Test Book"), QStringLiteral("Cursed Book"),
                                QStringLiteral("Test Record"), QStringLiteral("Test Scroll")}));

        const Book testBook = findByTitle(all, QStringLiteral("Test Book"));
        QVERIFY(testBook.difficulty == std::optional<int>(4));
        QVERIFY2(testBook.readStatus == ReadStatus::Mastered, "mastery.sky mutation");
        qint64 fucineID = 0;
        for (const Language& l : LanguageRepository(db).all()) {
            if (l.name == QStringLiteral("Fucine"))
                fucineID = l.id;
        }
        QVERIFY2(testBook.languageID == std::optional<qint64>(fucineID), "w.fucine aspect");
        QVERIFY(testBook.yieldedMemoryID.has_value());
        QVERIFY2(testBook.location == QStringLiteral("Library — shelf D.3"),
                 "location = humanized room > slot chain");
        QVERIFY2(!testBook.contamination.has_value(), "defunct copies don't leak mutations");

        const auto lessons = books.lessons(testBook.id);
        QCOMPARE((int) lessons.size(), 1);
        SkillRepository skillsRepo(db, fixture.playthroughID);
        qint64 skyStoriesID = 0;
        for (const Skill& s : skillsRepo.all()) {
            if (s.name == QStringLiteral("Sky Stories"))
                skyStoriesID = s.id;
        }
        QVERIFY((lessons[0] == BookLessonsEntry{skyStoriesID, 2}));

        const Book cursed = findByTitle(all, QStringLiteral("Cursed Book"));
        QVERIFY2(cursed.readStatus == ReadStatus::Catalogued, "no mastery mutation");
        QVERIFY(cursed.contamination == std::optional<Contamination>(Contamination::Winkwell));
        QVERIFY2(cursed.bookKind == BookKind::Book,
                 "codex is the plain bound-book format — not a phonograph record");
        const Book testRecord = findByTitle(all, QStringLiteral("Test Record"));
        QVERIFY(testRecord.bookKind == BookKind::Record);
        QVERIFY(testRecord.readStatus == ReadStatus::Catalogued);
        const Book testScroll = findByTitle(all, QStringLiteral("Test Scroll"));
        QVERIFY(testScroll.bookKind == BookKind::Scroll);
        qint64 greekID = 0;
        for (const Language& l : LanguageRepository(db).all()) {
            if (l.name == QStringLiteral("Greek"))
                greekID = l.id;
        }
        QVERIFY2(cursed.languageID == std::optional<qint64>(greekID),
                 "native language matched by name");

        const auto skills = skillsRepo.all();
        const Skill skyStories = findSkill(skills, QStringLiteral("Sky Stories"));
        QVERIFY(skyStories.level == std::optional<int>(2)); // skill:1 mutation means level 2
        QCOMPARE(*skyStories.wisdom, QStringLiteral("horomachistry"));
        QCOMPARE(*skyStories.element, QStringLiteral("Trist"));
        const Skill fucineSkill = findSkill(skills, QStringLiteral("Fucine"));
        QVERIFY2(fucineSkill.isLanguage, "skill.language marker");

        MemoryRepository memoriesRepo(db, fixture.playthroughID);
        const auto memories = memoriesRepo.all();
        Memory impulse;
        for (const Memory& m : memories) {
            if (m.name == QStringLiteral("Memory: Impulse"))
                impulse = m;
        }
        QVERIFY(impulse.id != 0);
        QVERIFY(!impulse.persistent);
        std::vector<int> levels;
        for (const Aspect& a : impulse.aspects)
            levels.push_back(a.level);
        std::sort(levels.begin(), levels.end());
        QCOMPARE(levels, (std::vector<int>{1, 2})); // moth 2, nectar 1
        Memory numen;
        for (const Memory& m : memories) {
            if (m.name.startsWith(QStringLiteral("Numen")))
                numen = m;
        }
        QVERIFY2(numen.persistent, "numina are persistent");
        QVERIFY(numen.kind == MemoryKind::Numen);

        JournalRepository journalRepo(db, fixture.playthroughID);
        QVERIFY(journalRepo.recent().front().entry.contains(QStringLiteral("Imported from AUTOSAVE.json")));
    }

    void reimportFillsEmptyFieldsWithoutStompingUserData()
    {
        auto fixture = makeMigratedDB();
        SQLiteDatabase& db = *fixture.db;
        BookRepository books(db, fixture.playthroughID);
        // A user-recorded book that must survive the import untouched where filled.
        BookDraft draft = bookDraft(QStringLiteral("Test Book"));
        draft.difficulty = 8;
        const Book manual = books.insert(draft);
        Book withNotes = manual;
        withNotes.notes = QStringLiteral("my precious notes");
        books.update(withNotes);

        const QString elements = makeElementsDirectory();
        const ImportReport report =
            SaveImporter::run(makeSave(), db, fixture.playthroughID, elements);

        QVERIFY2(report.booksCreated == 3,
                 "three new tomes (the fourth title is pre-recorded)");
        QCOMPARE(report.booksUpdated, 1);
        const auto reloaded = books.get(manual.id);
        QVERIFY(reloaded.has_value());
        QCOMPARE(reloaded->notes, QStringLiteral("my precious notes")); // user notes preserved
        QVERIFY2(reloaded->difficulty == std::optional<int>(8), "user-recorded difficulty not stomped");
        QVERIFY2(reloaded->readStatus == ReadStatus::Mastered, "but read state upgrades");
        QVERIFY2(reloaded->location == QStringLiteral("Library — shelf D.3"),
                 "fill-empty: save location stamps an unrecorded one");
    }

    /// Live failure reported by the user (2026-10-06): first playthrough already
    /// held the autosave's names; importing into a new playthrough died on the
    /// 001-era table-global Skills.name UNIQUE (fixed by migration 006).
    void importIntoSecondPlaythroughWithSameEntityNames()
    {
        SQLiteDatabase db(QStringLiteral(":memory:"));
        Migrator(Migrator::bundled()).apply(db);
        const auto first = PlaythroughRepository(db).active();
        QVERIFY(first.has_value());
        const QString save = makeSave();
        const QString elements = makeElementsDirectory();

        SaveImporter::run(save, db, first->id, elements);

        const Playthrough second = PlaythroughRepository(db).insert(QStringLiteral("Playthrough 2"));
        const ImportReport report = SaveImporter::run(save, db, second.id, elements);

        QCOMPARE(report.booksCreated, 4);
        QCOMPARE(report.skillsCreated, 2);
        QCOMPARE(report.memoriesCreated, 2);
        QCOMPARE((int) SkillRepository(db, first->id).all().size(), 2);
        QCOMPARE((int) SkillRepository(db, second.id).all().size(), 2);
    }

    /// Books sitting in an Oriflamme's-auction purchases sphere: seen but not
    /// owned — unearned, skipped entirely (user ruling 2026-10-08).
    void auctionLotsAreSkippedAsUnearned()
    {
        auto fixture = makeMigratedDB();
        SQLiteDatabase& db = *fixture.db;
        const QString dir = m_work->path() + QStringLiteral("/auction-elems");
        QVERIFY(QDir().mkpath(dir));
        writeFile(dir + QStringLiteral("/tomes.json"),
                  "{ \"elements\": [\n"
                  "  { \"ID\": \"t.lot\", \"Label\": \"Unacquired Tome\",\n"
                  "    \"aspects\": { \"mystery.moon\": 4, \"codex\": 1 } },\n"
                  "  { \"ID\": \"t.owned\", \"Label\": \"Owned Tome\",\n"
                  "    \"aspects\": { \"mystery.rose\": 2, \"codex\": 1 } } ], }");
        writeFile(dir + QStringLiteral("/skills.json"), "{ \"elements\": [] }");
        writeFile(dir + QStringLiteral("/AUTOSAVE.json"),
                  "{ \"RootPopulationCommand\": { \"Spheres\": [ {\n"
                  "  \"$type\": \"SphereCreationCommand\",\n"
                  "  \"GoverningSphereSpec\": { \"$type\": \"SphereSpec\", \"Id\": \"purchases.europe\", \"Label\": \"\" },\n"
                  "  \"Tokens\": [\n"
                  "    { \"$type\": \"TokenCreationCommand\", \"Payload\": {\n"
                  "    \"$type\": \"ElementStackCreationCommand\", \"EntityId\": \"t.lot\", \"Mutations\": { } } }\n"
                  "  ] },\n"
                  "  { \"$type\": \"SphereCreationCommand\",\n"
                  "  \"GoverningSphereSpec\": { \"$type\": \"SphereSpec\", \"Id\": \"Library\", \"Label\": \"\" },\n"
                  "  \"Tokens\": [\n"
                  "    { \"$type\": \"TokenCreationCommand\", \"Payload\": {\n"
                  "    \"$type\": \"ElementStackCreationCommand\", \"EntityId\": \"t.owned\", \"Mutations\": { } } }\n"
                  "  ] } ] } }");

        const ImportReport report = SaveImporter::run(dir + QStringLiteral("/AUTOSAVE.json"), db,
                                                      fixture.playthroughID, dir);
        QVERIFY2(report.unearnedSkipped == 1, "the auction lot is skipped");
        QCOMPARE(report.booksCreated, 1); // the shelved book imports
        BookRepository books(db, fixture.playthroughID);
        QCOMPARE(titles(books.all()), (std::vector<QString>{QStringLiteral("Owned Tome")}));
    }

    /// Unknown contamination keys + Label-less skills surface in the report
    /// instead of vanishing silently (deferred here from Task 7 — needs the importer).
    void unknownContaminationAndLabellessSkillWarn()
    {
        auto fixture = makeMigratedDB();
        SQLiteDatabase& db = *fixture.db;
        const QString dir = m_work->path() + QStringLiteral("/warn-elems");
        QVERIFY(QDir().mkpath(dir));
        writeFile(dir + QStringLiteral("/tomes.json"),
                  "{ \"elements\": [\n"
                  "  { \"ID\": \"t.mad\", \"Label\": \"Mad Book\",\n"
                  "    \"aspects\": { \"mystery.moon\": 4, \"codex\": 1 } } ], }");
        writeFile(dir + QStringLiteral("/skills.json"),
                  "{ \"elements\": [\n"
                  "  { \"id\": \"s.labelless\", \"aspects\": { \"moon\": 2 } } ], }");
        writeFile(dir + QStringLiteral("/AUTOSAVE.json"),
                  "{ \"RootPopulationCommand\": { \"Spheres\": [ {\n"
                  "  \"Tokens\": [\n"
                  "    { \"$type\": \"TokenCreationCommand\", \"Payload\": {\n"
                  "    \"$type\": \"ElementStackCreationCommand\", \"EntityId\": \"t.mad\",\n"
                  "    \"Mutations\": { \"contamination.madness\": 1 } } },\n"
                  "    { \"$type\": \"TokenCreationCommand\", \"Payload\": {\n"
                  "    \"$type\": \"ElementStackCreationCommand\", \"EntityId\": \"s.labelless\", \"Mutations\": { } } }\n"
                  "  ] } ] } }");

        const ImportReport report = SaveImporter::run(dir + QStringLiteral("/AUTOSAVE.json"), db,
                                                      fixture.playthroughID, dir);
        bool sawContamination = false;
        bool sawLabelless = false;
        for (const QString& warning : report.warnings) {
            if (warning.contains(QStringLiteral("unknown contamination 'madness'")))
                sawContamination = true;
            if (warning.contains(QStringLiteral("no Label")))
                sawLabelless = true;
        }
        QVERIFY2(sawContamination, qPrintable(QStringList(report.warnings.begin(), report.warnings.end()).join(QStringLiteral(" | "))));
        QVERIFY2(sawLabelless, qPrintable(QStringList(report.warnings.begin(), report.warnings.end()).join(QStringLiteral(" | "))));
        BookRepository books(db, fixture.playthroughID);
        const auto all = books.all();
        QCOMPARE((int) all.size(), 1);
        QVERIFY2(!all.front().contamination.has_value(), "unknown contamination is not guessed");
        QVERIFY(SkillRepository(db, fixture.playthroughID).all().empty());
    }

    // MARK: - Linux path discovery (Review Focus #3)

    void steamLibraryDiscoveryReadsAllLibrariesFromVdf()
    {
        const QString root = m_work->path() + QStringLiteral("/steamroot");
        QVERIFY(QDir().mkpath(root + QStringLiteral("/steamapps")));
        const QString vdf = QStringLiteral("\"libraries\"\n{\n"
                                           "  \"0\"\t{\n\t\t\"path\"\t\t\"%1\"\n\t\t}\n"
                                           "  \"1\"\t{\n\t\t\"path\"\t\t\"/mnt/games/SteamLibrary\"\n\t\t}\n"
                                           "}").arg(root);
        writeFile(root + QStringLiteral("/steamapps/libraryfolders.vdf"), vdf);
        const QStringList libraries =
            BoHPaths::steamLibraryPaths({root});
        QVERIFY(libraries.contains(QDir::cleanPath(m_work->path() + QStringLiteral("/steamroot"))));
        QVERIFY(libraries.contains(QStringLiteral("/mnt/games/SteamLibrary")));
    }

    void saveDiscoveryBuildsProtonPrefixCandidates()
    {
        const QStringList libraries = {QStringLiteral("/mnt/games/SteamLibrary")};
        const QStringList candidates = BoHPaths::saveDirectoryCandidates(libraries);
        QCOMPARE((int) candidates.size(), 1);
        QCOMPARE(candidates.front(),
                 QStringLiteral("/mnt/games/SteamLibrary/steamapps/compatdata/1028310/pfx/drive_c/"
                                "users/steamuser/AppData/LocalLow/Weather Factory/Book of Hours"));
    }

    /// The native Linux build (verified on a real install): Unity saves in
    /// ~/.config/unity3d, StreamingAssets under bh_Data — both BEFORE the
    /// Proton layouts in the candidate lists.
    void nativeBuildCandidatesComeFirst()
    {
        QCOMPARE(BoHPaths::nativeSaveDirectoryCandidates().front(),
                 QDir::homePath() + QStringLiteral("/.config/unity3d/Weather Factory/Book of Hours"));
        const QStringList elements =
            BoHPaths::gameElementsDirectoryCandidates({QStringLiteral("/mnt/games/SteamLibrary")});
        QCOMPARE((int) elements.size(), 2);
        QCOMPARE(elements[0],
                 QStringLiteral("/mnt/games/SteamLibrary/steamapps/common/Book of Hours/"
                                "bh_Data/StreamingAssets/bhcontent/core/elements"));
        QCOMPARE(elements[1],
                 QStringLiteral("/mnt/games/SteamLibrary/steamapps/common/Book of Hours/"
                                "Book of Hours_Data/StreamingAssets/bhcontent/core/elements"));
    }

    void firstExistingPicksTheRealInstall()
    {
        // A synthetic tree: library A lacks the prefix, library B has it.
        // (Proton-only candidates — the native unity3d candidate is covered by
        // nativeBuildCandidatesComeFirst and the live test below.)
        const QString libB = m_work->path() + QStringLiteral("/libB");
        const QString saves = libB
                              + QStringLiteral("/steamapps/compatdata/1028310/pfx/drive_c/users/"
                                               "steamuser/AppData/LocalLow/Weather Factory/Book of Hours");
        QVERIFY(QDir().mkpath(saves));
        const QStringList candidates = BoHPaths::saveDirectoryCandidates(
            {m_work->path() + QStringLiteral("/libA"), libB});
        QCOMPARE(*BoHPaths::firstExisting(candidates), saves);
    }

    /// Live acceptance: on a machine with the game installed, discovery must
    /// find the real elements directory and the real saves (skips otherwise).
    void discoveryFindsTheRealInstallWhenPresent()
    {
        const auto elements = BoHPaths::gameElementsDirectory();
        const auto saves = BoHPaths::saveDirectory();
        if (!elements || !QDir(*elements).exists() || !saves || !QDir(*saves).exists())
            QSKIP("game not installed on this machine");
        QVERIFY2(elements->endsWith(QStringLiteral("bhcontent/core/elements")),
                 qPrintable(*elements));
        QVERIFY2(saves->contains(QStringLiteral("Book of Hours")), qPrintable(*saves));
        QVERIFY(!SaveScanner::availableSaves().empty());
    }

    // MARK: - Real game (skips when not installed)

    void importFromRealInstalledGameAndSave()
    {
        const auto elementsDirectory = BoHPaths::gameElementsDirectory();
        const auto saves = SaveScanner::availableSaves();
        if (!elementsDirectory || !QDir(*elementsDirectory).exists() || saves.empty())
            QSKIP("Book of Hours (and/or a save) not installed at the standard path");
        auto fixture = makeMigratedDB();
        SQLiteDatabase& db = *fixture.db;
        ImportReport report;
        db.transaction([&] {
            report = SaveImporter::run(saves.front().path, db, fixture.playthroughID,
                                       *elementsDirectory);
        });
        QVERIFY(report.booksCreated + report.booksUpdated > 0);
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


    struct Fixture {
        std::unique_ptr<SQLiteDatabase> db;
        qint64 playthroughID = 0;
    };

    Fixture makeMigratedDB()
    {
        Fixture fixture;
        fixture.db = std::make_unique<SQLiteDatabase>(QStringLiteral(":memory:"));
        Migrator(Migrator::bundled()).apply(*fixture.db);
        const auto active = PlaythroughRepository(*fixture.db).active();
        if (!active)
            throw std::runtime_error("migration 005 must seed a default playthrough");
        fixture.playthroughID = active->id;
        return fixture;
    }

    Book findByTitle(const std::vector<Book>& books, const QString& title)
    {
        for (const Book& b : books) {
            if (b.title == title)
                return b;
        }
        throw std::runtime_error(QStringLiteral("no book titled %1").arg(title).toStdString());
    }

    Skill findSkill(const std::vector<Skill>& skills, const QString& name)
    {
        for (const Skill& s : skills) {
            if (s.name == name)
                return s;
        }
        throw std::runtime_error(QStringLiteral("no skill named %1").arg(name).toStdString());
    }

    BookDraft bookDraft(const QString& title)
    {
        BookDraft draft;
        draft.title = title;
        return draft;
    }

    static void writeFile(const QString& path, const QString& contents, bool utf16 = false)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            throw std::runtime_error("writeFile open failed: " + path.toStdString());
        if (utf16) {
            QByteArray data;
            data.append(char(0xFF));
            data.append(char(0xFE));
            data.append(QStringEncoder(QStringConverter::Utf16LE).encode(contents));
            file.write(data);
        } else {
            file.write(contents.toUtf8());
        }
    }

    QString makeElementsDirectory()
    {
        const QString dir = m_work->path() + QStringLiteral("/elements");
        if (!QDir().mkpath(dir))
            throw std::runtime_error("mkpath elements failed");
        writeFile(dir + QStringLiteral("/tomes.json"), R"json({ "elements": [
          { "ID": "t.testbook", "Label": "Test Book",
            "aspects": { "mystery.sky": 4, "w.fucine": 1, "r.skystories": 1, "soph": 4 },
            "xtriggers": {
              "mastering.sky": [ { "id": "x.skystories", "morpheffect": "spawn", "level": 2 } ],
              "reading.sky": [ { "id": "mem.impulse", "morpheffect": "spawn", "level": 1 } ] } },
          { "ID": "t.cursedbook", "Label": "Cursed Book",
            "aspects": { "mystery.moon": 6, "w.greek": 1, "codex": 1 },
            "xtriggers": {
              "reading.moon": [ { "id": "numen.asce", "morpheffect": "spawn", "level": 1 } ] } },
          { "ID": "t.testrecord", "Label": "Test Record",
            "aspects": { "mystery.edge": 3, "record.phonograph": 1 } },
          { "ID": "t.testscroll", "Label": "Test Scroll",
            "aspects": { "mystery.winter": 5, "scroll": 1 } },
        ], })json");
        writeFile(dir + QStringLiteral("/skills.json"), R"json({ "elements": [
          { "id": "s.skystories", "Label": "Sky Stories",
            "aspects": { "sky": 2, "rose": 1, "skill": 1, "w.horomachistry": 1 } },
          { "id": "s.fucine", "Label": "Fucine",
            "aspects": { "heart": 2, "knock": 1, "skill.language": 1, "skill": 1 } },
        ], })json", /*utf16=*/true);
        writeFile(dir + QStringLiteral("/memories.json"), R"json({ "elements": [
          { "ID": "mem.impulse", "Label": "Memory: Impulse", "inherits": "_memory",
            "aspects": { "moth": 2, "boost.moth": 2, "nectar": 1, "boost.nectar": 1 } },
          { "ID": "numen.asce", "Label": "Numen: a Final Understanding", "inherits": "_numen",
            "aspects": { "forge": 5, "knock": 5, "lantern": 5 } },
        ], })json");
        return dir;
    }

    QString makeSave()
    {
        const QString path = m_work->path() + QStringLiteral("/AUTOSAVE.json");
        // (written via writeFile below)
        writeFile(path, R"json({
          "$type": "Save",
          "Version": { "$type": "V", "Version": "2026.1.f.3" },
          "RootPopulationCommand": {
            "$type": "Root",
            "Spheres": [
              {
                "$type": "SphereCreationCommand",
                "GoverningSphereSpec": { "$type": "SphereSpec", "Id": "Library", "Label": "" },
                "Tokens": [
                  {
                    "$type": "TokenCreationCommand",
                    "Payload": {
                      "$type": "PopulateTerrainFeatureCommand",
                      "Id": "library",
                      "Dominions": [
                        {
                          "$type": "PopulateDominionCommand",
                          "Spheres": [
                            {
                              "$type": "SphereCreationCommand",
                              "GoverningSphereSpec": { "$type": "SphereSpec", "Id": "ShelfSpaceSphereD.3", "Label": "" },
                              "Tokens": [
            { "$type": "TokenCreationCommand", "Payload": {
            "$type": "ElementStackCreationCommand", "EntityId": "t.testbook", "Quantity": 1, "Mutations": { "mastery.sky": 4 } } },
            { "$type": "TokenCreationCommand", "Payload": {
            "$type": "ElementStackCreationCommand", "EntityId": "t.testbook", "Quantity": 1,
            "Mutations": { "contamination.curse": 1 }, "Defunct": true } },
            { "$type": "TokenCreationCommand", "Payload": {
            "$type": "ElementStackCreationCommand", "EntityId": "t.cursedbook", "Quantity": 1, "Mutations": { "contamination.winkwell": 1 } } },
            { "$type": "TokenCreationCommand", "Payload": {
            "$type": "ElementStackCreationCommand", "EntityId": "t.testrecord", "Quantity": 1, "Mutations": { } } },
            { "$type": "TokenCreationCommand", "Payload": {
            "$type": "ElementStackCreationCommand", "EntityId": "t.testscroll", "Quantity": 1, "Mutations": { } } },
            { "$type": "TokenCreationCommand", "Payload": {
            "$type": "ElementStackCreationCommand", "EntityId": "s.skystories", "Quantity": 1, "Mutations": { "skill": 1, "wisdom.committed": 1, "w.horomachistry": -1, "a.xtri": 1 } } },
            { "$type": "TokenCreationCommand", "Payload": {
            "$type": "ElementStackCreationCommand", "EntityId": "s.fucine", "Quantity": 1, "Mutations": { } } },
            { "$type": "TokenCreationCommand", "Payload": {
            "$type": "ElementStackCreationCommand", "EntityId": "uncatbook.baronial", "Quantity": 1, "Mutations": { } } },
            { "$type": "TokenCreationCommand", "Payload": {
            "$type": "ElementStackCreationCommand", "EntityId": "mem.leftover", "Quantity": 1, "Mutations": { } } }
                              ]
                            }
                          ]
                        }
                      ]
                    }
                  }
                ]
              }
            ]
          }
        })json");
        return path;
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

    static void writeFile(const QString& path, const char* contents) { writeFile(path, QString::fromUtf8(contents)); }
};

QTEST_MAIN(SaveImportTests)
#include "test_SaveImport.moc"
