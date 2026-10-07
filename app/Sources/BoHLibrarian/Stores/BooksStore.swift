import Foundation
import Observation
import BoHLibrarianCore

/// UI state for the Books screen. Owns the book list, query options and selection;
/// wraps repository calls, reloading after each mutation (docs/GUI_PLAN.md §Data flow).
@Observable
final class BooksStore {

    private let db: SQLiteDatabase
    private let playthroughID: Int64
    private let repo: BookRepository
    private let journalRepo: JournalRepository
    private let memoryRepo: MemoryRepository

    // Lookups for list rows, search and detail hints.
    private var principlesByID: [Int64: Principle] = [:]
    private var languagesByID: [Int64: Language] = [:]
    private var memoriesByID: [Int64: Memory] = [:]
    private var skillNamesByID: [Int64: String] = [:]

    /// Live per-record auxiliary caches, refilled on every reload — the detail
    /// pane reads these live (snapshot caches went stale after quick-note adds).
    private(set) var journalByBook: [Int64: [JournalEntry]] = [:]
    private(set) var lessonNamesByBook: [Int64: [String]] = [:]
    private var nativeLanguageNames: Set<String> = []
    private(set) var knownLanguageSkills: Set<String> = []

    private(set) var books: [Book] = []
    var lastError: String?

    var options = BookQueryOptions()
    var selectedBookID: Int64?

    init(db: SQLiteDatabase, playthroughID: Int64) {
        self.db = db
        self.playthroughID = playthroughID
        self.repo = BookRepository(db: db, playthroughID: playthroughID)
        self.journalRepo = JournalRepository(db: db, playthroughID: playthroughID)
        self.memoryRepo = MemoryRepository(db: db, playthroughID: playthroughID)
        let principles = (try? PrincipleRepository(db: db).all()) ?? []
        let languages = (try? LanguageRepository(db: db).all()) ?? []
        principlesByID = Dictionary(uniqueKeysWithValues: principles.map { ($0.id, $0) })
        languagesByID = Dictionary(uniqueKeysWithValues: languages.map { ($0.id, $0) })
        nativeLanguageNames = Set(languages.filter(\.native).map(\.name))
        reload()
    }

    // MARK: Derived

    var displayed: [Book] {
        BookFiltering.apply(books, options: options,
                            principleNames: principlesByID.mapValues(\.name),
                            languageNames: languagesByID.mapValues(\.name))
    }

    var selectedBook: Book? {
        displayed.first { $0.id == selectedBookID }
    }

    // MARK: Lookups for views

    func principleName(_ id: Int64?) -> String? { id.flatMap { principlesByID[$0]?.name } }
    func principleColor(_ id: Int64?) -> String? { id.flatMap { principlesByID[$0]?.color } }
    func languageName(_ id: Int64?) -> String? { id.flatMap { languagesByID[$0]?.name } }
    func memoryName(_ id: Int64?) -> String? { id.flatMap { memoriesByID[$0]?.name } }

    /// nil = no language recorded; true/false = whether the Librarian can read it.
    func isLanguageKnown(_ languageID: Int64?) -> Bool? {
        guard let languageID, let name = languagesByID[languageID]?.name else { return nil }
        return nativeLanguageNames.contains(name) || knownLanguageSkills.contains(name)
    }

    /// Live (cache refilled on reload): skill names + "×N", sorted.
    func lessonSkillNames(for book: Book) -> [String] {
        lessonNamesByBook[book.id] ?? []
    }

    /// Live (cache refilled on reload): entries linked to the book, newest first.
    func journalEntries(for book: Book) -> [JournalEntry] {
        journalByBook[book.id] ?? []
    }

    var allMemories: [Memory] {
        (try? memoryRepo.all()) ?? []
    }

    /// Earned memories only — "Memory used" can't offer what isn't possessed.
    var earnedMemories: [Memory] {
        (try? memoryRepo.allKnown()) ?? []
    }

    // MARK: Mutations

    func reload() {
        do {
            books = try repo.all()
            memoriesByID = Dictionary(uniqueKeysWithValues: try memoryRepo.all().map { ($0.id, $0) })
            let skills = try SkillRepository(db: db, playthroughID: playthroughID).all()
            skillNamesByID = Dictionary(uniqueKeysWithValues: skills.map { ($0.id, $0.name) })
            knownLanguageSkills = Set(skills.filter(\.isLanguage).map(\.name))
            journalByBook = try journalRepo.entriesByBook()
            let lessonAmounts = try repo.lessonSkillAmountsByBook()
            lessonNamesByBook = lessonAmounts.mapValues { rows in
                rows.compactMap { entry -> String? in
                    entry.amount > 1 ? "\(entry.skillName) ×\(entry.amount)" : entry.skillName
                }.sorted()
            }
        } catch {
            lastError = "\(error)"
        }
    }

    /// Runs one mutation, reloads the list on success, records the error otherwise.
    func perform(_ label: String, _ operation: () throws -> Void) {
        do {
            try operation()
            reload()
        } catch {
            lastError = "\(label) failed: \(error)"
        }
    }

