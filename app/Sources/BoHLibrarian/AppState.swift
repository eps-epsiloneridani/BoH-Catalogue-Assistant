import Foundation
import Observation
import BoHLibrarianCore

/// App-wide state. Owns the single database connection, the playthrough
/// lifecycle, and the per-screen stores for the active playthrough.
/// The database is main-thread confined by convention (docs/GUI_PLAN.md §Data flow).
@Observable
final class AppState {

    enum Phase: Equatable {
        case loading
        case ready
        case failed(String)
    }

    private(set) var phase: Phase = .loading
    var section: AppSection = .books

    /// Free text for "which in-game day is it" — pre-fills journal entries and the
    /// record-read sheet; kept for the whole session.
    var currentGameDay = ""

    private(set) var db: SQLiteDatabase?
    private(set) var dbPath: String = ""
    private(set) var schemaVersion: Int = 0
    private(set) var principles: [Principle] = []
    private(set) var languages: [Language] = []

    // MARK: Playthroughs

    private(set) var playthroughs: [Playthrough] = []
    private(set) var activePlaythrough: Playthrough?
    private var playthroughRepo: PlaythroughRepository?
    /// Playthrough-level errors (create/switch/delete) surface at root level.
    var playthroughError: String?

    // MARK: Screen stores (rebuilt when the active playthrough changes)

    private(set) var booksStore: BooksStore?
    private(set) var memoriesStore: MemoriesStore?
    private(set) var bookCount = 0
    private(set) var memoryCount = 0
    private(set) var skillCount = 0
    private(set) var journalCount = 0

    init() {
        bootstrap()
    }

    private func bootstrap() {
        do {
            let path = try DatabaseLocation.resolvePath()
            let database = try SQLiteDatabase(path: path)
            let migrator = try Migrator.resolve()
            try migrator.apply(to: database)

            db = database
            dbPath = path
            schemaVersion = database.userVersion
            principles = try PrincipleRepository(db: database).all()
            languages = try LanguageRepository(db: database).all()

            let repo = PlaythroughRepository(db: database)
            playthroughRepo = repo
            var active = try repo.active()
            if active == nil {
                // Belt and braces: a db older than migration 005 with no playthroughs.
                active = try repo.insert(name: "First playthrough")
            }
            activePlaythrough = active
            playthroughs = try repo.all()
            rebuildStores()
            phase = .ready
        } catch {
            phase = .failed(String(describing: error))
        }
    }

    // MARK: Store plumbing

    /// Recreates the scoped stores for the active playthrough. Old playthrough
    /// data stays in the db untouched — switching back is one click.
    private func rebuildStores() {
        guard let db, let playthrough = activePlaythrough else { return }
        booksStore = BooksStore(db: db, playthroughID: playthrough.id)
        memoriesStore = MemoriesStore(db: db, playthroughID: playthrough.id)
        refreshCounts()
    }

    private func refreshCounts() {
        guard let db, let playthrough = activePlaythrough else { return }
        bookCount = booksStore?.books.count ?? 0
        memoryCount = memoriesStore?.memories.count ?? 0
        skillCount = (try? db.scalarInt(
            "SELECT COUNT(*) FROM Skills WHERE playthrough_id = ?;", [playthrough.id])) ?? 0
        journalCount = (try? db.scalarInt(
            "SELECT COUNT(*) FROM Journal WHERE playthrough_id = ?;", [playthrough.id])) ?? 0
    }

    // MARK: Playthrough lifecycle

    /// Load another playthrough ("load a saved game").
    func switchPlaythrough(to id: Int64) {
        guard let repo = playthroughRepo, let db else { return }
        guard id != activePlaythrough?.id else { return }
        do {
            try repo.setActiveID(id)
            activePlaythrough = try repo.get(id)
            rebuildStores()
        } catch {
            playthroughError = "Switching playthrough failed: \(error)"
        }
    }

    /// Create a fresh playthrough and load it ("new game"); the old one is saved
    /// as-is. Empty names get a sensible default.
    @discardableResult
    func createPlaythrough(name: String, notes: String) -> Bool {
        guard let repo = playthroughRepo else { return false }
        do {
            let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
            let finalName = trimmed.isEmpty ? "Playthrough \(playthroughs.count + 1)" : trimmed
            let trimmedNotes = notes.trimmingCharacters(in: .whitespacesAndNewlines)
            let created = try repo.insert(name: finalName,
                                           notes: trimmedNotes.isEmpty ? nil : trimmedNotes)
            try repo.setActiveID(created.id)
            activePlaythrough = created
            playthroughs = try repo.all()
            rebuildStores()
            return true
        } catch {
            playthroughError = "Creating playthrough failed: \(error)"
            return false
        }
    }

    func renamePlaythrough(_ playthrough: Playthrough, to name: String) {
        guard let repo = playthroughRepo else { return }
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        var edited = playthrough
        edited.name = trimmed
        do {
            try repo.update(edited)
            if playthrough.id == activePlaythrough?.id {
                activePlaythrough = try repo.get(playthrough.id) ?? edited
            }
            playthroughs = try repo.all()
        } catch {
            playthroughError = "Renaming playthrough failed: \(error)"
        }
    }

    /// Delete a playthrough and everything recorded in it. Guarded at the UI
    /// level: never the active one, never the last one, always confirmed.
    func deletePlaythrough(_ playthrough: Playthrough) {
        guard let repo = playthroughRepo else { return }
        guard playthrough.id != activePlaythrough?.id else {
            playthroughError = "Switch to another playthrough before deleting this one."
            return
        }
        guard playthroughs.count > 1 else {
            playthroughError = "Can't delete the only playthrough."
            return
        }
        do {
            try repo.delete(playthrough.id)
            playthroughs = try repo.all()
        } catch {
            playthroughError = "Deleting playthrough failed: \(error)"
        }
    }
}