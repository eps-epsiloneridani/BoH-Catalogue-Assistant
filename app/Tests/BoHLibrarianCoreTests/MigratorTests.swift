import XCTest
@testable import BoHLibrarianCore

final class MigratorTests: XCTestCase {

    /// The repo's db/migrations/ directory, located relative to this file so the
    /// test works no matter where `swift test` runs. This file lives at
    /// <repo>/app/Tests/BoHLibrarianCoreTests/MigratorTests.swift.
    private static let repoMigrations = URL(fileURLWithPath: #filePath)
        .deletingLastPathComponent()   // BoHLibrarianCoreTests
        .deletingLastPathComponent()  // Tests
        .deletingLastPathComponent()  // app
        .deletingLastPathComponent()  // repo root
        .appendingPathComponent("db").appendingPathComponent("migrations")

    private func freshDB() throws -> SQLiteDatabase {
        try SQLiteDatabase(path: ":memory:")
    }

    // MARK: Fresh application

    func testFreshApplyReachesVersion3AndSeeds() throws {
        let db = try freshDB()
        let migrator = try Migrator(migrations: Migrator.bundled())
        XCTAssertTrue(migrator.pending(on: db).count == 3)
        try migrator.apply(to: db)

        XCTAssertEqual(db.userVersion, 3)
        for table in ["Principles", "Languages", "Memories", "MemoryAspects", "MemorySources",
                      "Books", "BookLessons", "Skills", "Journal"] {
            XCTAssertTrue(try db.tableExists(table), "\(table) missing after migration")
        }
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM Principles;"), 13)
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM Languages;"), 15)
    }

    func testApplyIsIdempotent() throws {
        let db = try freshDB()
        let migrator = try Migrator(migrations: Migrator.bundled())
        try migrator.apply(to: db)
        try migrator.apply(to: db)   // second run: nothing pending
        XCTAssertTrue(migrator.pending(on: db).isEmpty)
        XCTAssertEqual(db.userVersion, 3)
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM Principles;"), 13, "seeds must not duplicate")
    }

    // MARK: Validation

    func testEmptyMigrationListThrows() {
        XCTAssertThrowsError(try Migrator(migrations: [])) { error in
            XCTAssertTrue("\(error)".contains("no migrations"), "unexpected error: \(error)")
        }
    }

    func testSequenceGapThrows() {
        let one = Migration(number: 1, name: "one", sql: "PRAGMA user_version = 1;")
        let three = Migration(number: 3, name: "three", sql: "PRAGMA user_version = 3;")
        XCTAssertThrowsError(try Migrator(migrations: [three, one])) { error in
            XCTAssertTrue("\(error)".contains("expected #2"), "unexpected error: \(error)")
        }
    }

    func testBadSQLSurfacesAsApplyFailure() throws {
        let migrator = try Migrator(migrations: [
            Migration(number: 1, name: "broken", sql: "THIS IS NOT SQL;")
        ])
        let db = try freshDB()
        XCTAssertThrowsError(try migrator.apply(to: db)) { error in
            XCTAssertTrue("\(error)".contains("migration #1 'broken' failed"), "unexpected error: \(error)")
        }
        XCTAssertEqual(db.userVersion, 0, "version must not advance on failure")
    }

    // MARK: Legacy guard (docs/DECISIONS.md D8)

    func testLegacyDataRefusesToApply() throws {
        let db = try freshDB()
        try db.executeScript("""
            CREATE TABLE Books (Title TEXT);
            CREATE TABLE Memories (Name TEXT);
            INSERT INTO Books VALUES ('precious find');
            """)
        let migrator = try Migrator(migrations: Migrator.bundled())
        XCTAssertThrowsError(try migrator.apply(to: db)) { error in
            XCTAssertTrue("\(error)".contains("legacy"), "unexpected error: \(error)")
        }
        XCTAssertEqual(db.userVersion, 0, "nothing applied")
        XCTAssertTrue(try db.tableExists("Books"), "legacy data left untouched")
    }

    func testEmptyLegacyTablesAreSafeToDrop() throws {
        let db = try freshDB()
        try db.executeScript("CREATE TABLE Books (Title TEXT); CREATE TABLE Memories (Name TEXT);")
        let migrator = try Migrator(migrations: Migrator.bundled())
        try migrator.apply(to: db)
        XCTAssertEqual(db.userVersion, 3)
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM Books;"), 0, "new Books table, empty")
    }

    // MARK: Bundled copy stays in sync with the repo

    func testBundledMigrationsMatchRepoDirectory() throws {
        guard FileManager.default.fileExists(atPath: Self.repoMigrations.path) else {
            throw XCTSkip("repo db/migrations/ not present (detached resources-only run)")
        }
        let repo = try Migrator(migrations: Migrator.load(fromDirectory: Self.repoMigrations))
        let bundled = try Migrator(migrations: Migrator.bundled())
        XCTAssertEqual(repo.migrations, bundled.migrations,
                       "bundled migrations drifted from db/migrations/ — run scripts/sync-migrations.sh")
    }

    func testResolveUsesEnvVariableWhenSet() throws {
        let old = ProcessInfo.processInfo.environment["BOH_MIGRATIONS"]
        setenv("BOH_MIGRATIONS", Self.repoMigrations.path, 1)
        defer { restoreEnv("BOH_MIGRATIONS", old) }

        let migrator = try Migrator.resolve()
        XCTAssertEqual(migrator.migrations.count, 3)
    }

    private func restoreEnv(_ key: String, _ value: String?) {
        if let value { setenv(key, value, 1) } else { unsetenv(key) }
    }
}