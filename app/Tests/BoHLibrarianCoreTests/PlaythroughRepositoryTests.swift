import XCTest
@testable import BoHLibrarianCore

/// Playthrough lifecycle: CRUD, the active-playthrough Meta pointer, scoping
/// of the finding tables, and cascade deletion (docs/DATABASE.md §Playthroughs).
final class PlaythroughRepositoryTests: XCTestCase {

    private var db: SQLiteDatabase!
    private var repo: PlaythroughRepository!

    override func setUpWithError() throws {
        db = try SQLiteDatabase(path: ":memory:")
        try Migrator(migrations: Migrator.bundled()).apply(to: db)
        repo = PlaythroughRepository(db: db)
    }

    override func tearDown() {
        db = nil
    }

    func testDefaultPlaythroughIsSeededAndActive() throws {
        let active = try XCTUnwrap(repo.active())
        XCTAssertEqual(active.id, 1)
        XCTAssertEqual(active.name, "First playthrough")
        XCTAssertEqual(try repo.activeID(), 1)
    }

    func testInsertSetAndGetRoundTrip() throws {
        let second = try repo.insert(name: "The Twice-Born, Brancrug", notes: "second run")
        XCTAssertEqual(second.name, "The Twice-Born, Brancrug")
        XCTAssertEqual(try repo.all().count, 2)

        try repo.setActiveID(second.id)
        XCTAssertEqual(try repo.activeID(), second.id)
        XCTAssertEqual(try repo.active()?.id, second.id)

        var edited = second
        edited.name = "The Twice-Born (retired)"
        edited.notes = "abandoned before Numa"
        try repo.update(edited)
        XCTAssertEqual(try repo.get(second.id)?.name, "The Twice-Born (retired)")
    }

    func testScopingIsolatesPlaythroughs() throws {
        let first = try XCTUnwrap(repo.active())
        let second = try repo.insert(name: "Run 2")

        let booksA = BookRepository(db: db, playthroughID: first.id)
        let booksB = BookRepository(db: db, playthroughID: second.id)
        let memoriesA = MemoryRepository(db: db, playthroughID: first.id)
        let memoriesB = MemoryRepository(db: db, playthroughID: second.id)
        let journalB = JournalRepository(db: db, playthroughID: second.id)

        let turquoise = try booksA.insert(BookDraft(title: "The Turquoise Hand", difficulty: 10))
        let horizon = try memoriesA.insert(MemoryDraft(name: "Horizon-Sight", kind: .memory, persistent: true))
        let newBook = try booksB.insert(BookDraft(title: "De Horis book 1", difficulty: 4))
        _ = try journalB.insert(JournalDraft(entry: "new run, fresh library"))

        // Run A sees only its own rows…
        XCTAssertEqual(try booksA.all().map(\.title), ["The Turquoise Hand"])
        XCTAssertEqual(try memoriesA.all().map(\.name), ["Horizon-Sight"])
        // …run B sees only its own…
        XCTAssertEqual(try booksB.all().map(\.title), ["De Horis book 1"])
        XCTAssertTrue(try memoriesB.all().isEmpty)
        // …and cross-playthrough lookups fail closed.
        XCTAssertNil(try booksB.get(turquoise.id))
        XCTAssertNil(try memoriesB.get(horizon.id))
        XCTAssertEqual(try journalB.entries(bookID: newBook.id).count, 0)
    }

    func testDeletingPlaythroughCascadesFindings() throws {
        let doomed = try repo.insert(name: "Doomed run")
        let books = BookRepository(db: db, playthroughID: doomed.id)
        let memories = MemoryRepository(db: db, playthroughID: doomed.id)
        let skills = SkillRepository(db: db, playthroughID: doomed.id)
        let journal = JournalRepository(db: db, playthroughID: doomed.id)

        let rose = try XCTUnwrap(PrincipleRepository(db: db).all().first { $0.name == "Rose" })
        let book = try books.insert(BookDraft(title: "Just Verse", mysteryPrincipleID: rose.id, difficulty: 6))
        let memory = try memories.insert(MemoryDraft(name: "Pattern", kind: .memory, persistent: false,
                                                      aspects: [AspectDraft(principleID: rose.id, level: 2)]))
        let skill = try skills.insert(SkillDraft(name: "Preliminal Meter", isLanguage: false,
                                                 primaryPrincipleID: rose.id, level: 1))
        try books.setLessons(book.id, [BookLessonsEntry(skillID: skill.id, amount: 1)])
        try journal.insert(JournalDraft(entry: "started doomed run"))

        try repo.delete(doomed.id)

        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM Books WHERE playthrough_id = ?;", [doomed.id]), 0)
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM Memories WHERE playthrough_id = ?;", [doomed.id]), 0)
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM Skills WHERE playthrough_id = ?;", [doomed.id]), 0)
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM Journal WHERE playthrough_id = ?;", [doomed.id]), 0)
        // Junction rows cascade through their parents.
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM BookLessons;"), 0)
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM MemoryAspects;"), 0)
        // The default playthrough is untouched.
        XCTAssertEqual(try repo.all().count, 1)
        _ = book
        _ = memory
        _ = skill
    }
}