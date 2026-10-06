import XCTest
@testable import BoHLibrarianCore

/// Repository tests run against a fresh in-memory database with the real
/// migrations applied (schema v3 + seeds). Fixtures mirror the examples in
/// docs/DATABASE.md / docs/GAME_MECHANICS.md.
final class RepositoryTests: XCTestCase {

    private var db: SQLiteDatabase!
    private var memories: MemoryRepository!
    private var books: BookRepository!
    private var skills: SkillRepository!
    private var journal: JournalRepository!
    private var principles: PrincipleRepository!
    private var languages: LanguageRepository!

    override func setUpWithError() throws {
        db = try SQLiteDatabase(path: ":memory:")
        try Migrator(migrations: Migrator.bundled()).apply(to: db)
        // Migration 005 seeds a default playthrough and marks it active; the
        // repositories under test are scoped to it.
        let playthrough = try XCTUnwrap(PlaythroughRepository(db: db).active(),
                                        "migration 005 must seed a default playthrough")
        memories = MemoryRepository(db: db, playthroughID: playthrough.id)
        books = BookRepository(db: db, playthroughID: playthrough.id)
        skills = SkillRepository(db: db, playthroughID: playthrough.id)
        journal = JournalRepository(db: db, playthroughID: playthrough.id)
        principles = PrincipleRepository(db: db)
        languages = LanguageRepository(db: db)
    }

    override func tearDown() {
        db = nil
    }

    private func principle(_ name: String) throws -> Principle {
        try XCTUnwrap(try principles.all().first { $0.name == name }, "principle \(name) missing from seeds")
    }

    // MARK: - Seeded lookups

    func testAllThirteenPrinciplesSeededIncludingLanternAndNectar() throws {
        let all = try principles.all()
        XCTAssertEqual(all.count, 13)
        XCTAssertEqual(all.map(\.name), all.map(\.name).sorted())  // ordered by sort_order == alphabetical
        // The two the pre-project schema missed (docs/DECISIONS.md D3):
        XCTAssertNotNil(all.first { $0.name == "Lantern" })
        XCTAssertNotNil(all.first { $0.name == "Nectar" })
        XCTAssertTrue(all.allSatisfy { $0.color != nil }, "principles need UI badge colors")
    }

    func testPrincipleUpdate() throws {
        var rose = try principle("Rose")
        rose.color = "#FFC0CB"
        rose.notes = "the rose which encompasseth all"
        try principles.update(rose)
        let reloaded = try principle("Rose")
        XCTAssertEqual(reloaded.color, "#FFC0CB")
        XCTAssertEqual(reloaded.notes, "the rose which encompasseth all")
    }

    func testLanguagesSeededAndEditable() throws {
        let all = try languages.all()
        XCTAssertEqual(all.count, 15)
        XCTAssertEqual(all.filter(\.native).count, 5)
        let nativeNames = Set(all.filter(\.native).map(\.name))
        XCTAssertEqual(nativeNames, ["Latin", "Greek", "Sanskrit", "Aramaic", "Phrygian"])

        let added = try languages.insert(name: "Testamese", native: false)
        XCTAssertEqual(try languages.all().count, 16)
        try languages.delete(added.id)
        XCTAssertEqual(try languages.all().count, 15)
    }

    // MARK: - Memories

    func testMemoryLifecycleWithAspectsAndSources() throws {
        let rose = try principle("Rose").id
        let knock = try principle("Knock").id
        let moon = try principle("Moon").id

        let horizon = try memories.insert(MemoryDraft(
            name: "Horizon-Sight", kind: .memory, persistent: true,
            notes: "persistent Rose memory",
            aspects: [AspectDraft(principleID: rose, level: 4)]
        ))
        XCTAssertEqual(horizon.aspects, [Aspect(principleID: rose, principleName: "Rose", level: 4)])

        // Replace aspects wholesale.
        try memories.setAspects(horizon.id, [
            AspectDraft(principleID: knock, level: 3),
            AspectDraft(principleID: moon, level: 3),
        ])
        let reloaded = try memories.get(horizon.id)
        XCTAssertEqual(reloaded?.aspects.map(\.principleName), ["Knock", "Moon"],
                       "aspects order should follow principle sort order")

        // Update scalar fields without touching aspects via the transactional path.
        var edited = reloaded!
        edited.name = "Horizon-Sight (confirmed)"
        edited.notes = "gained by discarding a penny at Sea's Edge"
        try memories.update(edited)
        XCTAssertEqual(try memories.get(horizon.id)?.name, "Horizon-Sight (confirmed)")
        XCTAssertEqual(try memories.get(horizon.id)?.aspects.count, 2)

        // Sources.
        try memories.addSource(horizon.id, kind: MemorySourceKind.craft.rawValue, detail: "Keeper 00 Rose")
        try memories.addSource(horizon.id, kind: MemorySourceKind.numa.rawValue, detail: "gathering at Sea's Edge (25%)")
        try memories.addSource(horizon.id, kind: MemorySourceKind.numa.rawValue, detail: "gathering at Sea's Edge (25%)")  // duplicate ignored
        XCTAssertEqual(try memories.sources(for: horizon.id).count, 2)
        try memories.removeSource(horizon.id, kind: MemorySourceKind.craft.rawValue, detail: "Keeper 00 Rose")
        XCTAssertEqual(try memories.sources(for: horizon.id).count, 1)

        // Delete cascades aspects + sources.
        try memories.delete(horizon.id)
        XCTAssertNil(try memories.get(horizon.id))
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM MemoryAspects;"), 0)
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM MemorySources;"), 0)
    }

