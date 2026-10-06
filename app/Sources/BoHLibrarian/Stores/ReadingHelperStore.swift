import Foundation
import Observation
import BoHLibrarianCore

/// UI state for the Reading Helper: a book picker plus the live "desk math" —
/// which recorded memories and skills could satisfy the selected book's
/// requirement (the canonical queries from docs/DATABASE.md).
@Observable
final class ReadingHelperStore {

    private let bookRepo: BookRepository
    private let memoryRepo: MemoryRepository
    private let skillRepo: SkillRepository

    private var principlesByID: [Int64: Principle] = [:]
    private var languagesByID: [Int64: Language] = [:]
    private var memoriesByID: [Int64: Memory] = [:]
    private var nativeLanguageNames: Set<String> = []
    private(set) var knownLanguageSkills: Set<String> = []

    private(set) var books: [Book] = []
    var searchText = ""
    var selectedBookID: Int64?

    init(db: SQLiteDatabase, playthroughID: Int64) {
        self.bookRepo = BookRepository(db: db, playthroughID: playthroughID)
        self.memoryRepo = MemoryRepository(db: db, playthroughID: playthroughID)
        self.skillRepo = SkillRepository(db: db, playthroughID: playthroughID)
        let principles = (try? PrincipleRepository(db: db).all()) ?? []
        let languages = (try? LanguageRepository(db: db).all()) ?? []
        principlesByID = Dictionary(uniqueKeysWithValues: principles.map { ($0.id, $0) })
        languagesByID = Dictionary(uniqueKeysWithValues: languages.map { ($0.id, $0) })
        nativeLanguageNames = Set(languages.filter(\.native).map(\.name))
        reload()
    }

    // MARK: Picker

    /// Search-filtered, unread first (this screen exists to choose the next read).
    var displayed: [Book] {
        var list = books
        let query = searchText.trimmingCharacters(in: .whitespacesAndNewlines)
        if !query.isEmpty {
            let needle = query.lowercased()
            list = list.filter { book in
                [book.title, book.setName, book.volume, book.location]
                    .compactMap { $0 }
                    .joined(separator: " ")
                    .lowercased()
                    .contains(needle)
            }
        }
        return list.sorted {
            let lhs = Self.statusRank($0.readStatus)
            let rhs = Self.statusRank($1.readStatus)
            return lhs == rhs
                ? $0.title.localizedStandardCompare($1.title) == .orderedAscending
                : lhs < rhs
        }
    }

    /// Resolved against all books, so the panel doesn't vanish mid-search.
    var selectedBook: Book? {
        books.first { $0.id == selectedBookID }
    }

    /// Select the first displayed book on arrival, so the panel is never empty.
    func ensureSelection() {
        if selectedBookID == nil {
            selectedBookID = displayed.first?.id
        }
    }

    static func statusRank(_ status: ReadStatus) -> Int {
        switch status {
        case .uncatalogued: return 0
        case .catalogued: return 1
        case .mastered: return 2
        }
    }

    func reload() {
        books = (try? bookRepo.all()) ?? []
        memoriesByID = Dictionary(uniqueKeysWithValues:
            ((try? memoryRepo.all()) ?? []).map { ($0.id, $0) })
        knownLanguageSkills = Set(
            ((try? skillRepo.all()) ?? []).filter(\.isLanguage).map(\.name))
    }

    // MARK: Lookups

    func principleName(_ id: Int64?) -> String? {
        id.flatMap { principlesByID[$0]?.name }
    }

    func principleColor(_ id: Int64?) -> String? {
        id.flatMap { principlesByID[$0]?.color }
    }

    func languageName(_ id: Int64?) -> String? {
        id.flatMap { languagesByID[$0]?.name }
    }

    /// nil = no language recorded; true/false = whether the Librarian can read it.
    func isLanguageKnown(_ languageID: Int64?) -> Bool? {
        guard let languageID, let name = languagesByID[languageID]?.name else { return nil }
        return nativeLanguageNames.contains(name) || knownLanguageSkills.contains(name)
    }

    func yieldedMemoryName(for book: Book) -> String? {
        book.yieldedMemoryID.flatMap { memoriesByID[$0]?.name }
    }

    // MARK: Desk math

    /// Memories ranked in the book's mystery principle, split into those that
    /// satisfy the difficulty (best first) and, when none do, the closest near-misses.
    func memoryCandidates(for book: Book)
        -> (satisfying: [MemoryCandidate], nearMisses: [MemoryCandidate]) {
        guard let principleID = book.mysteryPrincipleID, let difficulty = book.difficulty else {
            return ([], [])
        }
        let ranked = (try? memoryRepo.candidates(principleID: principleID, minLevel: 1)) ?? []
        let satisfying = ranked.filter { $0.level >= difficulty }
        let nearMisses = Array(ranked.filter { $0.level < difficulty }.prefix(3))
        return (satisfying, nearMisses)
    }

    /// Non-language skills contributing to the book's mystery principle, best first.
    func skillContributions(for book: Book) -> [SkillContribution] {
        guard let principleID = book.mysteryPrincipleID else { return [] }
        return (try? skillRepo.contributions(principleID: principleID)) ?? []
    }
}