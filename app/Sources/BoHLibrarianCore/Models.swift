import Foundation

// Domain model for BoH Librarian. Mirrors schema v2 (db/migrations/001_initial_schema.sql,
// docs/DATABASE.md). Game terms keep their exact in-game spelling.

// MARK: - Value sets (documented sets; the GUI constrains entry)

public enum MemoryKind: String, CaseIterable {
    case memory, weather, numen
}

public enum BookKind: String, CaseIterable {
    case book, scroll, film, record
}

public enum ReadStatus: String, CaseIterable {
    case uncatalogued, catalogued, mastered

    public var isRead: Bool { self == .mastered }
}

/// Contamination types ("none" is stored for a book whose contamination has been
/// confirmed as absent; nil means "unknown / not checked").
public enum Contamination: String, CaseIterable {
    case clear = "none"
    case curse, theoplasmic, infestation, corruption

    /// Title-case label for pickers and detail rows.
    public var displayName: String {
        switch self {
        case .clear: "None"
        case .curse: "Curse"
        case .theoplasmic: "Theoplasm"
        case .infestation: "Infestation"
        case .corruption: "Corruption"
        }
    }
}

/// How a memory can be obtained (docs/DATABASE.md §value sets).
public enum MemorySourceKind: String, CaseIterable {
    case reReadBook = "re-read book"
    case firstRead = "first read"
    case weather
    case talk
    case consider
    case consume
    case craft
    case gather
    case numa
    case other
}

// MARK: - Lookups

public struct Principle: Identifiable, Hashable {
    public var id: Int64
    public var name: String
    public var sortOrder: Int
    public var color: String?
    public var notes: String?

    public init(id: Int64, name: String, sortOrder: Int, color: String? = nil, notes: String? = nil) {
        self.id = id
        self.name = name
        self.sortOrder = sortOrder
        self.color = color
        self.notes = notes
    }
}

public struct Language: Identifiable, Hashable {
    public var id: Int64
    public var name: String
    public var native: Bool
    public var notes: String?

    public init(id: Int64, name: String, native: Bool, notes: String? = nil) {
        self.id = id
        self.name = name
        self.native = native
        self.notes = notes
    }
}

// MARK: - Memories

public struct Aspect: Identifiable, Hashable {
    public var principleID: Int64
    public var principleName: String
    public var level: Int

    public var id: Int64 { principleID }

    public init(principleID: Int64, principleName: String, level: Int) {
        self.principleID = principleID
        self.principleName = principleName
        self.level = level
    }
}

/// An aspect pending creation, before it has a principle name attached.
public struct AspectDraft: Hashable {
    public var principleID: Int64
    public var level: Int

    public init(principleID: Int64, level: Int) {
        self.principleID = principleID
        self.level = level
    }
}

public struct Memory: Identifiable, Hashable {
    public var id: Int64
    public var name: String
    public var kind: MemoryKind
    public var persistent: Bool
    public var notes: String?
    public var aspects: [Aspect]

    public init(id: Int64, name: String, kind: MemoryKind, persistent: Bool,
                notes: String? = nil, aspects: [Aspect] = []) {
        self.id = id
        self.name = name
        self.kind = kind
        self.persistent = persistent
        self.notes = notes
        self.aspects = aspects
    }
}

public struct MemoryDraft {
    public var name: String
    public var kind: MemoryKind
    public var persistent: Bool
    public var notes: String?
    public var aspects: [AspectDraft]

    public init(name: String, kind: MemoryKind, persistent: Bool,
                notes: String? = nil, aspects: [AspectDraft] = []) {
        self.name = name
        self.kind = kind
        self.persistent = persistent
        self.notes = notes
        self.aspects = aspects
    }
}

public struct MemorySource: Identifiable, Hashable {
    public var kind: String
    public var detail: String?

    public var id: String { "\(kind)|\(detail ?? "")" }

    public init(kind: String, detail: String? = nil) {
        self.kind = kind
        self.detail = detail
    }
}

/// A memory satisfying a Reading-Helper lookup (canonical query 1, docs/DATABASE.md).
public struct MemoryCandidate: Identifiable, Hashable {
    public var id: Int64
    public var name: String
    public var kind: MemoryKind
    public var persistent: Bool
    public var level: Int

    public init(id: Int64, name: String, kind: MemoryKind, persistent: Bool, level: Int) {
        self.id = id
        self.name = name
        self.kind = kind
        self.persistent = persistent
        self.level = level
    }
}

/// A book that yields a given memory (backlink shown on memory screens).
public struct BookRef: Identifiable, Hashable {
    public var id: Int64
    public var title: String

    public init(id: Int64, title: String) {
        self.id = id
        self.title = title
    }
}

// MARK: - Books

public struct Book: Identifiable, Hashable {
    public var id: Int64
    public var title: String
    public var setName: String?
    public var volume: String?
    public var bookKind: BookKind
    public var languageID: Int64?
    public var mysteryPrincipleID: Int64?
    public var mysteryLevel: Int?
    public var readStatus: ReadStatus
    public var contamination: Contamination?
    public var location: String?
    public var timesRead: Int
    public var firstReadAt: String?
    public var lastReadAt: String?
    public var lessons: Int?
    public var yieldedMemoryID: Int64?
    public var notes: String?

