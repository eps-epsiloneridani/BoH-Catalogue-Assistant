import XCTest
@testable import BoHLibrarianCore

/// Pure-logic tests for the Memories screen list (search, principle/level filters, sorts).
final class MemoryQueryTests: XCTestCase {

    private let principleNames: [Int64: String] = [
        2: "Heart", 5: "Knock", 6: "Lantern", 7: "Moon", 8: "Moth",
        10: "Rose", 12: "Sky", 13: "Winter",
    ]

    private func aspect(_ principle: Int64, _ level: Int) -> Aspect {
        Aspect(principleID: principle,
               principleName: principleNames[principle] ?? "?", level: level)
    }

    private var cards: [Memory] {
        [
            Memory(id: 1, name: "Horizon-Sight", kind: .memory, persistent: true,
                   aspects: [aspect(10, 4)]),
            Memory(id: 2, name: "Fog", kind: .weather, persistent: false,
                   aspects: [aspect(5, 3), aspect(7, 3)]),
            Memory(id: 3, name: "Numen: That Old Lost Music", kind: .numen, persistent: true,
                   aspects: [aspect(10, 5), aspect(12, 5), aspect(13, 5)]),
            Memory(id: 4, name: "Curious Hunch", kind: .memory, persistent: true,
                   notes: "from swimming at Sea's Edge",
                   aspects: [aspect(2, 3), aspect(5, 4), aspect(6, 3), aspect(8, 3)]),
        ]
    }

    private func options(search: String = "", principle: Int64? = nil, minLevel: Int? = nil,
                         sort: MemorySort = .name) -> MemoryQueryOptions {
        var options = MemoryQueryOptions()
        options.searchText = search
        options.principleID = principle
        options.minLevel = minLevel
        options.sort = sort
        return options
    }

    private func apply(_ options: MemoryQueryOptions) -> [String] {
        MemoryFiltering.apply(cards, options: options, principleNames: principleNames)
            .map(\.name)
    }

    // MARK: Search

    func testSearchMatchesNameNotesKindAndAspectText() {
        XCTAssertEqual(apply(options(search: "hunch")), ["Curious Hunch"])
        XCTAssertEqual(apply(options(search: "swimming")), ["Curious Hunch"], "notes")
        XCTAssertEqual(apply(options(search: "weather")), ["Fog"], "kind")
        XCTAssertEqual(apply(options(search: "rose")).sorted(),
                       ["Horizon-Sight", "Numen: That Old Lost Music"], "principle name")
        XCTAssertEqual(apply(options(search: "knock 4")), ["Curious Hunch"], "aspect text")
    }

    func testEmptySearchReturnsEverythingAndSearchIsTrimmed() {
        XCTAssertEqual(apply(options(search: "")).count, 4)
        XCTAssertEqual(apply(options(search: "   ")).count, 4)
        XCTAssertEqual(apply(options(search: "zzz")).count, 0)
    }

    // MARK: Filters

    func testPrincipleFilter() {
        XCTAssertEqual(apply(options(principle: 10)).sorted(),
                       ["Horizon-Sight", "Numen: That Old Lost Music"])
        XCTAssertEqual(apply(options(principle: 7)), ["Fog"])
        XCTAssertEqual(apply(options(principle: 13)), ["Numen: That Old Lost Music"])
    }

    func testPrincipleFilterWithMinLevel() {
        XCTAssertEqual(apply(options(principle: 10, minLevel: 5)),
                       ["Numen: That Old Lost Music"])
        XCTAssertEqual(apply(options(principle: 5, minLevel: 4)), ["Curious Hunch"])
    }

    func testMinLevelWithoutPrincipleUsesHighestAspect() {
        XCTAssertEqual(apply(options(minLevel: 5)), ["Numen: That Old Lost Music"])
        XCTAssertEqual(apply(options(minLevel: 4)).sorted(),
                       ["Curious Hunch", "Horizon-Sight", "Numen: That Old Lost Music"])
        XCTAssertEqual(apply(options(minLevel: 6)).count, 0)
    }

    // MARK: Sorts

    func testSortByNameIsLocalized() {
        XCTAssertEqual(apply(options(sort: .name)).first, "Curious Hunch")
    }

    func testSortByLevelUsesFilteredPrincipleWhenSet() {
        // Filtering on Knock: Curious Hunch (Knock 4) before Fog (Knock 3).
        XCTAssertEqual(apply(options(principle: 5, sort: .level)), ["Curious Hunch", "Fog"])
    }

    func testSortByLevelWithoutPrincipleUsesHighestAspect() {
        XCTAssertEqual(apply(options(sort: .level)),
                       ["Numen: That Old Lost Music", "Curious Hunch", "Horizon-Sight", "Fog"])
    }

    func testSortByKindPutsNuminaFirstThenWeatherThenMemories() {
        let names = apply(options(sort: .kind))
        XCTAssertEqual(names.first, "Numen: That Old Lost Music")
        XCTAssertEqual(names[1], "Fog", "weather after numina")
        XCTAssertEqual(names[2], "Curious Hunch", "then memories alphabetically")
        XCTAssertEqual(names.last, "Horizon-Sight")
    }

    func testSortByRecency() {
        XCTAssertEqual(apply(options(sort: .recent)).first, "Curious Hunch")
    }
}