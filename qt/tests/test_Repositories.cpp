// Port of app/Tests/BoHLibrarianCoreTests/RepositoryTests.swift (plan Task 6).
// Repository tests run against a fresh in-memory database with the real
// migrations applied. Fixtures mirror docs/DATABASE.md / docs/GAME_MECHANICS.md.
// Plus the plan's new pin: insertOrReuse is Unicode-aware ("Cōnfected"/"cōnfected").
#include <QtTest/QtTest>

#include "Migrator.h"
#include "Repositories/BookRepository.h"
#include "Repositories/JournalRepository.h"
#include "Repositories/LookupRepositories.h"
#include "Repositories/MemoryRepository.h"
#include "Repositories/PlaythroughRepository.h"
#include "Repositories/SkillRepository.h"
#include "SQLiteDatabase.h"

#include <QStringList>

#include <algorithm>
#include <stdexcept>
#include <memory>
#include <vector>

using namespace boh;

namespace {
template <typename List>
std::vector<QString> titles(const List& books)
{
    std::vector<QString> out;
    for (const auto& b : books)
        out.push_back(b.title);
    return out;
}

template <typename List>
std::vector<QString> names(const List& memories)
{
    std::vector<QString> out;
    for (const auto& m : memories)
        out.push_back(m.name);
    return out;
}

template <typename List>
bool contains(const List& list, const QString& value)
{
    return std::find(list.cbegin(), list.cend(), value) != list.cend();
}

BookDraft bookDraft(const QString& title)
{
    BookDraft draft;
    draft.title = title;
    return draft;
}

MemoryDraft memoryDraft(const QString& name, MemoryKind kind, bool persistent)
{
    MemoryDraft draft;
    draft.name = name;
    draft.kind = kind;
    draft.persistent = persistent;
    return draft;
}
} // namespace

class RepositoryTests final : public QObject {
    Q_OBJECT

    std::unique_ptr<SQLiteDatabase> m_db;
    std::unique_ptr<MemoryRepository> memories;
    std::unique_ptr<BookRepository> books;
    std::unique_ptr<SkillRepository> skills;
    std::unique_ptr<JournalRepository> journal;
    std::unique_ptr<PrincipleRepository> principles;
    std::unique_ptr<LanguageRepository> languages;
    qint64 pid = 0;

    qint64 principle(const QString& name)
    {
        for (const Principle& p : principles->all()) {
            if (p.name == name)
                return p.id;
        }
        throw std::runtime_error(QStringLiteral("principle %1 missing from seeds").arg(name).toStdString());
    }

private slots:
    void init()
    {
        m_db = std::make_unique<SQLiteDatabase>(QStringLiteral(":memory:"));
        Migrator(Migrator::bundled()).apply(*m_db);
        // Migration 005 seeds a default playthrough and marks it active; the
        // repositories under test are scoped to it.
        const auto playthrough = PlaythroughRepository(*m_db).active();
        QVERIFY2(playthrough.has_value(), "migration 005 must seed a default playthrough");
        pid = playthrough->id;
        memories = std::make_unique<MemoryRepository>(*m_db, pid);
        books = std::make_unique<BookRepository>(*m_db, pid);
        skills = std::make_unique<SkillRepository>(*m_db, pid);
        journal = std::make_unique<JournalRepository>(*m_db, pid);
        principles = std::make_unique<PrincipleRepository>(*m_db);
        languages = std::make_unique<LanguageRepository>(*m_db);
    }

    void cleanup()
    {
        memories.reset();
        books.reset();
        skills.reset();
        journal.reset();
        principles.reset();
        languages.reset();
        m_db.reset();
    }

    // MARK: - Seeded lookups

    void allThirteenPrinciplesSeededIncludingLanternAndNectar()
    {
        const auto all = principles->all();
        QCOMPARE((int) all.size(), 13);
        QStringList order;
        for (const Principle& p : all)
            order << p.name;
        QStringList sorted = order;
        std::sort(sorted.begin(), sorted.end());
        QVERIFY2(order == sorted, "ordered by sort_order == alphabetical");
        QVERIFY(contains(order, QStringLiteral("Lantern"))); // missed by the pre-project schema (D3)
        QVERIFY(contains(order, QStringLiteral("Nectar")));
        for (const Principle& p : all)
            QVERIFY2(p.color.has_value(), "principles need UI badge colors");
    }

