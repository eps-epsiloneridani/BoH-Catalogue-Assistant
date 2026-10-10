// Port of app/Tests/BoHLibrarianCoreTests/PlaythroughRepositoryTests.swift (Task 6).
#include <QtTest/QtTest>

#include "Migrator.h"
#include "Repositories/BookRepository.h"
#include "Repositories/JournalRepository.h"
#include "Repositories/LookupRepositories.h"
#include "Repositories/MemoryRepository.h"
#include "Repositories/PlaythroughRepository.h"
#include "Repositories/SkillRepository.h"
#include "SQLiteDatabase.h"

#include <algorithm>
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

class PlaythroughRepositoryTests final : public QObject {
    Q_OBJECT

    std::unique_ptr<SQLiteDatabase> m_db;
    std::unique_ptr<PlaythroughRepository> repo;

private slots:
    void init()
    {
        m_db = std::make_unique<SQLiteDatabase>(QStringLiteral(":memory:"));
        Migrator(Migrator::bundled()).apply(*m_db);
        repo = std::make_unique<PlaythroughRepository>(*m_db);
    }

    void cleanup()
    {
        repo.reset();
        m_db.reset();
    }

    void defaultPlaythroughIsSeededAndActive()
    {
        const auto active = repo->active();
        QVERIFY(active.has_value());
        QCOMPARE(active->id, qint64(1));
        QCOMPARE(active->name, QStringLiteral("First playthrough"));
        QVERIFY(repo->activeID() == std::optional<qint64>(1));
    }

    void insertSetAndGetRoundTrip()
    {
        const Playthrough second = repo->insert(QStringLiteral("The Twice-Born, Brancrug"),
                                                QStringLiteral("second run"));
        QCOMPARE(second.name, QStringLiteral("The Twice-Born, Brancrug"));
        QCOMPARE((int) repo->all().size(), 2);

        repo->setActiveID(second.id);
        QVERIFY(repo->activeID() == std::optional<qint64>(second.id));
        QVERIFY(repo->active()->id == second.id);

        Playthrough edited = second;
        edited.name = QStringLiteral("The Twice-Born (retired)");
        edited.notes = QStringLiteral("abandoned before Numa");
        repo->update(edited);
        QCOMPARE(repo->get(second.id)->name, QStringLiteral("The Twice-Born (retired)"));
    }

    void scopingIsolatesPlaythroughs()
    {
        const auto first = repo->active();
        QVERIFY(first.has_value());
        const Playthrough second = repo->insert(QStringLiteral("Run 2"));

        BookRepository booksA(*m_db, first->id);
        BookRepository booksB(*m_db, second.id);
        MemoryRepository memoriesA(*m_db, first->id);
        MemoryRepository memoriesB(*m_db, second.id);
        JournalRepository journalB(*m_db, second.id);

        BookDraft turquoiseDraft = bookDraft(QStringLiteral("The Turquoise Hand"));
        turquoiseDraft.difficulty = 10;
        const Book turquoise = booksA.insert(turquoiseDraft);
        memoriesA.insert(memoryDraft(QStringLiteral("Horizon-Sight"), MemoryKind::Memory, true));
        BookDraft deHorisDraft = bookDraft(QStringLiteral("De Horis book 1"));
        deHorisDraft.difficulty = 4;
        const Book newBook = booksB.insert(deHorisDraft);
        journalB.insert(JournalDraft{{}, QStringLiteral("new run, fresh library"), {}, {}, {}});

        // Run A sees only its own rows…
        QCOMPARE(titles(booksA.all()), (std::vector<QString>{QStringLiteral("The Turquoise Hand")}));
        // …run B sees only its own…
        QCOMPARE(titles(booksB.all()), (std::vector<QString>{QStringLiteral("De Horis book 1")}));
        QVERIFY(memoriesB.all().empty());
        // …and cross-playthrough lookups fail closed.
        QVERIFY(!booksB.get(turquoise.id).has_value());
        QCOMPARE((int) journalB.entries(newBook.id).size(), 0);
    }

    void deletingPlaythroughCascadesFindings()
    {
        const Playthrough doomed = repo->insert(QStringLiteral("Doomed run"));
        BookRepository books(*m_db, doomed.id);
        MemoryRepository memories(*m_db, doomed.id);
        SkillRepository skills(*m_db, doomed.id);
        JournalRepository journal(*m_db, doomed.id);

        qint64 roseID = 0;
        for (const Principle& p : PrincipleRepository(*m_db).all()) {
            if (p.name == QStringLiteral("Rose"))
                roseID = p.id;
        }
        QVERIFY(roseID != 0);

        BookDraft draft = bookDraft(QStringLiteral("Just Verse"));
        draft.mysteryPrincipleID = roseID;
        draft.difficulty = 6;
        const Book book = books.insert(draft);
        MemoryDraft patternDraft = memoryDraft(QStringLiteral("Pattern"), MemoryKind::Memory, false);
        patternDraft.aspects = {AspectDraft{roseID, 2}};
        const Memory memory = memories.insert(patternDraft);
        SkillDraft skillDraft;
        skillDraft.name = QStringLiteral("Preliminal Meter");
        skillDraft.primaryPrincipleID = roseID;
        skillDraft.level = 1;
        const Skill skill = skills.insert(skillDraft);
        books.setLessons(book.id, {BookLessonsEntry{skill.id, 1}});
        journal.insert(JournalDraft{{}, QStringLiteral("started doomed run"), {}, {}, {}});

        repo->remove(doomed.id);

        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Books WHERE playthrough_id = ?;"),
                                 {SQLiteValue(doomed.id)}),
                 0);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Memories WHERE playthrough_id = ?;"),
                                 {SQLiteValue(doomed.id)}),
                 0);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Skills WHERE playthrough_id = ?;"),
                                 {SQLiteValue(doomed.id)}),
                 0);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM Journal WHERE playthrough_id = ?;"),
                                 {SQLiteValue(doomed.id)}),
                 0);
        // Junction rows cascade through their parents.
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM BookLessons;")), 0);
        QCOMPARE(m_db->scalarInt(QStringLiteral("SELECT COUNT(*) FROM MemoryAspects;")), 0);
        // The default playthrough is untouched.
        QCOMPARE((int) repo->all().size(), 1);
        Q_UNUSED(memory);
    }
};

QTEST_MAIN(PlaythroughRepositoryTests)
#include "test_PlaythroughRepository.moc"
