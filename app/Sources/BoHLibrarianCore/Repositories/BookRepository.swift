import Foundation

public final class BookRepository {
    private let db: SQLiteDatabase

    public init(db: SQLiteDatabase) { self.db = db }

    // MARK: CRUD

    public func all() throws -> [Book] {
        try db.query("SELECT * FROM Books ORDER BY title;", map: Self.map)
    }

    public func get(_ id: Int64) throws -> Book? {
        try db.query("SELECT * FROM Books WHERE id = ?;", [id], map: Self.map).first
    }

    @discardableResult
    public func insert(_ draft: BookDraft) throws -> Book {
        try db.execute(
            """
            INSERT INTO Books (title, set_name, volume, book_kind, language_id,
                               mystery_principle_id, mystery_level, read_status,
                               contamination, location, lessons, yielded_memory_id, notes)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
            """,
            [draft.title, draft.setName, draft.volume, draft.bookKind.rawValue, draft.languageID,
             draft.mysteryPrincipleID, draft.mysteryLevel, draft.readStatus.rawValue,
             draft.contamination?.rawValue, draft.location, draft.lessons,
             draft.yieldedMemoryID, draft.notes]
        )
        return try get(db.lastInsertRowID)!
    }

    public func update(_ book: Book) throws {
        // Read counters (times_read, first/last_read_at) are managed by
        // recordRead()/updateReadStatus(), never by generic edits.
        try db.execute(
            """
            UPDATE Books
            SET title = ?, set_name = ?, volume = ?, book_kind = ?, language_id = ?,
                mystery_principle_id = ?, mystery_level = ?, read_status = ?,
                contamination = ?, location = ?, lessons = ?, yielded_memory_id = ?,
                notes = ?, updated_at = datetime('now')
            WHERE id = ?;
            """,
            [book.title, book.setName, book.volume, book.bookKind.rawValue, book.languageID,
             book.mysteryPrincipleID, book.mysteryLevel, book.readStatus.rawValue,
             book.contamination?.rawValue, book.location, book.lessons,
             book.yieldedMemoryID, book.notes, book.id]
        )
    }

    public func delete(_ id: Int64) throws {
        try db.execute("DELETE FROM Books WHERE id = ?;", [id])
    }

    // MARK: Reading state

    public func updateReadStatus(_ id: Int64, _ status: ReadStatus) throws {
        try db.execute(
            "UPDATE Books SET read_status = ?, updated_at = datetime('now') WHERE id = ?;",
            [status.rawValue, id]
        )
    }

    /// Log a read: increments the counter, stamps first/last read. Call
    /// `updateReadStatus(_: .mastered)` alongside it when the book was mastered.
    public func recordRead(_ id: Int64) throws {
        try db.execute(
            """
            UPDATE Books
            SET times_read = times_read + 1,
                first_read_at = COALESCE(first_read_at, datetime('now')),
                last_read_at = datetime('now'),
                updated_at = datetime('now')
            WHERE id = ?;
            """,
            [id]
        )
    }

    /// Point the book at the memory it always yields (record-read flow).
    public func setYieldedMemory(_ id: Int64, memoryID: Int64?) throws {
        try db.execute(
            "UPDATE Books SET yielded_memory_id = ?, updated_at = datetime('now') WHERE id = ?;",
            [memoryID, id]
        )
    }

    /// Record how many Lessons the book granted on its mastering read.
    public func setLessonsCount(_ id: Int64, lessons: Int?) throws {
        try db.execute(
            "UPDATE Books SET lessons = ?, updated_at = datetime('now') WHERE id = ?;",
            [lessons, id]
        )
    }

    // MARK: Lessons junction

    public func lessons(forBook bookID: Int64) throws -> [BookLessonsEntry] {
        try db.query(
            "SELECT skill_id, amount FROM BookLessons WHERE book_id = ?;",
            [bookID]
        ) {
            BookLessonsEntry(skillID: try $0.requireInt64("skill_id"),
                            amount: try $0.requireInt("amount"))
        }
    }

    public func setLessons(_ bookID: Int64, _ entries: [BookLessonsEntry]) throws {
        try db.transaction {
            try db.execute("DELETE FROM BookLessons WHERE book_id = ?;", [bookID])
            for entry in entries {
                try db.execute(
                    "INSERT INTO BookLessons (book_id, skill_id, amount) VALUES (?, ?, ?);",
                    [bookID, entry.skillID, entry.amount]
                )
            }
        }
    }

    // MARK: Mapping

    static func map(_ row: Row) throws -> Book {
        Book(id: try row.requireInt64("id"),
             title: try row.requireString("title"),
             setName: row.string("set_name"),
             volume: row.string("volume"),
             bookKind: BookKind(rawValue: try row.requireString("book_kind")) ?? .book,
             languageID: row.int64("language_id"),
             mysteryPrincipleID: row.int64("mystery_principle_id"),
             mysteryLevel: row.int("mystery_level"),
             readStatus: ReadStatus(rawValue: try row.requireString("read_status")) ?? .uncatalogued,
             contamination: row.string("contamination").flatMap(Contamination.init),
             location: row.string("location"),
             timesRead: try row.requireInt("times_read"),
             firstReadAt: row.string("first_read_at"),
             lastReadAt: row.string("last_read_at"),
             lessons: row.int("lessons"),
             yieldedMemoryID: row.int64("yielded_memory_id"),
             notes: row.string("notes"))
    }
}