import XCTest
@testable import BoHLibrarianCore

final class SQLiteDatabaseTests: XCTestCase {

    private var db: SQLiteDatabase!

    override func setUpWithError() throws {
        db = try SQLiteDatabase(path: ":memory:")
    }

    override func tearDown() {
        db = nil
    }

    // MARK: Connection

    func testForeignKeysAreEnabledOnEveryConnection() throws {
        XCTAssertEqual(try db.scalarInt("PRAGMA foreign_keys;"), 1)
    }

    func testOpenFailsForUnwritablePath() {
        XCTAssertThrowsError(try SQLiteDatabase(path: "/definitely/not/a/real/place/Boh.db")) { error in
            XCTAssertTrue("\(error)".contains("cannot open database"), "unexpected error: \(error)")
        }
    }

    // MARK: Execute / query / bind

    func testExecuteQueryRoundTripAllTypes() throws {
        try db.execute("CREATE TABLE t (a INTEGER, b TEXT, c REAL, d BLOB, e INTEGER);")
        try db.execute("INSERT INTO t VALUES (?, ?, ?, ?, ?);", [42, "hush", 2.5, Data([1, 2, 3]), true])

        let rows = try db.query("SELECT a, b, c, d, e FROM t;") { row in
            (row.int("a"), row.string("b"), row.double("c"), row.data("d"), row.bool("e"))
        }
        XCTAssertEqual(rows.count, 1)
        let row = rows[0]
        XCTAssertEqual(row.0, 42)
        XCTAssertEqual(row.1, "hush")
        XCTAssertEqual(row.2, 2.5)
        XCTAssertEqual(row.3, Data([1, 2, 3]))
        XCTAssertEqual(row.4, true)
    }

    func testNullsComeBackAsNil() throws {
        try db.execute("CREATE TABLE t (a INTEGER, b TEXT);")
        try db.execute("INSERT INTO t VALUES (?, ?);", [nil as Int?, nil as String?])

        let rows = try db.query("SELECT a, b FROM t;") { row in (row.int("a"), row.string("b")) }
        XCTAssertEqual(rows.count, 1)
        XCTAssertNil(rows[0].0)
        XCTAssertNil(rows[0].1)
    }

    func testBindParametersFilterRows() throws {
        try db.execute("CREATE TABLE t (name TEXT);")
        try db.execute("INSERT INTO t VALUES (?);", ["Lantern"])
        try db.execute("INSERT INTO t VALUES (?);", ["Nectar"])
        let rows = try db.query("SELECT name FROM t WHERE name = ?;", ["Nectar"]) { $0.string("name") }
        XCTAssertEqual(rows, ["Nectar"])
    }

    func testConstraintViolationThrows() throws {
        try db.execute("CREATE TABLE t (a TEXT NOT NULL);")
        XCTAssertThrowsError(try db.execute("INSERT INTO t VALUES (?);", [nil as String?])) { error in
            XCTAssertTrue("\(error)".contains("step failed"), "unexpected error: \(error)")
        }
    }

    func testRequireAccessorsThrowOnMissingColumn() throws {
        try db.execute("CREATE TABLE t (a TEXT);")
        try db.execute("INSERT INTO t VALUES ('x');")
        XCTAssertThrowsError(try db.query("SELECT a FROM t;") { try $0.requireInt64("typo") }) { error in
            XCTAssertTrue("\(error)".contains("typo"), "unexpected error: \(error)")
        }
    }

    // MARK: Scalars, scripts, transactions, metadata

    func testScalarIntReturnsZeroForEmptyResult() throws {
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM sqlite_master WHERE name = 'nope';"), 0)
    }

    func testLastInsertRowID() throws {
        try db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY AUTOINCREMENT, a TEXT);")
        try db.execute("INSERT INTO t (a) VALUES (?);", ["first"])
        XCTAssertEqual(db.lastInsertRowID, 1)
    }

    func testExecuteScriptRunsMultipleStatements() throws {
        try db.executeScript("""
            CREATE TABLE t (a TEXT);
            INSERT INTO t VALUES ('one');
            INSERT INTO t VALUES ('two');
            """)
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM t;"), 2)
    }

    func testTransactionCommits() throws {
        try db.execute("CREATE TABLE t (a TEXT);")
        try db.transaction {
            try db.execute("INSERT INTO t VALUES (?);", ["kept"])
        }
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM t;"), 1)
    }

    func testTransactionRollsBackOnError() throws {
        try db.execute("CREATE TABLE t (a TEXT);")
        struct Boom: Error {}
        XCTAssertThrowsError(try db.transaction {
            try db.execute("INSERT INTO t VALUES (?);", ["lost"])
            throw Boom()
        })
        XCTAssertEqual(try db.scalarInt("SELECT COUNT(*) FROM t;"), 0)
    }

    func testTableExists() throws {
        try db.execute("CREATE TABLE real_one (a TEXT);")
        XCTAssertTrue(try db.tableExists("real_one"))
        XCTAssertFalse(try db.tableExists("not_here"))
    }

    func testUserVersionRoundTrip() throws {
        XCTAssertEqual(db.userVersion, 0)
        try db.setUserVersion(7)
        XCTAssertEqual(db.userVersion, 7)
    }
}