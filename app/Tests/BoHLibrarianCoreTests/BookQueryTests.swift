import XCTest
@testable import BoHLibrarianCore

/// Pure-logic tests for the Books screen: filtering, sorting, search, and the
/// journal text composed by the record-a-read flow.
final class BookQueryTests: XCTestCase {

    private let principleNames: [Int64: String] = [10: "Rose", 11: "Scale", 12: "Sky"]
    private let languageNames: [Int64: String] = [3: "Latin", 7: "Fucine"]

    private func book(_ title: String, id: Int64, difficulty: (Int64, Int)? = nil,
                      language: Int64? = nil, status: ReadStatus = .uncatalogued,
                      contamination: Contamination? = nil, set: String? = nil,
                      notes: String? = nil) -> Book {
        Book(id: id, title: title, setName: set, bookKind: .book, languageID: language,
             mysteryPrincipleID: difficulty?.0, difficulty: difficulty?.1, readStatus: status,
             contamination: contamination, notes: notes)
    }

    private var library: [Book] {
        [
            book("The Turquoise Hand", id: 1, difficulty: (10, 10), language: 7, status: .catalogued,
                 set: "Numen books", notes: "persistent Rose memory inside"),
            book("Annals of St Brandans", id: 2, difficulty: (11, 4), language: 3, status: .mastered,
                 contamination: .clear),
            book("An Introduction to Histories", id: 3, language: 3),
            book("De Bellis Murorum", id: 4, difficulty: (12, 6), contamination: .curse),
        ]
    }

    private func options(search: String = "", filter: BookStatusFilter = .all,
                         sort: BookSort = .title) -> BookQueryOptions {
        var options = BookQueryOptions()
        options.searchText = search
        options.statusFilter = filter
        options.sort = sort
        return options
    }

    private func apply(_ options: BookQueryOptions) -> [String] {
        BookFiltering.apply(library, options: options,
                            principleNames: principleNames, languageNames: languageNames)
            .map(\.title)
    }

    // MARK: Search

    /// The form's read-status picker can master a book outright — that counts as
    /// an actual read (counters + journal), never double-counting.
    func testFormMasteryCountsAsRead() {
        let mastered = BookDraft(title: "Low Mystery", readStatus: .mastered)
        let catalogued = BookDraft(title: "Low Mystery", readStatus: .catalogued)

        XCTAssertTrue(BookReadTransitions.countsAsRead(original: nil, draft: mastered),
                      "created as mastered = one read")
        XCTAssertFalse(BookReadTransitions.countsAsRead(original: nil, draft: catalogued))

        let book = Book(id: 1, title: "Low Mystery", readStatus: .catalogued)
        let alreadyMastered = Book(id: 2, title: "Low Mystery", readStatus: .mastered)
        XCTAssertTrue(BookReadTransitions.countsAsRead(original: book, draft: mastered))
        XCTAssertFalse(BookReadTransitions.countsAsRead(original: alreadyMastered, draft: mastered))
        XCTAssertFalse(BookReadTransitions.countsAsRead(original: book, draft: catalogued))
    }

    func testSearchMatchesTitleCaseInsensitively() {
        XCTAssertEqual(apply(options(search: "turquoise")), ["The Turquoise Hand"])
        XCTAssertEqual(apply(options(search: "ANNALS")), ["Annals of St Brandans"])
    }

    func testSearchMatchesSetNameNotesPrincipleNameAndLanguageName() {
        XCTAssertEqual(apply(options(search: "numen")), ["The Turquoise Hand"], "set name")
        XCTAssertEqual(apply(options(search: "persistent")), ["The Turquoise Hand"], "notes")
        XCTAssertEqual(apply(options(search: "rose")), ["The Turquoise Hand"], "principle name")
        XCTAssertEqual(apply(options(search: "fucine")), ["The Turquoise Hand"], "language name")
    }

    func testEmptyAndWhitespaceSearchReturnsEverything() {
        XCTAssertEqual(apply(options(search: "")).count, 4)
        XCTAssertEqual(apply(options(search: "   ")).count, 4, "search must be trimmed")
        XCTAssertEqual(apply(options(search: "nothing matches this")).count, 0)
    }

    // MARK: Filters

    func testStatusFilters() {
        XCTAssertEqual(apply(options(filter: .unread)), ["An Introduction to Histories", "De Bellis Murorum", "The Turquoise Hand"])
        XCTAssertEqual(apply(options(filter: .uncatalogued)), ["An Introduction to Histories", "De Bellis Murorum"])
        XCTAssertEqual(apply(options(filter: .catalogued)), ["The Turquoise Hand"])
        XCTAssertEqual(apply(options(filter: .mastered)), ["Annals of St Brandans"])
    }

    func testContaminatedFilterExcludesClearAndUnknown() {
        XCTAssertEqual(apply(options(filter: .contaminated)), ["De Bellis Murorum"],
                       "checked-clean (contamination = none) and unchecked (nil) books don't count")
    }

    // MARK: Sorts

    func testSortByTitleIsLocalized() {
        XCTAssertEqual(apply(options(sort: .title)).first, "An Introduction to Histories")
    }

    func testSortByDifficultyIsDescendingWithUnknownsLast() {
        XCTAssertEqual(apply(options(sort: .difficulty)),
                      ["The Turquoise Hand", "De Bellis Murorum", "Annals of St Brandans", "An Introduction to Histories"])
    }

    func testSortByStatusGroupsUnreadFirst() {
        XCTAssertEqual(apply(options(sort: .status)).last, "Annals of St Brandans", "mastered last")
        XCTAssertEqual(apply(options(sort: .status)).first, "An Introduction to Histories",
                       "unread first; ties break alphabetically")
    }

    func testSortByRecency() {
        XCTAssertEqual(apply(options(sort: .recent)).first, "De Bellis Murorum")
    }

    // MARK: ReadingLog

    func testReadingLogMasteringWithEverything() {
        let text = ReadingLog.journalText(
            book: book("The Turquoise Hand", id: 1),
            mastering: true,
            usedMemoryName: "Horizon-Sight",
            gainedMemoryName: "Numen: That Old Lost Music",
            lessons: 2,
            userNote: "Found in the Silver Vault."
        )
        XCTAssertEqual(text,
                       "Mastered “The Turquoise Hand”. Read with: Horizon-Sight. "
                       + "Memory gained: Numen: That Old Lost Music. Lessons learned: 2. Found in the Silver Vault.")
    }

    func testReadingLogReReadSkipsLessonsAndTrimsNote() {
        let text = ReadingLog.journalText(
            book: book("The Turquoise Hand", id: 1),
            mastering: false,
            usedMemoryName: nil,
            gainedMemoryName: "Numen: That Old Lost Music",
            lessons: 2,
            userNote: "   grinding memories for Numa   "
        )
        XCTAssertEqual(text,
                       "Re-read “The Turquoise Hand”. Memory gained: Numen: That Old Lost Music. "
                       + "grinding memories for Numa")
        XCTAssertFalse(text.contains("Lessons"), "re-reads don't grant lessons")
    }

    func testReadingLogEmptyOptionalsStaySilent() {
        let text = ReadingLog.journalText(book: book("Plain Book", id: 1), mastering: true,
                                          usedMemoryName: nil, gainedMemoryName: "",
                                          lessons: nil, userNote: nil)
        XCTAssertEqual(text, "Mastered “Plain Book”.")
    }
}