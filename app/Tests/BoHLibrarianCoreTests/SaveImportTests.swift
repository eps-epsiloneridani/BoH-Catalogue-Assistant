import XCTest
@testable import BoHLibrarianCore

/// Save-import pipeline. Fixtures deliberately reproduce the game's lenient JSON
/// dialect (UTF-16 with BOM, trailing commas, raw control characters) so the
/// importer is tested against the same quirks as the real files. A final test
/// exercises the real installed game's data and live save when present.
final class SaveImportTests: XCTestCase {

    private var workDirectory: URL!

    override func setUpWithError() throws {
        workDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("boh-import-tests-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: workDirectory, withIntermediateDirectories: true)
    }

    override func tearDownWithError() throws {
        try? FileManager.default.removeItem(at: workDirectory)
    }

    // MARK: - Lenient JSON

    func testLenientJSONHandlesTrailingCommasAndControlCharacters() throws {
        let text = """
        { "elements": [ { "id": "t.x", "Label": "Line\u{0001}Break", "aspects": { "a": 1, }, },
        ], }
        """
        let object = try XCTUnwrap(LenientJSON.object(from: Data(text.utf8)) as? [String: Any])
        let elements = LenientJSON.dictionaries(object["elements"])
        XCTAssertEqual(elements.count, 1)
        XCTAssertEqual(LenientJSON.string(elements[0]["Label"]), "Line\u{0001}Break")
        XCTAssertEqual(LenientJSON.intValue(LenientJSON.dictionary(elements[0]["aspects"])?["a"]), 1)
    }

    func testLenientJSONHandlesUTF16WithBOM() throws {
        let text = "{ \"elements\": [ { \"id\": \"t.x\", \"Label\": \"Tête\", }, ], }"
        var data = Data([0xFF, 0xFE])
        data.append(text.data(using: .utf16LittleEndian)!)
        let object = try XCTUnwrap(LenientJSON.object(from: data) as? [String: Any])
        XCTAssertEqual(LenientJSON.dictionaries(object["elements"]).count, 1)
    }

    // MARK: - Fixtures

    private func writeFixture(_ name: String, contents: String,
                              utf16: Bool = false) throws -> URL {
        let url = workDirectory.appendingPathComponent(name)
        if utf16 {
            var data = Data([0xFF, 0xFE])
            data.append(contents.data(using: .utf16LittleEndian)!)
            try data.write(to: url)
        } else {
            try Data(contents.utf8).write(to: url)
        }
        return url
    }

    private func makeElementsDirectory() throws -> URL {
        let dir = workDirectory.appendingPathComponent("elements", isDirectory: true)
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        _ = try writeFixture("tomes.json", into: dir, contents: """
            { "elements": [
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
            ], }
            """)
        _ = try writeFixture("skills.json", into: dir, contents: """
            { "elements": [
              { "id": "s.skystories", "Label": "Sky Stories",
                "aspects": { "sky": 2, "rose": 1, "skill": 1, "w.horomachistry": 1 } },
              { "id": "s.fucine", "Label": "Fucine",
                "aspects": { "heart": 2, "knock": 1, "skill.language": 1, "skill": 1 } },
            ], }
            """, utf16: true)
        _ = try writeFixture("memories.json", into: dir, contents: """
            { "elements": [
              { "ID": "mem.impulse", "Label": "Memory: Impulse", "inherits": "_memory",
                "aspects": { "moth": 2, "boost.moth": 2, "nectar": 1, "boost.nectar": 1 } },
              { "ID": "numen.asce", "Label": "Numen: a Final Understanding", "inherits": "_numen",
                "aspects": { "forge": 5, "knock": 5, "lantern": 5 } },
            ], }
            """)
        return dir
    }

    private func makeSave() throws -> URL {
        try writeFixture("AUTOSAVE.json", contents: """
            {
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
            }
            """)
    }

    private func writeFixture(_ name: String, into directory: URL,
                              contents: String, utf16: Bool = false) throws -> URL {
        let url = directory.appendingPathComponent(name)
        if utf16 {
            var data = Data([0xFF, 0xFE])
            data.append(contents.data(using: .utf16LittleEndian)!)
            try data.write(to: url)
        } else {
            try Data(contents.utf8).write(to: url)
        }
        return url
    }

    // MARK: - Import

    private func makeMigratedDB() throws -> (SQLiteDatabase, Playthrough) {
        let db = try SQLiteDatabase(path: ":memory:")
        try Migrator(migrations: Migrator.bundled()).apply(to: db)
        let playthrough = try XCTUnwrap(PlaythroughRepository(db: db).active())
        return (db, playthrough)
    }

    func testImportCreatesBooksSkillsMemoriesAndJournal() throws {
        let (db, playthrough) = try makeMigratedDB()
        let elementsDirectory = try makeElementsDirectory()
        let saveURL = try makeSave()

        let report = try SaveImporter.run(saveURL: saveURL, db: db,
                                          playthroughID: playthrough.id,
                                          elementsDirectory: elementsDirectory)

        XCTAssertEqual(report.booksCreated, 4)
        XCTAssertEqual(report.booksUpdated, 0)
        XCTAssertEqual(report.skillsCreated, 2)
        XCTAssertEqual(report.memoriesCreated, 2)
        XCTAssertEqual(report.uncataloguedSkipped, 1, "uncatbooks are skipped")

        let books = try BookRepository(db: db, playthroughID: playthrough.id).all()
        XCTAssertEqual(Set(books.map(\.title)),
                       ["Test Book", "Cursed Book", "Test Record", "Test Scroll"])

        let testBook = try XCTUnwrap(books.first { $0.title == "Test Book" })
        XCTAssertEqual(testBook.difficulty, 4)
        XCTAssertEqual(testBook.readStatus, .mastered, "mastery.sky mutation")
        let fucine = try LanguageRepository(db: db).all().first { $0.name == "Fucine" }
        XCTAssertEqual(testBook.languageID, fucine?.id, "w.fucine aspect")
        XCTAssertNotNil(testBook.yieldedMemoryID)
        XCTAssertEqual(testBook.location, "Library — shelf D.3",
                       "location = humanized room > slot chain")
        XCTAssertNil(testBook.contamination, "defunct copies don't leak mutations")

        let lessons = try BookRepository(db: db, playthroughID: playthrough.id)
            .lessons(forBook: testBook.id)
        XCTAssertEqual(lessons, [BookLessonsEntry(skillID: try XCTUnwrap(
            SkillRepository(db: db, playthroughID: playthrough.id).all()
                .first { $0.name == "Sky Stories" }?.id), amount: 2)])

        let cursed = try XCTUnwrap(books.first { $0.title == "Cursed Book" })
        XCTAssertEqual(cursed.readStatus, .catalogued, "no mastery mutation")
        XCTAssertEqual(cursed.contamination, .winkwell)
        XCTAssertEqual(cursed.bookKind, .book,
                       "codex is the plain bound-book format — not a phonograph record")
        let testRecord = try XCTUnwrap(books.first { $0.title == "Test Record" })
        XCTAssertEqual(testRecord.bookKind, .record, "record.phonograph aspect")
        XCTAssertEqual(testRecord.readStatus, .catalogued)
        let testScroll = try XCTUnwrap(books.first { $0.title == "Test Scroll" })
        XCTAssertEqual(testScroll.bookKind, .scroll, "scroll aspect")
        let greek = try LanguageRepository(db: db).all().first { $0.name == "Greek" }
        XCTAssertEqual(cursed.languageID, greek?.id, "native language matched by name")

        let skills = try SkillRepository(db: db, playthroughID: playthrough.id).all()
        let skyStories = try XCTUnwrap(skills.first { $0.name == "Sky Stories" })
        XCTAssertEqual(skyStories.level, 2, "skill:1 mutation means level 2")
        XCTAssertEqual(skyStories.wisdom, "horomachistry")
        XCTAssertEqual(skyStories.element, "Trist")
        let fucineSkill = try XCTUnwrap(skills.first { $0.name == "Fucine" })
        XCTAssertTrue(fucineSkill.isLanguage, "skill.language marker")

        let memories = try MemoryRepository(db: db, playthroughID: playthrough.id).all()
        let impulse = try XCTUnwrap(memories.first { $0.name == "Memory: Impulse" })
        XCTAssertFalse(impulse.persistent)
        XCTAssertEqual(impulse.aspects.map(\.level).sorted(), [1, 2], "moth 2, nectar 1")
        let numen = try XCTUnwrap(memories.first { $0.name.hasPrefix("Numen") })
        XCTAssertTrue(numen.persistent, "numina are persistent")
        XCTAssertEqual(numen.kind, .numen)

        let journal = try JournalRepository(db: db, playthroughID: playthrough.id).recent()
        XCTAssertTrue(journal.first?.entry.contains("Imported from AUTOSAVE.json") == true)
    }

    func testReimportFillsEmptyFieldsWithoutStompingUserData() throws {
        let (db, playthrough) = try makeMigratedDB()
        let books = BookRepository(db: db, playthroughID: playthrough.id)
        // A user-recorded book that must survive the import untouched where filled.
        var manual = try books.insert(BookDraft(title: "Test Book",
                                                mysteryPrincipleID: nil, difficulty: 8,
                                                readStatus: .uncatalogued))
        var created = manual
        created.notes = "my precious notes"
        try books.update(created)

        let report = try SaveImporter.run(
            saveURL: try makeSave(), db: db, playthroughID: playthrough.id,
            elementsDirectory: try makeElementsDirectory())

        XCTAssertEqual(report.booksCreated, 3, "three new tomes (the fourth title is pre-recorded)")
        XCTAssertEqual(report.booksUpdated, 1)
        let reloaded = try XCTUnwrap(try books.get(manual.id))
        XCTAssertEqual(reloaded.notes, "my precious notes", "user notes preserved")
        XCTAssertEqual(reloaded.difficulty, 8, "user-recorded difficulty not stomped")
        XCTAssertEqual(reloaded.readStatus, .mastered, "but read state upgrades")
        XCTAssertEqual(reloaded.location, "Library — shelf D.3",
                       "fill-empty: save location stamps an unrecorded one")
    }

    // Live failure reported by the user (2026-10-06): first playthrough already held the autosave's
    // names; importing into a new playthrough died on the table-global
    // `Skills.name UNIQUE` (001-era, not rebuilt when 005 scoped the tables).
    // Migration 006 rebuilds Skills/Memories so names are unique per playthrough.
    func testImportIntoSecondPlaythroughWithSameEntityNames() throws {
        let db = try SQLiteDatabase(path: ":memory:")
        try Migrator(migrations: Migrator.bundled()).apply(to: db)
        let first = try XCTUnwrap(PlaythroughRepository(db: db).active())
        let saveURL = try makeSave()
        let elements = try makeElementsDirectory()

        try SaveImporter.run(saveURL: saveURL, db: db, playthroughID: first.id,
                             elementsDirectory: elements)

        let second = try PlaythroughRepository(db: db).insert(name: "Playthrough 2")
        let report = try SaveImporter.run(saveURL: saveURL, db: db,
                                          playthroughID: second.id,
                                          elementsDirectory: elements)

        XCTAssertEqual(report.booksCreated, 4)
        XCTAssertEqual(report.skillsCreated, 2)
        XCTAssertEqual(report.memoriesCreated, 2)
        XCTAssertEqual(try SkillRepository(db: db, playthroughID: first.id).all().count, 2)
        XCTAssertEqual(try SkillRepository(db: db, playthroughID: second.id).all().count, 2)
    }

    // MARK: - Scanner

    func testSaveScannerFindsOnlyValidSaves() throws {
        let saveDir = workDirectory.appendingPathComponent("saves", isDirectory: true)
        try FileManager.default.createDirectory(at: saveDir, withIntermediateDirectories: true)
        try Data("""
        { "RootPopulationCommand": { "Spheres": [] },
          "Version": { "Version": "2026.1.f.3" } }
        """.utf8).write(to: saveDir.appendingPathComponent("AUTOSAVE.json"))
        try Data("{ \"not\": \"a save\" }".utf8)
            .write(to: saveDir.appendingPathComponent("achievements.json"))
        // A valid-save-shaped stray file, grown to over the scanner's cap via
        // truncation (sparse — cheap to create, logical size is what counts).
        let huge = try FileHandle(forWritingTo: {
            let url = saveDir.appendingPathComponent("junk-64GB.json")
            try Data("{ \"RootPopulationCommand\": { \"Spheres\": [] } }".utf8).write(to: url)
            return url
        }())
        try huge.truncate(atOffset: 64 * 1024 * 1024 + 1)
        try huge.close()

        let oldEnv = ProcessInfo.processInfo.environment["BOH_SAVE_DIR"]
        setenv("BOH_SAVE_DIR", saveDir.path, 1)
        defer { restoreEnv("BOH_SAVE_DIR", oldEnv) }

        XCTAssertEqual(SaveScanner.availableSaves().map(\.fileName), ["AUTOSAVE.json"],
                       "achievements and oversized files excluded")
        XCTAssertEqual(SaveScanner.availableSaves().first?.gameVersion, "2026.1.f.3")
        XCTAssertTrue(SaveScanner.availableSaves(maxFileBytes: 10).isEmpty,
                      "size cap skips slurping huge files")
    }

    // MARK: - Real game (skips when not installed)

    func testImportFromRealInstalledGameAndSave() throws {
        guard let elementsDirectory = BoHPaths.gameElementsDirectory(),
              FileManager.default.fileExists(atPath: elementsDirectory.path),
              let save = SaveScanner.availableSaves().first else {
            throw XCTSkip("Book of Hours (and/or a save) not installed at the standard path")
        }
        let (db, playthrough) = try makeMigratedDB()
        var report: ImportReport!
        try db.transaction {
            report = try SaveImporter.run(saveURL: save.url, db: db,
                                          playthroughID: playthrough.id,
                                          elementsDirectory: elementsDirectory)
        }
        XCTAssertGreaterThan(report.booksCreated, 0, "live save should import books")
        let books = try BookRepository(db: db, playthroughID: playthrough.id).all()
        let offenders = books.filter { $0.title.isEmpty || $0.title.hasPrefix("t.") }
        XCTAssertTrue(offenders.isEmpty,
                      "titles should be human labels, not element ids — offenders: \(offenders.map(\.title))")
        XCTAssertGreaterThan(report.memoriesCreated, 0, "mastered books yield memories")
        XCTAssertTrue(books.contains { $0.bookKind == .book },
                      "codex tomes import as books, not records — kind mapping regression")
    }

    private func restoreEnv(_ key: String, _ value: String?) {
        if let value { setenv(key, value, 1) } else { unsetenv(key) }
    }
}