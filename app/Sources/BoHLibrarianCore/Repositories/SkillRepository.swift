import Foundation

public final class SkillRepository {
    private let db: SQLiteDatabase
    private let playthroughID: Int64

    public init(db: SQLiteDatabase, playthroughID: Int64) {
        self.db = db
        self.playthroughID = playthroughID
    }

    // MARK: CRUD

    public func all() throws -> [Skill] {
        try db.query("SELECT * FROM Skills WHERE playthrough_id = ? ORDER BY name;",
                     [playthroughID], map: Self.map)
    }

    public func get(_ id: Int64) throws -> Skill? {
        try db.query("SELECT * FROM Skills WHERE playthrough_id = ? AND id = ?;",
                     [playthroughID, id], map: Self.map).first
    }

    @discardableResult
    public func insert(_ draft: SkillDraft) throws -> Skill {
        try db.execute(
            """
            INSERT INTO Skills (name, is_language, primary_principle_id,
                                secondary_principle_id, level, wisdom, element, notes,
                                playthrough_id)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
            """,
            [draft.name, draft.isLanguage, draft.primaryPrincipleID, draft.secondaryPrincipleID,
             draft.level, draft.wisdom, draft.element, draft.notes, playthroughID]
        )
        return try get(db.lastInsertRowID)!
    }

    public func update(_ skill: Skill) throws {
        try db.execute(
            """
            UPDATE Skills
            SET name = ?, is_language = ?, primary_principle_id = ?, secondary_principle_id = ?,
                level = ?, wisdom = ?, element = ?, notes = ?, updated_at = datetime('now')
            WHERE id = ?;
            """,
            [skill.name, skill.isLanguage, skill.primaryPrincipleID, skill.secondaryPrincipleID,
             skill.level, skill.wisdom, skill.element, skill.notes, skill.id]
        )
    }

    public func delete(_ id: Int64) throws {
        try db.execute("DELETE FROM Skills WHERE playthrough_id = ? AND id = ?;",
                       [playthroughID, id])
    }

    // MARK: Reading Helper (canonical query 2 — docs/DATABASE.md)

    /// Non-language skills with the principle, and how much of it they contribute.
    /// A level-L skill contributes L+1 to its primary principle, L to its secondary.
    public func contributions(principleID: Int64) throws -> [SkillContribution] {
        try db.query(
            """
            SELECT *,
                   CASE WHEN primary_principle_id = ? THEN level + 1 ELSE level END AS contributes
            FROM Skills
            WHERE playthrough_id = ? AND is_language = 0 AND level IS NOT NULL
              AND (primary_principle_id = ? OR secondary_principle_id = ?)
            ORDER BY contributes DESC, name;
            """,
            [principleID, playthroughID, principleID, principleID]
        ) { row in
            var skill = try Self.map(row)
            // SELECT * plus the computed column: keep the real level.
            skill.level = row.int("level")
            return SkillContribution(skill: skill, contributes: try row.requireInt("contributes"))
        }
    }

    static func map(_ row: Row) throws -> Skill {
        Skill(id: try row.requireInt64("id"),
              name: try row.requireString("name"),
              isLanguage: row.bool("is_language"),
              primaryPrincipleID: row.int64("primary_principle_id"),
              secondaryPrincipleID: row.int64("secondary_principle_id"),
              level: row.int("level"),
              wisdom: row.string("wisdom"),
              element: row.string("element"),
              notes: row.string("notes"))
    }
}