    void principleUpdate()
    {
        Principle rose;
        for (const Principle& p : principles->all()) {
            if (p.name == QStringLiteral("Rose"))
                rose = p;
        }
        QVERIFY(rose.id != 0);
        rose.color = QStringLiteral("#FFC0CB");
        rose.notes = QStringLiteral("the rose which encompasseth all");
        principles->update(rose);
        for (const Principle& p : principles->all()) {
            if (p.name == QStringLiteral("Rose")) {
                QCOMPARE(*p.color, QStringLiteral("#FFC0CB"));
                QCOMPARE(*p.notes, QStringLiteral("the rose which encompasseth all"));
                return;
            }
        }
        QFAIL("Rose vanished");
    }

    void languagesSeededAndEditable()
    {
        const auto all = languages->all();
        QCOMPARE((int) all.size(), 15);
        QSet<QString> nativeNames;
        for (const Language& l : all) {
            if (l.native)
                nativeNames.insert(l.name);
        }
        QCOMPARE(nativeNames,
                 (QSet<QString>{QStringLiteral("Latin"), QStringLiteral("Greek"),
                                QStringLiteral("Sanskrit"), QStringLiteral("Aramaic"),
                                QStringLiteral("Phrygian")}));

        const Language added = languages->insert(QStringLiteral("Testamese"), false);
        QCOMPARE((int) languages->all().size(), 16);
        languages->remove(added.id);
        QCOMPARE((int) languages->all().size(), 15);
    }

    // MARK: - Memories