    func testReadingHelperMemoryCandidates() throws {
        let rose = try principle("Rose").id
        let knock = try principle("Knock").id
        let sky = try principle("Sky").id
        let winter = try principle("Winter").id

        try memories.insert(MemoryDraft(name: "Horizon-Sight", kind: .memory, persistent: true,
                                         aspects: [AspectDraft(principleID: rose, level: 4)]))
        try memories.insert(MemoryDraft(name: "Fog", kind: .weather, persistent: false,
                                         aspects: [AspectDraft(principleID: knock, level: 3)]))
        let numen = try memories.insert(MemoryDraft(
            name: "Numen: That Old Lost Music", kind: .numen, persistent: true,
            aspects: [AspectDraft(principleID: rose, level: 5),
                      AspectDraft(principleID: sky, level: 5),
                      AspectDraft(principleID: winter, level: 5)]
        ))

        // Rose >= 4 → the numen (level 5) first, then Horizon-Sight (4).
        let roseCandidates = try memories.candidates(principleID: rose, minLevel: 4)
        XCTAssertEqual(roseCandidates.map(\.name), ["Numen: That Old Lost Music", "Horizon-Sight"])
        XCTAssertEqual(roseCandidates.first?.kind, .numen)

        // Out of reach.
        XCTAssertTrue(try memories.candidates(principleID: rose, minLevel: 10).isEmpty)

        // Weather counts as a candidate source.
        XCTAssertEqual(try memories.candidates(principleID: knock, minLevel: 3).map(\.name), ["Fog"])

        // Exact minimum is inclusive.
        XCTAssertEqual(try memories.candidates(principleID: sky, minLevel: 5).map(\.name),
                       [numen.name])
    }

    // MARK: - Books

    func testBookLifecycle() throws {
        let rose = try principle("Rose").id
        let fucine = try languages.all().first { $0.name == "Fucine" }!.id

        let book = try books.insert(BookDraft(
            title: "The Turquoise Hand",
            bookKind: .book,
            languageID: fucine,
            mysteryPrincipleID: rose,
            difficulty: 10,
            readStatus: .catalogued,
            contamination: .clear,
            location: "Silver Vault",
            lessons: 2
        ))
        XCTAssertEqual(book.title, "The Turquoise Hand")
        XCTAssertEqual(book.difficulty, 10)
        XCTAssertEqual(book.readStatus, .catalogued)
        XCTAssertEqual(book.contamination, .clear)
        XCTAssertEqual(book.timesRead, 0)

        // Reads.
        try books.recordRead(book.id)
        try books.updateReadStatus(book.id, .mastered)
        var reloaded = try books.get(book.id)
        XCTAssertEqual(reloaded?.timesRead, 1)
        XCTAssertEqual(reloaded?.readStatus, .mastered)
        XCTAssertNotNil(reloaded?.firstReadAt)
        XCTAssertEqual(reloaded?.firstReadAt, reloaded?.lastReadAt)
        try books.recordRead(book.id)
        reloaded = try books.get(book.id)
        XCTAssertEqual(reloaded?.timesRead, 2)

        // Generic update preserves read counters.
        var edited = reloaded!
        edited.title = "The Turquoise Hand (glossed)"
        try books.update(edited)
        XCTAssertEqual(try books.get(book.id)?.timesRead, 2)
        XCTAssertEqual(try books.get(book.id)?.title, "The Turquoise Hand (glossed)")

        try books.delete(book.id)
        XCTAssertNil(try books.get(book.id))
    }

