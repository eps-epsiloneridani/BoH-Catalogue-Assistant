import Foundation
import Observation
import BoHLibrarianCore

/// App-wide state. Owns the single database connection and the cached lookups.
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

    /// Free text for “which in-game day is it” — pre-fills journal entries and the
    /// record-read sheet; kept for the whole session.
    var currentGameDay = ""

    private(set) var db: SQLiteDatabase?
    private(set) var dbPath: String = ""
    private(set) var schemaVersion: Int = 0
    private(set) var principles: [Principle] = []
    private(set) var languages: [Language] = []
    private(set) var bookCount: Int = 0
    private(set) var memoryCount: Int = 0
    private(set) var skillCount: Int = 0
    private(set) var journalCount: Int = 0
    private(set) var booksStore: BooksStore?
    private(set) var memoriesStore: MemoriesStore?

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
            bookCount = try database.scalarInt("SELECT COUNT(*) FROM Books;")
            memoryCount = try database.scalarInt("SELECT COUNT(*) FROM Memories;")
            skillCount = try database.scalarInt("SELECT COUNT(*) FROM Skills;")
            journalCount = try database.scalarInt("SELECT COUNT(*) FROM Journal;")
            booksStore = BooksStore(db: database)
            memoriesStore = MemoriesStore(db: database)
            phase = .ready
        } catch {
            phase = .failed(String(describing: error))
        }
    }
}