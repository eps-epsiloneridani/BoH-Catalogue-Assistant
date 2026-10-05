import Foundation
import Observation
import BoHLibrarianCore

/// UI state for the Books screen. Owns the book list, query options and selection;
/// wraps repository calls, reloading after each mutation (docs/GUI_PLAN.md §Data flow).
@Observable
final class BooksStore {

    private let db: SQLiteDatabase
    private let repo: BookRepository
    private let journalRepo: JournalRepository
    private let memoryRepo: MemoryRepository

    // Lookups for list rows, search and detail hints.
    private var principlesByID: [Int64: Principle] = [:]
    private var languagesByID: [Int64: Language] = [:]
    private var memoriesByID: [Int64: Memory] = [:]
    private var skillNamesByID: [Int64: String] = [:]
    private var nativeLanguageNames: Set<String> = []
    private(set) var knownLanguageSkills: Set<String> = []

    private(set) var books: [Book] = []
    var lastError: String?

    var options = BookQueryOptions()
    var selectedBookID: Int64?

    init(db: SQLiteDatabase) {
        self.db = db
        self.repo = BookRepository(db: db)
        self.journalRepo = JournalRepository(db: db)
        self.memoryRepo = MemoryRepository(db: db)
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

    func lessonSkillNames(for book: Book) -> [String] {
        let entries = (try? repo.lessons(forBook: book.id)) ?? []
        return entries.compactMap { entry -> String? in
            guard let name = skillNamesByID[entry.skillID] else { return nil }
            return entry.amount > 1 ? "\(name) ×\(entry.amount)" : name
        }
        .sorted()
    }

    func journalEntries(for book: Book) -> [JournalEntry] {
        (try? journalRepo.entries(bookID: book.id)) ?? []
    }

    var allMemories: [Memory] {
        (try? memoryRepo.all()) ?? []
    }

    // MARK: Mutations

    func reload() {
        do {
            books = try repo.all()
            memoriesByID = Dictionary(uniqueKeysWithValues: try memoryRepo.all().map { ($0.id, $0) })
            let skills = try SkillRepository(db: db).all()
            skillNamesByID = Dictionary(uniqueKeysWithValues: skills.map { ($0.id, $0.name) })
            knownLanguageSkills = Set(skills.filter(\.isLanguage).map(\.name))
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

    func add(_ draft: BookDraft) {
        perform("Adding book") {
            let book = try repo.insert(draft)
            selectedBookID = book.id
        }
    }

    func update(_ book: Book) {
        perform("Saving book") { try repo.update(book) }
    }

    /// Apply the edit form's draft onto an existing book, keeping read counters.
    func update(_ original: Book, with draft: BookDraft) {
        var book = original
        book.title = draft.title
        book.setName = draft.setName
        book.volume = draft.volume
        book.bookKind = draft.bookKind
        book.languageID = draft.languageID
        book.mysteryPrincipleID = draft.mysteryPrincipleID
        book.mysteryLevel = draft.mysteryLevel
        book.readStatus = draft.readStatus
        book.contamination = draft.contamination
        book.location = draft.location
        book.lessons = draft.lessons
        book.yieldedMemoryID = draft.yieldedMemoryID
        book.notes = draft.notes
        update(book)
    }

    func updateNotes(_ book: Book, notes: String) {
        var edited = book
        edited.notes = notes.isEmpty ? nil : notes
        update(edited)
    }

    func setReadStatus(_ book: Book, _ status: ReadStatus) {
        perform("Changing read status") { try repo.updateReadStatus(book.id, status) }
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
            return try memoryRepo.insert(draft)
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
                                                   gainedMemoryName: gainedMemory?.name,
                                                   lessons: lessons, userNote: note),
                    bookID: book.id, memoryID: gainedMemory?.id
                ))
            }
        }
    }
}