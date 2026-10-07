import Foundation

public final class MemoryRepository {
    private let db: SQLiteDatabase
    private let playthroughID: Int64

    public init(db: SQLiteDatabase, playthroughID: Int64) {
        self.db = db
        self.playthroughID = playthroughID
    }

    // MARK: CRUD

    public func all() throws -> [Memory] {
        let memories = try db.query(
            "SELECT * FROM Memories WHERE playthrough_id = ? ORDER BY name;",
            [playthroughID],
            map: Self.map
        )
        return try attachAspects(to: memories)
    }

    /// The memories the player can actually know. A memory is *earned* when it has
    /// no yield links (hand-created) or at least one yielding book is mastered;
    /// a memory whose yielding books are all unmastered stays in the table (import
    /// and lookup paths depend on it) but not in the list — knowing its name/traits
    /// before earning it would spoil the playthrough. Matches the mastered-only
    /// backlinks display on the detail side (docs/DATABASE.md §Books).
    public func allKnown() throws -> [Memory] {
        let memories = try db.query(
            """
            SELECT * FROM Memories m
            WHERE m.playthrough_id = ? AND \(Self.earnedVisibility)
            ORDER BY name;
            """,
            [playthroughID],
            map: Self.map
        )
        return try attachAspects(to: memories)
    }

    /// Earned-visibility SQL over alias `m` (also drives the Reading Helper's
    /// aspect candidates — docs/DATABASE.md §Memories).
    static let earnedVisibility = """
        ( NOT EXISTS (
            SELECT 1 FROM Books b
            WHERE b.playthrough_id = m.playthrough_id
              AND b.yielded_memory_id = m.id )
          OR EXISTS (
            SELECT 1 FROM Books b
            WHERE b.playthrough_id = m.playthrough_id
              AND b.yielded_memory_id = m.id
              AND b.read_status = 'mastered' ) )
        """

    public func get(_ id: Int64) throws -> Memory? {
        guard var memory = try db.query(
            "SELECT * FROM Memories WHERE playthrough_id = ? AND id = ?;",
            [playthroughID, id], map: Self.map
        ).first else { return nil }
        memory.aspects = try aspects(for: id)
        return memory
    }

    @discardableResult
    public func insert(_ draft: MemoryDraft) throws -> Memory {
        let id = try insertRow(draft)
        try setAspects(id, draft.aspects)
        return try get(id)!
    }

    /// Insert `draft`, unless a memory of the same (name, kind) already exists in
    /// this playthrough — game entities are unique per playthrough (006), and the
    /// save import seeds hidden (unearned) memories that a later record-read will
    /// legitimately re-create by hand: reusing it links the existing row rather
    /// than failing the whole read. Returns the memory with its aspects.
    public func insertOrReuse(_ draft: MemoryDraft) throws -> Memory {
        if let existing = try db.query(
            """
            SELECT id FROM Memories
            WHERE playthrough_id = ? AND kind = ? AND LOWER(name) = LOWER(?)
            LIMIT 1;
            """,
            [playthroughID, draft.kind.rawValue, draft.name],
            map: { try $0.requireInt64("id") }
        ).first {
            return try get(existing)!
        }
        return try insert(draft)
    }
    public func update(_ memory: Memory) throws {
        try db.transaction {
            try db.execute(
                """
                UPDATE Memories
                SET name = ?, kind = ?, persistent = ?, notes = ?, updated_at = datetime('now')
                WHERE id = ?;
                """,
                [memory.name, memory.kind.rawValue, memory.persistent, memory.notes, memory.id]
            )
            try setAspects(memory.id, memory.aspects.map { AspectDraft(principleID: $0.principleID, level: $0.level) })
        }
    }

    public func delete(_ id: Int64) throws {
        try db.execute("DELETE FROM Memories WHERE playthrough_id = ? AND id = ?;",
                       [playthroughID, id])
    }

    private func insertRow(_ draft: MemoryDraft) throws -> Int64 {
        try db.execute(
            """
            INSERT INTO Memories (name, kind, persistent, notes, playthrough_id)
            VALUES (?, ?, ?, ?, ?);
            """,
            [draft.name, draft.kind.rawValue, draft.persistent, draft.notes, playthroughID]
        )
        return db.lastInsertRowID
    }

    // MARK: Aspects