    func testYieldedMemoryLinkClearsWhenMemoryDeleted() throws {
        let horizon = try memories.insert(MemoryDraft(name: "Horizon-Sight", kind: .memory, persistent: true))
        let book = try books.insert(BookDraft(title: "Towards a Fundamental Aesthetic",
                                              yieldedMemoryID: horizon.id))
        XCTAssertEqual(try books.get(book.id)?.yieldedMemoryID, horizon.id)

        try memories.delete(horizon.id)
        XCTAssertNil(try books.get(book.id)?.yieldedMemoryID, "ON DELETE SET NULL must clear the link")
    }

    func testBooksYieldingBacklink() throws {
        let memory = try memories.insert(MemoryDraft(name: "Memory: Revelation", kind: .memory, persistent: false))
        let mastered = try books.insert(BookDraft(title: "Gospel of Nicodemus", yieldedMemoryID: memory.id))
        let unread = try books.insert(BookDraft(title: "A Light in the Inkwell", yieldedMemoryID: memory.id))
        try books.insert(BookDraft(title: "Sunrise Awakenings", yieldedMemoryID: nil))
        try books.updateReadStatus(mastered.id, .mastered)

        // Spoiler policy: only books the player has mastered are displayed backlinks —
        // the recorded-but-unread book keeps its link privately.
        XCTAssertEqual(try memories.booksYielding(memory.id).map(\.title), ["Gospel of Nicodemus"])

        // Once the player masters the unread book, its yield becomes theirs to see.
        try books.updateReadStatus(unread.id, .mastered)
        XCTAssertEqual(try memories.booksYielding(memory.id).map(\.title),
                       ["A Light in the Inkwell", "Gospel of Nicodemus"])
    }

    /// Earned-only list: a memory is visible when it has no yield links (player-made)
    /// or a mastered yielding book; imports carry yields of unmastered books that must
    /// stay out of the list while remaining in the table for lookups.
    func testAllKnownHidesUnearnedYieldMemories() throws {
        let earned = try memories.insert(MemoryDraft(name: "Memory: Revelation", kind: .memory, persistent: false))
        let handMade = try memories.insert(MemoryDraft(name: "Weather: Drizzle", kind: .weather, persistent: false))
        let unearned = try memories.insert(MemoryDraft(name: "Numen: a Final Understanding", kind: .numen, persistent: true))
        _ = try books.insert(BookDraft(title: "Gospel of Nicodemus", yieldedMemoryID: earned.id))
        let masteredBook = try books.insert(BookDraft(title: "A Light in the Inkwell", yieldedMemoryID: earned.id))
        try books.updateReadStatus(masteredBook.id, .mastered)
        _ = try books.insert(BookDraft(title: "The Carbonek Schism", yieldedMemoryID: unearned.id))

        let known = try memories.allKnown().map(\.name)
        XCTAssertTrue(known.contains("Memory: Revelation"),
                      "a mastered yielding link earns visibility even with unmastered links")
        XCTAssertTrue(known.contains("Weather: Drizzle"), "no yield links = player-created")
        XCTAssertFalse(known.contains("Numen: a Final Understanding"),
                       "only unmastered yielding books = not yet known")

        // Earning it flips it into the list; the table keeps all rows for lookups.
        let bookID = try db.scalarInt(
            "SELECT id FROM Books WHERE title = 'The Carbonek Schism';")
        try books.updateReadStatus(Int64(bookID), .mastered)
        XCTAssertTrue(try memories.allKnown().map(\.name).contains("Numen: a Final Understanding"))
        XCTAssertEqual(try memories.all().count, 3)
    }

    func testSetYieldedMemoryAndLessonsCount() throws {
        let memory = try memories.insert(MemoryDraft(name: "Occult Scrap", kind: .memory, persistent: true))
        let book = try books.insert(BookDraft(title: "Yellowing Newspaper"))

        try books.setYieldedMemory(book.id, memoryID: memory.id)
        try books.setLessonsCount(book.id, lessons: 2)
        var reloaded = try books.get(book.id)
        XCTAssertEqual(reloaded?.yieldedMemoryID, memory.id)
        XCTAssertEqual(reloaded?.lessons, 2)

        try books.setYieldedMemory(book.id, memoryID: nil)
        try books.setLessonsCount(book.id, lessons: nil)
        reloaded = try books.get(book.id)
        XCTAssertNil(reloaded?.yieldedMemoryID)
        XCTAssertNil(reloaded?.lessons)
    }

