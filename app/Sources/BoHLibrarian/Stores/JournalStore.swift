import Foundation
import Observation
import BoHLibrarianCore

/// UI state for the Journal screen: the findings timeline, quick capture, and
/// entity-link lookups for display and editing.
@Observable
final class JournalStore {

    private let repo: JournalRepository
    private let bookRepo: BookRepository
    private let memoryRepo: MemoryRepository
    private let skillRepo: SkillRepository

    private(set) var entries: [JournalEntry] = []
    var lastError: String?

    var searchText = ""
    /// Set by the ⌘⇧J global command; the screen consumes it to focus quick-add.
    var requestFocus = false

    init(db: SQLiteDatabase, playthroughID: Int64) {
        self.repo = JournalRepository(db: db, playthroughID: playthroughID)
        self.bookRepo = BookRepository(db: db, playthroughID: playthroughID)
        self.memoryRepo = MemoryRepository(db: db, playthroughID: playthroughID)
        self.skillRepo = SkillRepository(db: db, playthroughID: playthroughID)
        reload()
    }

    // MARK: Timeline rows

    /// A row with the day header that precedes it, if the in-game day changed.
    struct Row: Identifiable {
        let header: String?
        let entry: JournalEntry
        var id: Int64 { entry.id }
    }

    var rows: [Row] {
        let query = searchText.trimmingCharacters(in: .whitespacesAndNewlines)
        let filtered: [JournalEntry]
        if query.isEmpty {
            filtered = entries
        } else {
            let needle = query.lowercased()
            filtered = entries.filter { entry in
                [entry.entry, entry.gameDay]
                    .compactMap { $0 }
                    .joined(separator: " ")
                    .lowercased()
                    .contains(needle)
            }
        }
        var rows: [Row] = []
        var previousDay: String?
        var hasPrevious = false
        for entry in filtered {
            let day = entry.gameDay
            // Header whenever the in-game day changes (newest first), so each
            // day's findings group visually.
            let header: String?
            if !hasPrevious || day != previousDay {
                header = day ?? "No in-game day noted"
            } else {
                header = nil
            }
            rows.append(Row(header: header, entry: entry))
            previousDay = day
            hasPrevious = true
        }
        return rows
    }

    // MARK: Data

    func reload() {
        do {
            entries = try repo.recent(limit: 500)
            refreshLinkData()
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

    /// Quick capture: today's game day + whatever you noticed.
    func quickAdd(text: String, gameDay: String?) {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        perform("Adding note") {
            _ = try repo.insert(JournalDraft(gameDay: gameDay, entry: trimmed))
        }
    }

    @discardableResult
    func add(_ draft: JournalDraft) -> Bool {
        do {
            _ = try repo.insert(draft)
            reload()
            return true
        } catch {
            lastError = "Adding entry failed: \(error)"
            return false
        }
    }

    func update(_ entry: JournalEntry) {
        perform("Saving entry") { try repo.update(entry) }
    }

    func delete(_ entry: JournalEntry) {
        perform("Deleting entry") { try repo.delete(entry.id) }
    }

    // MARK: Link lookups

    private var bookTitles: [Int64: String] = [:]
    private var memoryNames: [Int64: String] = [:]
    private var skillNames: [Int64: String] = [:]

    /// Lists for the edit sheet's link pickers (loaded on demand).
    func refreshLinkData() {
        bookTitles = Dictionary(uniqueKeysWithValues:
            ((try? bookRepo.all()) ?? []).map { ($0.id, $0.title) })
        memoryNames = Dictionary(uniqueKeysWithValues:
            ((try? memoryRepo.all()) ?? []).map { ($0.id, $0.name) })
        skillNames = Dictionary(uniqueKeysWithValues:
            ((try? skillRepo.all()) ?? []).map { ($0.id, $0.name) })
    }

    func bookTitle(_ id: Int64?) -> String? { id.flatMap { bookTitles[$0] } }
    func memoryName(_ id: Int64?) -> String? { id.flatMap { memoryNames[$0] } }
    func skillName(_ id: Int64?) -> String? { id.flatMap { skillNames[$0] } }

    var bookPickerList: [BookRef] {
        ((try? bookRepo.all()) ?? []).map { BookRef(id: $0.id, title: $0.title) }
    }

    var memoryPickerList: [Memory] {
        (try? memoryRepo.all()) ?? []
    }

    var skillPickerList: [Skill] {
        (try? skillRepo.all()) ?? []
    }
}