    public func aspects(for memoryID: Int64) throws -> [Aspect] {
        try db.query(
            """
            SELECT ma.principle_id, p.name AS principle_name, ma.level
            FROM MemoryAspects ma
            JOIN Principles p ON p.id = ma.principle_id
            WHERE ma.memory_id = ?
            ORDER BY p.sort_order;
            """,
            [memoryID],
            map: Self.mapAspect
        )
    }

    /// Replace the memory's aspects entirely.
    public func setAspects(_ memoryID: Int64, _ aspects: [AspectDraft]) throws {
        try db.transaction {
            try db.execute("DELETE FROM MemoryAspects WHERE memory_id = ?;", [memoryID])
            for aspect in aspects {
                try db.execute(
                    "INSERT INTO MemoryAspects (memory_id, principle_id, level) VALUES (?, ?, ?);",
                    [memoryID, aspect.principleID, aspect.level]
                )
            }
        }
    }

    // MARK: Sources

    public func sources(for memoryID: Int64) throws -> [MemorySource] {
        try db.query(
            "SELECT kind, detail FROM MemorySources WHERE memory_id = ? ORDER BY kind, detail;",
            [memoryID]
        ) {
            MemorySource(kind: try $0.requireString("kind"), detail: $0.string("detail"))
        }
    }

    public func addSource(_ memoryID: Int64, kind: String, detail: String? = nil) throws {
        try db.execute(
            "INSERT OR IGNORE INTO MemorySources (memory_id, kind, detail) VALUES (?, ?, ?);",
            [memoryID, kind, detail]
        )
    }

    /// Replace the memory's "how to obtain" rows wholesale (form save).
    public func setSources(_ memoryID: Int64, _ sources: [MemorySource]) throws {
        try db.transaction {
            try db.execute("DELETE FROM MemorySources WHERE memory_id = ?;", [memoryID])
            for source in sources {
                try db.execute(
                    "INSERT OR IGNORE INTO MemorySources (memory_id, kind, detail) VALUES (?, ?, ?);",
                    [memoryID, source.kind, source.detail]
                )
            }
        }
    }

    /// All "how to obtain" rows grouped per memory in one query (scoped to this
    /// playthrough) — the UI caches live from this after every reload rather than
    /// snapshots that go stale until reselect (user-reported bug, 2026-10-06).
    public func allSourcesByMemory() throws -> [Int64: [MemorySource]] {
        let rows = try db.query(
            """
            SELECT ms.memory_id, ms.kind, ms.detail
            FROM MemorySources ms
            JOIN Memories m ON m.id = ms.memory_id
            WHERE m.playthrough_id = ?
            ORDER BY ms.kind, ms.detail;
            """,
            [playthroughID],
            map: { (try $0.requireInt64("memory_id"),
                    try $0.requireString("kind"),
                    $0.string("detail")) })
        var grouped: [Int64: [MemorySource]] = [:]
        for (memoryID, kind, detail) in rows {
            grouped[memoryID, default: []].append(MemorySource(kind: kind, detail: detail))
        }
        return grouped
    }

    /// Mastered-only yielding links grouped per memory (display semantics —
    /// matching `booksYielding`) in one query.
    public func yieldingByMemory() throws -> [Int64: [BookRef]] {
        let rows = try db.query(
            """
            SELECT id, title, yielded_memory_id FROM Books
            WHERE playthrough_id = ? AND yielded_memory_id IS NOT NULL
              AND read_status = 'mastered'
            ORDER BY title;
            """,
            [playthroughID],
            map: { (try $0.requireInt64("id"),
                    try $0.requireString("title"),
                    try $0.requireInt64("yielded_memory_id")) })
        var grouped: [Int64: [BookRef]] = [:]
        for (id, title, memoryID) in rows {
            grouped[memoryID, default: []].append(BookRef(id: id, title: title))
        }
        return grouped
    }

    /// Every book currently linked as yielding this memory, any read status —
    /// for editing; the mastered-only `booksYielding` variant is for display.
    public func allYielding(_ memoryID: Int64) throws -> [BookRef] {
        try db.query(
            """
            SELECT id, title FROM Books
            WHERE playthrough_id = ? AND yielded_memory_id = ?
            ORDER BY title;
            """,
            [playthroughID, memoryID]
        ) {
            BookRef(id: try $0.requireInt64("id"), title: try $0.requireString("title"))
        }
    }