    void memoryLifecycleWithAspectsAndSources()
    {
        const qint64 rose = principle(QStringLiteral("Rose"));
        const qint64 knock = principle(QStringLiteral("Knock"));
        const qint64 moon = principle(QStringLiteral("Moon"));

        MemoryDraft draft = memoryDraft(QStringLiteral("Horizon-Sight"), MemoryKind::Memory, true);
        draft.notes = QStringLiteral("persistent Rose memory");
        draft.aspects = {AspectDraft{rose, 4}};
        const Memory horizon = memories->insert(draft);
        QVERIFY((horizon.aspects == std::vector<Aspect>{{rose, QStringLiteral("Rose"), 4}}));

        // Replace aspects wholesale.
        memories->setAspects(horizon.id, {AspectDraft{knock, 3}, AspectDraft{moon, 3}});
        const auto reloaded = memories->get(horizon.id);
        QVERIFY(reloaded.has_value());
        QCOMPARE((int) reloaded->aspects.size(), 2);
        QCOMPARE(reloaded->aspects[0].principleName, QStringLiteral("Knock"));
        QCOMPARE(reloaded->aspects[1].principleName, QStringLiteral("Moon"));

        // Update scalar fields without touching aspects via the transactional path.
        Memory edited = *reloaded;
        edited.name = QStringLiteral("Horizon-Sight (confirmed)");
        edited.notes = QStringLiteral("gained by discarding a penny at Sea's Edge");
        memories->update(edited);
        QCOMPARE(memories->get(horizon.id)->name, QStringLiteral("Horizon-Sight (confirmed)"));
        QCOMPARE((int) memories->get(horizon.id)->aspects.size(), 2);

        // Sources.
        memories->addSource(horizon.id, QStringLiteral("craft"), QStringLiteral("Keeper 00 Rose"));
        memories->addSource(horizon.id, QStringLiteral("numa"),
                            QStringLiteral("gathering at Sea's Edge (25%)"));
        memories->addSource(horizon.id, QStringLiteral("numa"),
                            QStringLiteral("gathering at Sea's Edge (25%)")); // duplicate ignored
        QCOMPARE((int) memories->sources(horizon.id).size(), 2);
        memories->removeSource(horizon.id, QStringLiteral("craft"), QStringLiteral("Keeper 00 Rose"));
        QCOMPARE((int) memories->sources(horizon.id).size(), 1);

        // Delete cascades aspects + sources.
        memories->remove(horizon.id);
        QVERIFY(!memories->get(horizon.id).has_value());
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM MemoryAspects;")), 0);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM MemorySources;")), 0);
    }

    void readingHelperMemoryCandidates()
    {
        const qint64 rose = principle(QStringLiteral("Rose"));
        const qint64 knock = principle(QStringLiteral("Knock"));
        const qint64 sky = principle(QStringLiteral("Sky"));
        const qint64 winter = principle(QStringLiteral("Winter"));

        MemoryDraft horizonDraft = memoryDraft(QStringLiteral("Horizon-Sight"), MemoryKind::Memory, true);
        horizonDraft.aspects = {AspectDraft{rose, 4}};
        memories->insert(horizonDraft);
        MemoryDraft fogDraft = memoryDraft(QStringLiteral("Fog"), MemoryKind::Weather, false);
        fogDraft.aspects = {AspectDraft{knock, 3}};
        memories->insert(fogDraft);
        MemoryDraft numenDraft = memoryDraft(QStringLiteral("Numen: That Old Lost Music"),
                                             MemoryKind::Numen, true);
        numenDraft.aspects = {AspectDraft{rose, 5}, AspectDraft{sky, 5}, AspectDraft{winter, 5}};
        const Memory numen = memories->insert(numenDraft);

        // Rose >= 4 → the numen (level 5) first, then Horizon-Sight (4).
        std::vector<QString> roseNames;
        for (const MemoryCandidate& c : memories->candidates(rose, 4))
            roseNames.push_back(c.name);
        QCOMPARE(roseNames,
                 (std::vector<QString>{QStringLiteral("Numen: That Old Lost Music"),
                                       QStringLiteral("Horizon-Sight")}));
        QCOMPARE(memories->candidates(rose, 4).front().kind, MemoryKind::Numen);

        // Out of reach.
        QVERIFY(memories->candidates(rose, 10).empty());

        // Weather counts as a candidate source.
        std::vector<QString> knockNames;
        for (const MemoryCandidate& c : memories->candidates(knock, 3))
            knockNames.push_back(c.name);
        QCOMPARE(knockNames, (std::vector<QString>{QStringLiteral("Fog")}));

        // Exact minimum is inclusive.
        std::vector<QString> skyNames;
        for (const MemoryCandidate& c : memories->candidates(sky, 5))
            skyNames.push_back(c.name);
        QCOMPARE(skyNames, (std::vector<QString>{numen.name}));
    }

    // MARK: - Books

    void bookLifecycle()
    {
        const qint64 rose = principle(QStringLiteral("Rose"));
        qint64 fucine = 0;
        for (const Language& l : languages->all()) {
            if (l.name == QStringLiteral("Fucine"))
                fucine = l.id;
        }

        BookDraft draft;
        draft.title = QStringLiteral("The Turquoise Hand");
        draft.bookKind = BookKind::Book;
        draft.languageID = fucine;
        draft.mysteryPrincipleID = rose;
        draft.difficulty = 10;
        draft.readStatus = ReadStatus::Catalogued;
        draft.contamination = Contamination::None;
        draft.location = QStringLiteral("Silver Vault");
        draft.lessons = 2;
        const Book book = books->insert(draft);
        QCOMPARE(book.title, QStringLiteral("The Turquoise Hand"));
        QVERIFY(book.difficulty == std::optional<int>(10));
        QVERIFY(book.readStatus == ReadStatus::Catalogued);
        QVERIFY(book.contamination == std::optional<Contamination>(Contamination::None));
        QCOMPARE(book.timesRead, 0);

        // Reads.
        books->recordRead(book.id);
        books->updateReadStatus(book.id, ReadStatus::Mastered);
        auto reloaded = books->get(book.id);
        QVERIFY(reloaded.has_value());
        QCOMPARE(reloaded->timesRead, 1);
        QVERIFY(reloaded->readStatus == ReadStatus::Mastered);
        QVERIFY(reloaded->firstReadAt.has_value());
        QCOMPARE(*reloaded->firstReadAt, *reloaded->lastReadAt);
        books->recordRead(book.id);
        reloaded = books->get(book.id);
        QCOMPARE(reloaded->timesRead, 2);

        // Generic update preserves read counters.
        Book edited = *reloaded;
        edited.title = QStringLiteral("The Turquoise Hand (glossed)");
        books->update(edited);
        QCOMPARE(books->get(book.id)->timesRead, 2);
        QCOMPARE(books->get(book.id)->title, QStringLiteral("The Turquoise Hand (glossed)"));

        books->remove(book.id);
        QVERIFY(!books->get(book.id).has_value());
    }

    void yieldedMemoryLinkClearsWhenMemoryDeleted()
    {
        const Memory horizon = memories->insert(memoryDraft(QStringLiteral("Horizon-Sight"),
                                                            MemoryKind::Memory, true));
        BookDraft draft = bookDraft(QStringLiteral("Towards a Fundamental Aesthetic"));
        draft.yieldedMemoryID = horizon.id;
        const Book book = books->insert(draft);
        QVERIFY(books->get(book.id)->yieldedMemoryID == std::optional<qint64>(horizon.id));

        memories->remove(horizon.id);
        QVERIFY2(!books->get(book.id)->yieldedMemoryID.has_value(),
                 "ON DELETE SET NULL must clear the link");
    }

    void booksYieldingBacklink()
    {
        const Memory memory = memories->insert(memoryDraft(QStringLiteral("Memory: Revelation"),
                                                           MemoryKind::Memory, false));
        BookDraft masteredDraft = bookDraft(QStringLiteral("Gospel of Nicodemus"));
        masteredDraft.yieldedMemoryID = memory.id;
        const Book mastered = books->insert(masteredDraft);
        BookDraft unreadDraft = bookDraft(QStringLiteral("A Light in the Inkwell"));
        unreadDraft.yieldedMemoryID = memory.id;
        const Book unread = books->insert(unreadDraft);
        books->insert(bookDraft(QStringLiteral("Sunrise Awakenings")));
        books->updateReadStatus(mastered.id, ReadStatus::Mastered);

        // Spoiler policy: only mastered books are displayed backlinks —
        // the recorded-but-unread book keeps its link privately.
        QCOMPARE(titles(memories->booksYielding(memory.id)),
                 (std::vector<QString>{QStringLiteral("Gospel of Nicodemus")}));

        // Once the player masters the unread book, its yield becomes theirs to see.
        books->updateReadStatus(unread.id, ReadStatus::Mastered);
        QCOMPARE(titles(memories->booksYielding(memory.id)),
                 (std::vector<QString>{QStringLiteral("A Light in the Inkwell"),
                                       QStringLiteral("Gospel of Nicodemus")}));
    }

    /// Earned-only list: a memory is visible when it has no yield links (player-made)
    /// or a mastered yielding book; unearned imports stay out of the list while
    /// remaining in the table for lookups.
    void allKnownHidesUnearnedYieldMemories()
    {
        const Memory earned = memories->insert(memoryDraft(QStringLiteral("Memory: Revelation"),
                                                           MemoryKind::Memory, false));
        memories->insert(memoryDraft(QStringLiteral("Weather: Drizzle"), MemoryKind::Weather, false));
        const Memory unearned = memories->insert(memoryDraft(QStringLiteral("Numen: a Final Understanding"),
                                                             MemoryKind::Numen, true));
        BookDraft linkDraft = bookDraft(QStringLiteral("Gospel of Nicodemus"));
        linkDraft.yieldedMemoryID = earned.id;
        books->insert(linkDraft);
        BookDraft masteredDraft = bookDraft(QStringLiteral("A Light in the Inkwell"));
        masteredDraft.yieldedMemoryID = earned.id;
        const Book masteredBook = books->insert(masteredDraft);
        books->updateReadStatus(masteredBook.id, ReadStatus::Mastered);
        BookDraft unearnedDraft = bookDraft(QStringLiteral("The Carbonek Schism"));
        unearnedDraft.yieldedMemoryID = unearned.id;
        books->insert(unearnedDraft);

        const auto known = names(memories->allKnown());
        QVERIFY2(contains(known, QStringLiteral("Memory: Revelation")),
                 "a mastered yielding link earns visibility even with unmastered links");
        QVERIFY2(contains(known, QStringLiteral("Weather: Drizzle")),
                 "no yield links = player-created");
        QVERIFY2(!contains(known, QStringLiteral("Numen: a Final Understanding")),
                 "only unmastered yielding books = not yet known");

        // Earning it flips it into the list; the table keeps all rows for lookups.
        const int bookID = m_db->scalarInt(
            QStringLiteral("SELECT id FROM Books WHERE title = 'The Carbonek Schism';"));
        books->updateReadStatus(bookID, ReadStatus::Mastered);
        QVERIFY(contains(names(memories->allKnown()), QStringLiteral("Numen: a Final Understanding")));
        QCOMPARE((int) memories->all().size(), 3);
    }

    /// Quick-add from a read reuses same-(name,kind) memories instead of failing
    /// on per-playthrough uniqueness; different kind inserts fine.
    void insertOrReuseMatchesNameAndKindCaseInsensitively()
    {
        const Memory existing = memories->insert(memoryDraft(QStringLiteral("Memory: Salt"),
                                                             MemoryKind::Memory, false));
        memories->setAspects(existing.id, {AspectDraft{principle(QStringLiteral("Moon")), 1}});

        MemoryDraft lowerDraft = memoryDraft(QStringLiteral("memory: salt"), MemoryKind::Memory, true);
        const Memory reuse = memories->insertOrReuse(lowerDraft);
        QVERIFY2(reuse.id == existing.id, "reused, not re-created");
        QCOMPARE((int) memories->all().size(), 1);
        QVERIFY2(memories->allKnown().front().persistent == false, "the stored row wins");

        const Memory other = memories->insertOrReuse(
            memoryDraft(QStringLiteral("Memory: Salt"), MemoryKind::Weather, true));
        QVERIFY2(other.id != existing.id, "kind differs — new row (006 allows)");
        QCOMPARE((int) memories->all().size(), 2);

        const Memory fresh = memories->insertOrReuse(
            memoryDraft(QStringLiteral("Brand New"), MemoryKind::Memory, false));
        QCOMPARE(fresh.name, QStringLiteral("Brand New")); // no match — plain insert
    }

    /// Plan's new pin (Review Focus #1): Unicode case folding must match —
    /// SQL LOWER() is ASCII-only, QString is not.
    void insertOrReuseMatchesNonAsciiNamesUnicodeAware()
    {
        const Memory existing = memories->insert(memoryDraft(QStringLiteral("Cōnfected"),
                                                             MemoryKind::Memory, false));
        const Memory reuse = memories->insertOrReuse(
            memoryDraft(QStringLiteral("cōnfected"), MemoryKind::Memory, false));
        QVERIFY2(reuse.id == existing.id, "Ō/ō fold to the same entity");
        QCOMPARE((int) memories->all().size(), 1);
    }

    void insertOrReuseTrimsWhitespaceNames()
    {
        const Memory existing = memories->insert(memoryDraft(QStringLiteral("Memory: Salt"),
                                                             MemoryKind::Memory, false));
        const Memory reuse = memories->insertOrReuse(
            memoryDraft(QStringLiteral("  Memory: Salt  "), MemoryKind::Memory, false));
        QVERIFY2(reuse.id == existing.id, "trimmed match");
        QCOMPARE(reuse.name, QStringLiteral("Memory: Salt")); // stored name keeps its shape
        QCOMPARE((int) memories->all().size(), 1);

        const Memory blankish = memories->insertOrReuse(
            memoryDraft(QStringLiteral("   "), MemoryKind::Memory, false));
        QVERIFY2(blankish.name.isEmpty(), "insert trims too (documented edge)");
    }

    /// The book pane reads live caches from these grouped queries — grouping,
    /// newest-first order, skill-name resolution, and playthrough scoping.
    void groupedJournalEntriesAndLessonAmounts()
    {
        const Book book = books->insert(bookDraft(QStringLiteral("The Carbonek Schism")));
        SkillDraft skillDraft;
        skillDraft.name = QStringLiteral("Sky Stories");
        skillDraft.primaryPrincipleID = principle(QStringLiteral("Sky"));
        skillDraft.level = 2;
        const Skill skill = skills->insert(skillDraft);
        books->setLessons(book.id, {BookLessonsEntry{skill.id, 3}});
        const JournalEntry e1 = journal->insert(JournalDraft{{}, QStringLiteral("found it"), book.id, {}, {}});
        const JournalEntry e2 = journal->insert(JournalDraft{{}, QStringLiteral("read it"), book.id, {}, {}});

        const auto journalByBook = journal->entriesByBook();
        QCOMPARE((int) journalByBook.value(book.id).size(), 2);
        QCOMPARE(journalByBook.value(book.id)[0].id, e2.id); // newest first per book
        QCOMPARE(journalByBook.value(book.id)[1].id, e1.id);
        const auto byBook = books->lessonSkillAmountsByBook();
        QCOMPARE((int) byBook.size(), 1);
        QCOMPARE(byBook.value(book.id).first().skillName, QStringLiteral("Sky Stories"));
        QCOMPARE(byBook.value(book.id).first().amount, 3);

        // A second playthrough contributes nothing to the first's caches.
        const Playthrough second = PlaythroughRepository(*m_db).insert(QStringLiteral("Second"));
        JournalRepository journal2(*m_db, second.id);
        journal2.insert(JournalDraft{{}, QStringLiteral("elsewhere"), {}, {}, {}});
        QCOMPARE((int) journal2.entriesByBook().size(), 0); // no book link, not grouped
        QVERIFY(!journal->entriesByBook().isEmpty());
        QCOMPARE((int) journal->entriesByBook().value(book.id).size(), 2);
    }

    /// Strict playthrough scoping and mastered-only yields for the group queries.
    void groupedSourcesAndYieldsScopeToPlaythrough()
    {
        const Memory memory = memories->insert(memoryDraft(QStringLiteral("Memory: Salt Taste"),
                                                           MemoryKind::Memory, false));
        memories->setSources(memory.id, {MemorySource{QStringLiteral("first read"),
                                                      QStringLiteral("Autumn 1936")},
                                         MemorySource{QStringLiteral("talk"), {}}});
        BookDraft bookDraft2 = bookDraft(QStringLiteral("The Carbonek Schism"));
        bookDraft2.yieldedMemoryID = memory.id;
        const Book book = books->insert(bookDraft2);
        books->updateReadStatus(book.id, ReadStatus::Mastered);

        // A second playthrough must contribute nothing to the first's caches.
        const Playthrough second = PlaythroughRepository(*m_db).insert(QStringLiteral("Second"));
        MemoryRepository memories2(*m_db, second.id);
        const Memory memory2 = memories2.insert(memoryDraft(QStringLiteral("Memory: Salt Taste"),
                                                            MemoryKind::Memory, false));
        memories2.setSources(memory2.id, {MemorySource{QStringLiteral("weather"), {}}});
        BookRepository books2(*m_db, second.id);
        BookDraft otherDraft = bookDraft(QStringLiteral("A Light in the Inkwell"));
        otherDraft.yieldedMemoryID = memory2.id;
        books2.insert(otherDraft);

        const auto sources = memories->allSourcesByMemory();
        QCOMPARE((int) sources.size(), 1);
        const QList<MemorySource> memorySources = sources.value(memory.id);
        QCOMPARE((int) memorySources.size(), 2);
        QCOMPARE(memorySources[0].kind, QStringLiteral("first read")); // detail nil sorts after
        QCOMPARE(memorySources[1].kind, QStringLiteral("talk"));
        const auto yielding = memories->yieldingByMemory();
        QCOMPARE((int) yielding.size(), 1);
        const QList<BookRef> refs = yielding.value(memory.id);
        QCOMPARE((int) refs.size(), 1);
        QVERIFY((refs.first() == BookRef{book.id, QStringLiteral("The Carbonek Schism")}));
    }

    /// Fail-closed scoping: a cross-playthrough update is a silent no-op.
    void crossPlaythroughWritesNoOp()
    {
        const Playthrough second = PlaythroughRepository(*m_db).insert(QStringLiteral("Second"));
        BookRepository books2(*m_db, second.id);
        MemoryRepository memories2(*m_db, second.id);
        SkillRepository skills2(*m_db, second.id);

        const Book book = books->insert(bookDraft(QStringLiteral("The Carbonek Schism")));
        books2.updateReadStatus(book.id, ReadStatus::Mastered);
        QVERIFY2(books->get(book.id)->readStatus != ReadStatus::Mastered,
                 "another playthrough's repo cannot restatus the book");
        books2.recordRead(book.id);
        QCOMPARE(books->get(book.id)->timesRead, 0);
        books2.setYieldedMemory(book.id, {});
        books2.setLessonsCount(book.id, 2);
        QVERIFY(!books->get(book.id)->lessons.has_value());

        const Memory memory = memories->insert(memoryDraft(QStringLiteral("Memory: Impulse"),
                                                           MemoryKind::Memory, false));
        memories2.update(memory);
        QCOMPARE(memories->get(memory.id)->name, QStringLiteral("Memory: Impulse"));

        SkillDraft skillDraft;
        skillDraft.name = QStringLiteral("Sky Stories");
        skillDraft.primaryPrincipleID = principle(QStringLiteral("Sky"));
        skillDraft.level = 1;
        const Skill skill = skills->insert(skillDraft);
        skills2.update(skill);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT level FROM Skills WHERE id = ?;"),
                                 {SQLiteValue(skill.id)}),
                 1);

        const JournalEntry entry = journal->insert(JournalDraft{{}, QStringLiteral("found it"), book.id, {}, {}});
        JournalRepository journal2(*m_db, second.id);
        JournalEntry edited = entry;
        edited.entry = QStringLiteral("hijacked");
        journal2.update(edited);
        QCOMPARE(journal->get(entry.id)->entry, QStringLiteral("found it"));
    }

    /// The edit form's source/link editors: setSources replaces wholesale,
    /// allYielding covers links of any status, setYieldingBooks syncs both ways.
    void sourceEditingAndYieldLinkSync()
    {
        const Memory memory = memories->insert(memoryDraft(QStringLiteral("Memory: Salt Taste"),
                                                           MemoryKind::Memory, false));
        BookDraft aDraft = bookDraft(QStringLiteral("A Light in the Inkwell"));
        aDraft.yieldedMemoryID = memory.id;
        const Book bookA = books->insert(aDraft);
        BookDraft bDraft = bookDraft(QStringLiteral("The Carbonek Schism"));
        bDraft.yieldedMemoryID = memory.id;
        const Book bookB = books->insert(bDraft);
        books->updateReadStatus(bookA.id, ReadStatus::Mastered);

        std::vector<MemorySource> sources;
        sources.push_back(MemorySource{QStringLiteral("first read"), QStringLiteral("Autumn 1936")});
        sources.push_back(MemorySource{QStringLiteral("first read"), QStringLiteral("Autumn 1936")});
        memories->setSources(memory.id, sources);
        QCOMPARE((int) memories->sources(memory.id).size(), 1); // PK dedupes

        sources.push_back(MemorySource{QStringLiteral("craft"), {}});
        memories->setSources(memory.id, sources);
        QSet<QString> kinds;
        for (const MemorySource& s : memories->sources(memory.id))
            kinds.insert(s.kind);
        QCOMPARE(kinds, (QSet<QString>{QStringLiteral("first read"), QStringLiteral("craft")}));

        // allYielding sees every link; booksYielding (display) only mastered ones.
        std::vector<QString> allYielding = titles(memories->allYielding(memory.id));
        std::sort(allYielding.begin(), allYielding.end());
        QCOMPARE(allYielding, (std::vector<QString>{QStringLiteral("A Light in the Inkwell"),
                                                    QStringLiteral("The Carbonek Schism")}));
        QCOMPARE(titles(memories->booksYielding(memory.id)),
                 (std::vector<QString>{QStringLiteral("A Light in the Inkwell")}));

        // Sync links to just B: A unlinked, B kept — and cross-links move.
        const Memory other = memories->insert(memoryDraft(QStringLiteral("Memory: Hindsight"),
                                                          MemoryKind::Memory, false));
        memories->setYieldingBooks(memory.id, {bookB.id});
        memories->setYieldingBooks(other.id, {bookA.id});
        QVERIFY(books->get(bookB.id)->yieldedMemoryID == std::optional<qint64>(memory.id));
        QVERIFY2(books->get(bookA.id)->yieldedMemoryID == std::optional<qint64>(other.id),
                 "re-links move the book");

        memories->setYieldingBooks(memory.id, {});
        QVERIFY2(!books->get(bookB.id)->yieldedMemoryID.has_value(),
                 "empty list clears every link");
        QCOMPARE((int) memories->all().size(), 2);
    }

    /// Reading Helper candidates exclude unearned memories; mastering flips them in.
    void candidatesExcludeUnearnedMemories()
    {
        const qint64 sky = principle(QStringLiteral("Sky"));
        const Memory earned = memories->insert(memoryDraft(QStringLiteral("Memory: Impulse"),
                                                           MemoryKind::Memory, false));
        memories->setAspects(earned.id, {AspectDraft{sky, 4}});
        const Memory unearned = memories->insert(memoryDraft(QStringLiteral("Memory: Pattern"),
                                                             MemoryKind::Memory, false));
        memories->setAspects(unearned.id, {AspectDraft{sky, 6}});
        BookDraft unreadDraft = bookDraft(QStringLiteral("Travelling at Night"));
        unreadDraft.yieldedMemoryID = unearned.id;
        const Book unreadBook = books->insert(unreadDraft);

        std::vector<QString> candidates = names(memories->candidates(sky, 1));
        QCOMPARE(candidates, (std::vector<QString>{QStringLiteral("Memory: Impulse")}));
        QVERIFY(memories->candidates(sky, 5).empty());

        books->updateReadStatus(unreadBook.id, ReadStatus::Mastered);
        candidates = names(memories->candidates(sky, 1));
        QCOMPARE(candidates, (std::vector<QString>{QStringLiteral("Memory: Pattern"),
                                                   QStringLiteral("Memory: Impulse")}));
        candidates = names(memories->candidates(sky, 5));
        QCOMPARE(candidates, (std::vector<QString>{QStringLiteral("Memory: Pattern")}));
    }

    void setYieldedMemoryAndLessonsCount()
    {
        const Memory memory = memories->insert(memoryDraft(QStringLiteral("Occult Scrap"),
                                                           MemoryKind::Memory, true));
        const Book book = books->insert(bookDraft(QStringLiteral("Yellowing Newspaper")));

        books->setYieldedMemory(book.id, memory.id);
        books->setLessonsCount(book.id, 2);
        auto reloaded = books->get(book.id);
        QVERIFY(reloaded->yieldedMemoryID == std::optional<qint64>(memory.id));
        QVERIFY(reloaded->lessons == std::optional<int>(2));

        books->setYieldedMemory(book.id, {});
        books->setLessonsCount(book.id, {});
        reloaded = books->get(book.id);
        QVERIFY(!reloaded->yieldedMemoryID.has_value());
        QVERIFY(!reloaded->lessons.has_value());
    }

    void bookLessonsJunctionCascades()
    {
        const qint64 sky = principle(QStringLiteral("Sky"));
        const qint64 heart = principle(QStringLiteral("Heart"));
        SkillDraft stringsDraft;
        stringsDraft.name = QStringLiteral("Strings & Songs");
        stringsDraft.primaryPrincipleID = sky;
        stringsDraft.secondaryPrincipleID = heart;
        stringsDraft.level = 1;
        const Skill stringsAndSongs = skills->insert(stringsDraft);
        BookDraft draft = bookDraft(QStringLiteral("Opening the Sky"));
        draft.lessons = 2;
        const Book book = books->insert(draft);
        books->setLessons(book.id, {BookLessonsEntry{stringsAndSongs.id, 2}});
        const auto entries = books->lessons(book.id);
        QCOMPARE((int) entries.size(), 1);
        QVERIFY((entries[0] == BookLessonsEntry{stringsAndSongs.id, 2}));

        // Deleting the book removes its junction rows (ON DELETE CASCADE).
        books->remove(book.id);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM BookLessons;")), 0);
    }

    // MARK: - Skills

    void skillLifecycleAndContributions()
    {
        const qint64 sky = principle(QStringLiteral("Sky"));
        const qint64 rose = principle(QStringLiteral("Rose"));

        SkillDraft draft;
        draft.name = QStringLiteral("Sky Stories");
        draft.primaryPrincipleID = sky;
        draft.secondaryPrincipleID = rose;
        draft.level = 3;
        const Skill skyStories = skills->insert(draft);
        QVERIFY(skyStories.level == std::optional<int>(3));

        Skill edited = skyStories;
        edited.level = 4;
        edited.wisdom = QStringLiteral("Birdsong");
        edited.element = QStringLiteral("Trist");
        skills->update(edited);
        const auto reloaded = skills->get(skyStories.id);
        QVERIFY(reloaded->level == std::optional<int>(4));
        QCOMPARE(*reloaded->wisdom, QStringLiteral("Birdsong"));

        // Canonical query 2: level-4 skill contributes 5 primary, 4 secondary.
        QCOMPARE((int) skills->contributions(sky).size(), 1);
        QCOMPARE(skills->contributions(sky).front().contributes, 5);
        QCOMPARE(skills->contributions(rose).front().contributes, 4);

        // A primary-Rose skill at level 3 also contributes 4 — the tie with
        // Sky Stories breaks alphabetically (ORDER BY contributes DESC, name).
        SkillDraft inksDraft;
        inksDraft.name = QStringLiteral("Inks of Power");
        inksDraft.primaryPrincipleID = rose;
        inksDraft.secondaryPrincipleID = sky;
        inksDraft.level = 3;
        skills->insert(inksDraft);
        const auto best = skills->contributions(rose).front();
        QCOMPARE(best.skill.name, QStringLiteral("Inks of Power"));
        QCOMPARE(best.contributes, 4);

        skills->remove(skyStories.id);
        QVERIFY(!skills->get(skyStories.id).has_value());
    }

    void languagesDoNotCountAsSkillContributions()
    {
        const qint64 knock = principle(QStringLiteral("Knock"));
        SkillDraft vakDraft;
        vakDraft.name = QStringLiteral("Vak");
        vakDraft.isLanguage = true;
        vakDraft.primaryPrincipleID = knock;
        vakDraft.level = 2;
        skills->insert(vakDraft);
        QVERIFY2(skills->contributions(knock).empty(),
                 "languages slot into reading, not into the desk math");
    }

    // MARK: - Journal

    void journalLifecycle()
    {
        const Book book = books->insert(bookDraft(QStringLiteral("Travelling at Night, vol 1")));
        const Memory memory = memories->insert(memoryDraft(QStringLiteral("Memory: Impulse"),
                                                           MemoryKind::Memory, false));

        const JournalEntry entry = journal->insert(JournalDraft{
            QStringLiteral("Year 1, Spring, day 2"),
            QStringLiteral("Keeper's Lodge book gives Impulse on every re-read."), book.id,
            memory.id, {}});
        QVERIFY(!entry.loggedAt.isEmpty());

        const auto recentEntries = journal->recent();
        QCOMPARE((int) recentEntries.size(), 1);
        QCOMPARE(recentEntries.front().entry, entry.entry);
        QVERIFY(recentEntries.front().bookID == std::optional<qint64>(book.id));

        // Entity filtering.
        QCOMPARE((int) journal->entries(book.id).size(), 1);
        QCOMPARE((int) journal->entries({}, memory.id).size(), 1);
        QCOMPARE((int) journal->entries({}, {}, book.id).size(), 0);

        // Newest first.
        journal->insert(JournalDraft{{}, QStringLiteral("second entry"), {}, {}, {}});
        QCOMPARE(journal->recent().front().entry, QStringLiteral("second entry"));

        // Update + delete.
        JournalEntry edited = entry;
        edited.entry = QStringLiteral("corrected note");
        journal->update(edited);
        QCOMPARE(journal->get(entry.id)->entry, QStringLiteral("corrected note"));
        journal->remove(entry.id);
        QCOMPARE((int) journal->recent().size(), 1);
    }
};

QTEST_MAIN(RepositoryTests)
#include "test_Repositories.moc"