    func testBookLessonsJunctionCascades() throws {
        let sky = try principle("Sky").id
        let heart = try principle("Heart").id
        let stringsAndSongs = try skills.insert(SkillDraft(name: "Strings & Songs",
                                                           primaryPrincipleID: sky,
                                                           secondaryPrincipleID: heart, level: 1))
        let book = try books.insert(BookDraft(title: "Opening the Sky", lessons: 2))
        try books.setLessons(book.id, [BookLessonsEntry(skillID: stringsAndSongs.id, amount: 2)])
        XCTAssertEqual(try books.lessons(forBook: book.id), [BookLessonsEntry(skillID: stringsAndSongs.id, amount: 2)])

        // Deleting the book removes its junction rows (ON DELETE CASCADE).
        try books.delete(book.id)
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM BookLessons;"), 0)
    }

    // MARK: - Skills

    func testSkillLifecycleAndContributions() throws {
        let sky = try principle("Sky").id
        let rose = try principle("Rose").id

        let skyStories = try skills.insert(SkillDraft(name: "Sky Stories",
                                                      primaryPrincipleID: sky,
                                                      secondaryPrincipleID: rose, level: 3))
        XCTAssertEqual(skyStories.level, 3)

        var edited = skyStories
        edited.level = 4
        edited.wisdom = "Birdsong"
        edited.element = "Trist"
        try skills.update(edited)
        let reloaded = try skills.get(skyStories.id)
        XCTAssertEqual(reloaded?.level, 4)
        XCTAssertEqual(reloaded?.wisdom, "Birdsong")

        // Canonical query 2: level-4 skill contributes 5 primary, 4 secondary.
        let skyContribs = try skills.contributions(principleID: sky)
        XCTAssertEqual(skyContribs.first?.contributes, 5)
        let roseContribs = try skills.contributions(principleID: rose)
        XCTAssertEqual(roseContribs.first?.contributes, 4)

        // A primary-Rose skill at level 3 also contributes 4 — the tie with
        // Sky Stories breaks alphabetically (ORDER BY contributes DESC, name).
        try skills.insert(SkillDraft(name: "Inks of Power", primaryPrincipleID: rose,
                                     secondaryPrincipleID: sky, level: 3))
        let best = try skills.contributions(principleID: rose).first
        XCTAssertEqual(best?.skill.name, "Inks of Power")
        XCTAssertEqual(best?.contributes, 4)

        try skills.delete(skyStories.id)
        XCTAssertNil(try skills.get(skyStories.id))
    }

    func testLanguagesDoNotCountAsSkillContributions() throws {
        let knock = try principle("Knock").id
        try skills.insert(SkillDraft(name: "Vak", isLanguage: true,
                                     primaryPrincipleID: knock, level: 2))
        XCTAssertTrue(try skills.contributions(principleID: knock).isEmpty,
                      "languages slot into reading, not into the desk math")
    }

    // MARK: - Journal

    func testJournalLifecycle() throws {
        let book = try books.insert(BookDraft(title: "Travelling at Night, vol 1"))
        let memory = try memories.insert(MemoryDraft(name: "Memory: Impulse", kind: .memory, persistent: false))

        let entry = try journal.insert(JournalDraft(
            gameDay: "Year 1, Spring, day 2",
            entry: "Keeper's Lodge book gives Impulse on every re-read.",
            bookID: book.id, memoryID: memory.id
        ))
        XCTAssertFalse(entry.loggedAt.isEmpty)

        let recent = try journal.recent()
        XCTAssertEqual(recent.first?.entry, entry.entry)
        XCTAssertEqual(recent.first?.bookID, book.id)

        // Entity filtering.
        XCTAssertEqual(try journal.entries(bookID: book.id).count, 1)
        XCTAssertEqual(try journal.entries(memoryID: memory.id).count, 1)
        XCTAssertEqual(try journal.entries(skillID: book.id).count, 0)

        // Newest first.
        try journal.insert(JournalDraft(entry: "second entry"))
        XCTAssertEqual(try journal.recent().first?.entry, "second entry")

        // Update + delete.
        var edited = entry
        edited.entry = "corrected note"
        try journal.update(edited)
        XCTAssertEqual(try journal.get(entry.id)?.entry, "corrected note")
        try journal.delete(entry.id)
        XCTAssertEqual(try journal.recent().count, 1)
    }
}