    /// Sync the yielding links to `bookIDs` (form save): books linked but not
    /// listed get unlinked, listed books get the link (a book yields one memory).
    /// Same-playthrough only; ids are bound, never interpolated.
    public func setYieldingBooks(_ memoryID: Int64, _ bookIDs: [Int64]) throws {
        try db.transaction {
            if bookIDs.isEmpty {
                try db.execute(
                    """
                    UPDATE Books SET yielded_memory_id = NULL, updated_at = datetime('now')
                    WHERE playthrough_id = ? AND yielded_memory_id = ?;
                    """, [playthroughID, memoryID])
            } else {
                let placeholders = Array(repeating: "?", count: bookIDs.count).joined(separator: ", ")
                try db.execute(
                    """
                    UPDATE Books SET yielded_memory_id = NULL, updated_at = datetime('now')
                    WHERE playthrough_id = ? AND yielded_memory_id = ?
                      AND id NOT IN (\(placeholders));
                    """, [playthroughID, memoryID] + bookIDs)
            }
            for id in bookIDs {
                try db.execute(
                    """
                    UPDATE Books SET yielded_memory_id = ?, updated_at = datetime('now')
                    WHERE playthrough_id = ? AND id = ?;
                    """, [memoryID, playthroughID, id])
            }
        }
    }

    public func removeSource(_ memoryID: Int64, kind: String, detail: String? = nil) throws {
        try db.execute(
            "DELETE FROM MemorySources WHERE memory_id = ? AND kind = ? AND detail IS ?;",
            [memoryID, kind, detail]
        )
    }

    // MARK: Reading Helper (canonical query 1 — docs/DATABASE.md)

    /// Memories whose level in `principleID` is at least `minLevel`, best first.
    public func candidates(principleID: Int64, minLevel: Int) throws -> [MemoryCandidate] {
        try db.query(
            """
            SELECT m.id, m.name, m.kind, m.persistent, ma.level
            FROM Memories m
            JOIN MemoryAspects ma ON ma.memory_id = m.id
            WHERE m.playthrough_id = ? AND ma.principle_id = ? AND ma.level >= ?
              AND \(Self.earnedVisibility)
            ORDER BY ma.level DESC, m.name;
            """,
            [playthroughID, principleID, minLevel]
        ) {
            MemoryCandidate(id: try $0.requireInt64("id"),
                            name: try $0.requireString("name"),
                            kind: MemoryKind(rawValue: try $0.requireString("kind")) ?? .memory,
                            persistent: $0.bool("persistent"),
                            level: try $0.requireInt("level"))
        }
    }

    /// Books that yield this memory when read — displayed backlinks only cover
    /// books the player has mastered. A recorded-but-unread book still carries its
    /// yield (import/record-read data, links survive), but until the player has
    /// actually mastered it, naming its yield is a spoiler (docs/DATABASE.md,
    /// D6's spirit: the UI shows what the player knows, from their own play).
    public func booksYielding(_ memoryID: Int64) throws -> [BookRef] {
        try db.query(
            """
            SELECT id, title FROM Books
            WHERE playthrough_id = ? AND yielded_memory_id = ?
              AND read_status = 'mastered'
            ORDER BY title;
            """,
            [playthroughID, memoryID]
        ) {
            BookRef(id: try $0.requireInt64("id"), title: try $0.requireString("title"))
        }
    }

    // MARK: Mapping helpers

    private func attachAspects(to memories: [Memory]) throws -> [Memory] {
        guard !memories.isEmpty else { return memories }
        var byMemory: [Int64: [Aspect]] = [:]
        let rows = try db.query(
            """
            SELECT ma.memory_id, ma.principle_id, p.name AS principle_name, ma.level
            FROM MemoryAspects ma
            JOIN Principles p ON p.id = ma.principle_id
            ORDER BY p.sort_order;
            """
        ) { row in
            (memoryID: try row.requireInt64("memory_id"),
             aspect: try Self.mapAspect(row))
        }
        for row in rows {
            byMemory[row.memoryID, default: []].append(row.aspect)
        }
        return memories.map { memory in
            var updated = memory
            updated.aspects = byMemory[memory.id] ?? []
            return updated
        }
    }

    private static func mapAspect(_ row: Row) throws -> Aspect {
        Aspect(principleID: try row.requireInt64("principle_id"),
               principleName: try row.requireString("principle_name"),
               level: try row.requireInt("level"))
    }

    static func map(_ row: Row) throws -> Memory {
        Memory(id: try row.requireInt64("id"),
               name: try row.requireString("name"),
               kind: MemoryKind(rawValue: try row.requireString("kind")) ?? .memory,
               persistent: row.bool("persistent"),
               notes: row.string("notes"))
    }
}