    public init(id: Int64, title: String, setName: String? = nil, volume: String? = nil,
                bookKind: BookKind = .book, languageID: Int64? = nil,
                mysteryPrincipleID: Int64? = nil, mysteryLevel: Int? = nil,
                readStatus: ReadStatus = .uncatalogued, contamination: Contamination? = nil,
                location: String? = nil, timesRead: Int = 0, firstReadAt: String? = nil,
                lastReadAt: String? = nil, lessons: Int? = nil, yieldedMemoryID: Int64? = nil,
                notes: String? = nil) {
        self.id = id
        self.title = title
        self.setName = setName
        self.volume = volume
        self.bookKind = bookKind
        self.languageID = languageID
        self.mysteryPrincipleID = mysteryPrincipleID
        self.mysteryLevel = mysteryLevel
        self.readStatus = readStatus
        self.contamination = contamination
        self.location = location
        self.timesRead = timesRead
        self.firstReadAt = firstReadAt
        self.lastReadAt = lastReadAt
        self.lessons = lessons
        self.yieldedMemoryID = yieldedMemoryID
        self.notes = notes
    }
}

public struct BookDraft {
    public var title: String
    public var setName: String?
    public var volume: String?
    public var bookKind: BookKind
    public var languageID: Int64?
    public var mysteryPrincipleID: Int64?
    public var mysteryLevel: Int?
    public var readStatus: ReadStatus
    public var contamination: Contamination?
    public var location: String?
    public var lessons: Int?
    public var yieldedMemoryID: Int64?
    public var notes: String?

    public init(title: String, setName: String? = nil, volume: String? = nil,
                bookKind: BookKind = .book, languageID: Int64? = nil,
                mysteryPrincipleID: Int64? = nil, mysteryLevel: Int? = nil,
                readStatus: ReadStatus = .uncatalogued, contamination: Contamination? = nil,
                location: String? = nil, lessons: Int? = nil, yieldedMemoryID: Int64? = nil,
                notes: String? = nil) {
        self.title = title
        self.setName = setName
        self.volume = volume
        self.bookKind = bookKind
        self.languageID = languageID
        self.mysteryPrincipleID = mysteryPrincipleID
        self.mysteryLevel = mysteryLevel
        self.readStatus = readStatus
        self.contamination = contamination
        self.location = location
        self.lessons = lessons
        self.yieldedMemoryID = yieldedMemoryID
        self.notes = notes
    }
}

/// Which skill's lessons a book teaches (junction `BookLessons`).
public struct BookLessonsEntry: Hashable {
    public var skillID: Int64
    public var amount: Int

    public init(skillID: Int64, amount: Int = 1) {
        self.skillID = skillID
        self.amount = amount
    }
}

// MARK: - Skills

public struct Skill: Identifiable, Hashable {
    public var id: Int64
    public var name: String
    public var isLanguage: Bool
    public var primaryPrincipleID: Int64?
    public var secondaryPrincipleID: Int64?
    public var level: Int?
    public var wisdom: String?
    public var element: String?
    public var notes: String?

    public init(id: Int64, name: String, isLanguage: Bool = false,
                primaryPrincipleID: Int64? = nil, secondaryPrincipleID: Int64? = nil,
                level: Int? = nil, wisdom: String? = nil, element: String? = nil,
                notes: String? = nil) {
        self.id = id
        self.name = name
        self.isLanguage = isLanguage
        self.primaryPrincipleID = primaryPrincipleID
        self.secondaryPrincipleID = secondaryPrincipleID
        self.level = level
        self.wisdom = wisdom
        self.element = element
        self.notes = notes
    }
}

public struct SkillDraft {
    public var name: String
    public var isLanguage: Bool
    public var primaryPrincipleID: Int64?
    public var secondaryPrincipleID: Int64?
    public var level: Int?
    public var wisdom: String?
    public var element: String?
    public var notes: String?

    public init(name: String, isLanguage: Bool = false,
                primaryPrincipleID: Int64? = nil, secondaryPrincipleID: Int64? = nil,
                level: Int? = nil, wisdom: String? = nil, element: String? = nil,
                notes: String? = nil) {
        self.name = name
        self.isLanguage = isLanguage
        self.primaryPrincipleID = primaryPrincipleID
        self.secondaryPrincipleID = secondaryPrincipleID
        self.level = level
        self.wisdom = wisdom
        self.element = element
        self.notes = notes
    }
}

/// A skill's computed contribution toward a principle (canonical query 2:
/// a level-L skill contributes L+1 primary / L secondary).
public struct SkillContribution: Identifiable, Hashable {
    public var skill: Skill
    public var contributes: Int

    public var id: Int64 { skill.id }

    public init(skill: Skill, contributes: Int) {
        self.skill = skill
        self.contributes = contributes
    }
}

// MARK: - Journal

public struct JournalEntry: Identifiable, Hashable {
    public var id: Int64
    public var loggedAt: String
    public var gameDay: String?
    public var entry: String
    public var bookID: Int64?
    public var memoryID: Int64?
    public var skillID: Int64?

    public init(id: Int64, loggedAt: String, gameDay: String? = nil, entry: String,
                bookID: Int64? = nil, memoryID: Int64? = nil, skillID: Int64? = nil) {
        self.id = id
        self.loggedAt = loggedAt
        self.gameDay = gameDay
        self.entry = entry
        self.bookID = bookID
        self.memoryID = memoryID
        self.skillID = skillID
    }
}

public struct JournalDraft {
    public var gameDay: String?
    public var entry: String
    public var bookID: Int64?
    public var memoryID: Int64?
    public var skillID: Int64?

    public init(gameDay: String? = nil, entry: String,
                bookID: Int64? = nil, memoryID: Int64? = nil, skillID: Int64? = nil) {
        self.gameDay = gameDay
        self.entry = entry
        self.bookID = bookID
        self.memoryID = memoryID
        self.skillID = skillID
    }
}