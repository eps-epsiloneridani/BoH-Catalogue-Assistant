import Foundation
import Observation
import BoHLibrarianCore

/// UI state for the Memories screen (mirrors BooksStore).
@Observable
final class MemoriesStore {

    private let db: SQLiteDatabase
    private let repo: MemoryRepository
    private let bookRepo: BookRepository

    private let principlesByID: [Int64: Principle]

    private(set) var memories: [Memory] = []
    private(set) var booksForLinking: [BookRef] = []
    var lastError: String?

    var options = MemoryQueryOptions()
    var selectedMemoryID: Int64?

    init(db: SQLiteDatabase, playthroughID: Int64) {
        self.db = db
        self.repo = MemoryRepository(db: db, playthroughID: playthroughID)
        self.bookRepo = BookRepository(db: db, playthroughID: playthroughID)
        let principles = (try? PrincipleRepository(db: db).all()) ?? []
        principlesByID = Dictionary(uniqueKeysWithValues: principles.map { ($0.id, $0) })
        reload()
    }

    // MARK: Derived

    var displayed: [Memory] {
        MemoryFiltering.apply(memories, options: options,
                              principleNames: principlesByID.mapValues(\.name))
    }

    var selectedMemory: Memory? {
        displayed.first { $0.id == selectedMemoryID }
    }

    // MARK: Lookups

    func principleName(_ id: Int64?) -> String? {
        id.flatMap { principlesByID[$0]?.name }
    }

    func principleColor(_ id: Int64?) -> String? {
        id.flatMap { principlesByID[$0]?.color }
    }

    // MARK: Data

    func reload() {
        do {
            // Earned memories only — imports carry yields of unmastered books that
            // the player can't know yet; all() stays all-inclusive for pickers.
            memories = try repo.allKnown()
            booksForLinking = try bookRepo.all().map { BookRef(id: $0.id, title: $0.title) }
        } catch {
            lastError = "\(error)"
        }
    }

    func perform(_ label: String, _ operation: () throws -> Void) {
        do {
            try operation()
            reload()
        } catch {
            lastError = "\(label) failed: \(error)"
        }
    }

    // MARK: CRUD

    func add(_ draft: MemoryDraft) {
        perform("Adding memory") { selectedMemoryID = try repo.insert(draft).id }
    }

    func update(_ memory: Memory) {
        perform("Saving memory") { try repo.update(memory) }
    }

    /// Apply the edit form's draft onto an existing memory, keeping the id.
    func update(_ original: Memory, with draft: MemoryDraft) {
        var memory = original
        memory.name = draft.name
        memory.kind = draft.kind
        memory.persistent = draft.persistent
        memory.notes = draft.notes
        memory.aspects = draft.aspects.map {
            Aspect(principleID: $0.principleID,
                   principleName: principleName($0.principleID) ?? "?",
                   level: $0.level)
        }
        update(memory)
    }

    func updateNotes(_ memory: Memory, notes: String) {
        var edited = memory
        edited.notes = notes.isEmpty ? nil : notes
        update(edited)
    }

    func delete(_ memory: Memory) {
        perform("Deleting memory") {
            try repo.delete(memory.id)
            if selectedMemoryID == memory.id { selectedMemoryID = nil }
        }
    }

    /// Replace all aspects (persisted immediately when editing inline).
    // MARK: Sources

    func sources(for memoryID: Int64) -> [MemorySource] {
        (try? repo.sources(for: memoryID)) ?? []
    }

    /// Every yielding link, any status — the edit form edits the full set.
    func allYielding(_ memoryID: Int64) -> [BookRef] {
        (try? repo.allYielding(memoryID)) ?? []
    }

    func setSources(_ memoryID: Int64, _ sources: [MemorySource]) {
        perform("Saving sources") { try repo.setSources(memoryID, sources) }
    }

    func setYieldingBooks(_ memoryID: Int64, _ bookIDs: [Int64]) {
        perform("Saving yielding books") { try repo.setYieldingBooks(memoryID, bookIDs) }
    }

    // MARK: Book backlinks

    func booksYielding(_ memoryID: Int64) -> [BookRef] {
        (try? repo.booksYielding(memoryID)) ?? []
    }

}