    /// A form master is an actual read: log counters/stamps and drop the
    /// mastered journal entry. The yield memory stays whatever is linked (an
    /// imported link is earned by this very read); memory/lessons capture
    /// remains the record-read sheet's job (skills/lessons in-form parked).
    private func logFormMasteredRead(_ book: Book, gameDay: String?) throws {
        try repo.recordRead(book.id)
        _ = try journalRepo.insert(JournalDraft(
            gameDay: gameDay,
            entry: ReadingLog.journalText(book: book, mastering: true,
                                          usedMemoryName: nil, gainedMemoryName: nil,
                                          lessons: nil, userNote: nil),
            bookID: book.id))
    }

    func add(_ draft: BookDraft, gameDay: String? = nil) {
        perform("Adding book") {
            var newID: Int64?
            try db.transaction {
                let book = try repo.insert(draft)
                newID = book.id
                if BookReadTransitions.countsAsRead(original: nil, draft: draft) {
                    try logFormMasteredRead(book, gameDay: gameDay)
                }
            }
            selectedBookID = newID
        }
    }

    func update(_ book: Book) {
        perform("Saving book") { try repo.update(book) }
    }

    /// Apply the edit form's draft onto an existing book, keeping read counters.
    /// Moving the status to mastered through the form counts as a read.
    func update(_ original: Book, with draft: BookDraft, gameDay: String? = nil) {
        perform("Saving book") {
            try db.transaction {
                var book = original
                book.title = draft.title
                book.setName = draft.setName
                book.volume = draft.volume
                book.bookKind = draft.bookKind
                book.languageID = draft.languageID
                book.mysteryPrincipleID = draft.mysteryPrincipleID
                book.difficulty = draft.difficulty
                book.readStatus = draft.readStatus
                book.contamination = draft.contamination
                book.location = draft.location
                book.lessons = draft.lessons
                book.yieldedMemoryID = draft.yieldedMemoryID
                book.notes = draft.notes
                try repo.update(book)
                if BookReadTransitions.countsAsRead(original: original, draft: draft) {
                    try logFormMasteredRead(book, gameDay: gameDay)
                }
            }
        }
    }

    func updateNotes(_ book: Book, notes: String) {
        var edited = book
        edited.notes = notes.isEmpty ? nil : notes
        update(edited)
    }

    func setReadStatus(_ book: Book, _ status: ReadStatus, gameDay: String? = nil) {
        perform("Changing read status") {
            try db.transaction {
                try repo.updateReadStatus(book.id, status)
                if status == .mastered && book.readStatus != .mastered {
                    try logFormMasteredRead(book, gameDay: gameDay)
                }
            }
        }
    }

    func delete(_ book: Book) {
        perform("Deleting book") {
            try repo.delete(book.id)
            if selectedBookID == book.id { selectedBookID = nil }
        }
    }

    /// Quick note from the book detail; linked to the book.
    func addJournalNote(book: Book, text: String, gameDay: String?) {
        perform("Adding note") {
            _ = try journalRepo.insert(JournalDraft(gameDay: gameDay, entry: text, bookID: book.id))
        }
    }

    // MARK: Record-a-read flow

    /// Create a memory on the fly from the mark-as-read sheet.
    @discardableResult
    func createMemory(_ draft: MemoryDraft) -> Memory? {
        do {
            // insertOrReuse: an imported (hidden) memory of the same name+kind is
            // the same game entity — reusing it earns it via this read instead of
            // failing on the per-playthrough (name, kind) uniqueness.
            return try memoryRepo.insertOrReuse(draft)
        } catch {
            lastError = "Creating memory failed: \(error)"
            return nil
        }
    }

    /// The full record-a-read transaction: status, counters, yielded memory,
    /// lessons and the Journal entry land together or not at all.
    func recordRead(book: Book, mastering: Bool, usedMemoryID: Int64?,
                    gainedMemory: Memory?, lessons: Int?, gameDay: String?, note: String?) {
        perform("Recording read") {
            let usedName = usedMemoryID.flatMap { memoriesByID[$0]?.name }
            // A read that doesn't earn the memory (non-mastering on an unmastered
            // book) can't reveal its name in the journal either — mask it.
            let willBeMastered = mastering || book.readStatus == .mastered
            try db.transaction {
                if mastering {
                    if book.readStatus != .mastered {
                        try repo.updateReadStatus(book.id, .mastered)
                    }
                    if let lessons {
                        try repo.setLessonsCount(book.id, lessons: lessons)
                    }
                }
                try repo.recordRead(book.id)
                if let gained = gainedMemory {
                    try repo.setYieldedMemory(book.id, memoryID: gained.id)
                }
                _ = try journalRepo.insert(JournalDraft(
                    gameDay: gameDay,
                    entry: ReadingLog.journalText(book: book, mastering: mastering,
                                                   usedMemoryName: usedName,
                                                   gainedMemoryName: gainedMemory.map {
                                                       willBeMastered ? $0.name : "unrevealed memory"
                                                   },
                                                   lessons: lessons, userNote: note),
                    bookID: book.id, memoryID: gainedMemory?.id
                ))
            }
        }
    }
}