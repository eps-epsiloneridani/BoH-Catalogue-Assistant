import Foundation

// Client-side list queries for the Books screen. Kept in Core as pure functions so
// they're unit-testable (docs/GUI_PLAN.md §Testing; the executable target has no
// test target of its own).

// MARK: - Filters & sorts

public enum BookStatusFilter: String, CaseIterable, Identifiable {
    case all = "All"
    case unread = "Unread"
    case uncatalogued = "Uncatalogued"
    case catalogued = "Catalogued"
    case mastered = "Mastered"
    case contaminated = "Contaminated"

    public var id: String { rawValue }
}

public enum BookSort: String, CaseIterable, Identifiable {
    case title = "Title"
    case mystery = "Mystery"
    case status = "Status"
    case recent = "Recently added"

    public var id: String { rawValue }
}

public struct BookQueryOptions {
    public var searchText: String = ""
    public var statusFilter: BookStatusFilter = .all
    public var sort: BookSort = .title

    public init() {}
}

public enum BookFiltering {

    /// Filter + sort `books` for display. `principleNames`/`languageNames` let the
    /// search match the *names* a book's IDs point at, not just raw columns.
    public static func apply(
        _ books: [Book],
        options: BookQueryOptions,
        principleNames: [Int64: String] = [:],
        languageNames: [Int64: String] = [:]
    ) -> [Book] {
        var result = books

        // Search: case-insensitive substring across the visible text a player knows.
        let query = options.searchText.trimmingCharacters(in: .whitespacesAndNewlines)
        if !query.isEmpty {
            let needle = query.lowercased()
            result = result.filter { book in
                let haystack = [
                    book.title, book.setName, book.volume, book.location, book.notes,
                    book.mysteryPrincipleID.flatMap { principleNames[$0] },
                    book.languageID.flatMap { languageNames[$0] },
                ]
                .compactMap { $0 }
                .joined(separator: " ")
                .lowercased()
                return haystack.contains(needle)
            }
        }

        switch options.statusFilter {
        case .all:
            break
        case .unread:
            result = result.filter { $0.readStatus != .mastered }
        case .uncatalogued:
            result = result.filter { $0.readStatus == .uncatalogued }
        case .catalogued:
            result = result.filter { $0.readStatus == .catalogued }
        case .mastered:
            result = result.filter { $0.readStatus == .mastered }
        case .contaminated:
            result = result.filter { $0.contamination != nil && $0.contamination != .clear }
        }

        switch options.sort {
        case .title:
            result.sort { $0.title.localizedStandardCompare($1.title) == .orderedAscending }
        case .mystery:
            result.sort {
                let lhs = $0.mysteryLevel ?? Int.min
                let rhs = $1.mysteryLevel ?? Int.min
                return lhs == rhs
                    ? $0.title.localizedStandardCompare($1.title) == .orderedAscending
                    : lhs > rhs
            }
        case .status:
            result.sort {
                let lhs = Self.readRank($0)
                let rhs = Self.readRank($1)
                return lhs == rhs
                    ? $0.title.localizedStandardCompare($1.title) == .orderedAscending
                    : lhs < rhs
            }
        case .recent:
            result.sort { $0.id > $1.id }
        }

        return result
    }

    private static func readRank(_ book: Book) -> Int {
        switch book.readStatus {
        case .uncatalogued: return 0
        case .catalogued: return 1
        case .mastered: return 2
        }
    }
}

// MARK: - Journal composition

/// Composes the Journal entry written by the record-a-read flow. Pure and tested:
/// the wording is user-facing, so changes show up in git as test diffs.
public enum ReadingLog {

    public static func journalText(
        book: Book,
        mastering: Bool,
        usedMemoryName: String?,
        gainedMemoryName: String?,
        lessons: Int?,
        userNote: String?
    ) -> String {
        var sentences: [String] = []
        sentences.append(mastering ? "Mastered “\(book.title)”." : "Re-read “\(book.title)”.")
        if let used = usedMemoryName, !used.isEmpty {
            sentences.append("Read with: \(used).")
        }
        if let gained = gainedMemoryName, !gained.isEmpty {
            sentences.append("Memory gained: \(gained).")
        }
        if mastering, let lessons {
            sentences.append("Lessons learned: \(lessons).")
        }
        if let note = userNote?.trimmingCharacters(in: .whitespacesAndNewlines), !note.isEmpty {
            sentences.append(note)
        }
        return sentences.joined(separator: " ")
    }
}