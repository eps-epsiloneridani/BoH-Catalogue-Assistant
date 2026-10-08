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

    /// In-pane navigation: "open the relevant item" = switch the section and set
    /// its selection (no windows). Used by clickable entity links (backlinks,
    /// yield pointers, journal chips).
    func showBook(_ id: Int64) {
        section = .books
        booksStore?.selectedBookID = id
        booksStore?.reload()
    }

    func showMemory(_ id: Int64) {
        section = .memories
        memoriesStore?.selectedMemoryID = id
        memoriesStore?.reload()
    }

    func showSkill(_ id: Int64) {
        section = .skills
        skillsStore?.selectedSkillID = id
        skillsStore?.reload()
    }

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
    private(set) var helperStore: ReadingHelperStore?
    private(set) var skillsStore: SkillsStore?
    private(set) var journalStore: JournalStore?
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
        helperStore = ReadingHelperStore(db: db, playthroughID: playthrough.id)
        skillsStore = SkillsStore(db: db, playthroughID: playthrough.id)
        journalStore = JournalStore(db: db, playthroughID: playthrough.id)
        refreshCounts()
    }

    private func refreshCounts() {
        bookCount = booksStore?.books.count ?? 0
        memoryCount = memoriesStore?.memories.count ?? 0
        skillCount = skillsStore?.skills.count ?? 0
        // Journal rows are capped at 500 in the store; fall back to the exact count
        // for runs that outgrow it.
        if let journal = journalStore, journal.entries.count < 500 {
            journalCount = journal.entries.count
        } else if let db, let playthrough = activePlaythrough {
            journalCount = (try? db.scalarInt(
                "SELECT COUNT(*) FROM Journal WHERE playthrough_id = ?;", [playthrough.id])) ?? 0
        }
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

    // MARK: Save import

    /// Import a save game into a playthrough (docs/SAVE_IMPORT.md). Creates the
    /// destination playthrough when `asNewPlaythrough`; on success with a new
    /// playthrough it becomes the active one. The parse/write runs on the main
    /// thread (the db is main-thread confined) — the UI shows progress state.
    @discardableResult
    func importSave(_ summary: SaveGameSummary, asNewPlaythrough: Bool,
                    playthroughName: String) -> ImportReport? {
        guard let db, let repo = playthroughRepo else { return nil }
        do {
            guard let elementsDirectory = BoHPaths.gameElementsDirectory(),
                  FileManager.default.fileExists(atPath: elementsDirectory.path) else {
                throw SaveImportError.gameDataNotFound
            }
            var targetID = activePlaythrough?.id
            var report: ImportReport?
            try db.transaction {
                if asNewPlaythrough {
                    let trimmed = playthroughName.trimmingCharacters(in: .whitespacesAndNewlines)
                    let name = trimmed.isEmpty ? "Imported from \(summary.stem)" : trimmed
                    let versionNote = summary.gameVersion.map { " (game \($0))" } ?? ""
                    let created = try repo.insert(name: name,
                                                  notes: "Imported from \(summary.fileName)\(versionNote)")
                    targetID = created.id
                }
                guard let id = targetID else { throw SaveImportError.noActivePlaythrough }
                report = try SaveImporter.run(saveURL: summary.url, db: db,
                                              playthroughID: id,
                                              elementsDirectory: elementsDirectory)
            }
            if asNewPlaythrough, let id = targetID {
                try repo.setActiveID(id)
                activePlaythrough = try repo.get(id)
                rebuildStores()
            }
            playthroughs = try repo.all()
            return report
        } catch {
            playthroughError = "Import failed: \(error)"
            return nil
        }
    }
}