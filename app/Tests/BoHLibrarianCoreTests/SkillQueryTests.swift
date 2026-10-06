import XCTest
@testable import BoHLibrarianCore

/// Pure-logic tests for the Skills screen list, plus the level→contribution math.
final class SkillQueryTests: XCTestCase {

    private let principleNames: [Int64: String] = [
        5: "Knock", 7: "Moon", 8: "Moth", 10: "Rose", 12: "Sky", 1: "Edge",
    ]

    private func skill(_ name: String, id: Int64, primary: Int64?, secondary: Int64?,
                       level: Int? = nil, language: Bool = false,
                       wisdom: String? = nil, notes: String? = nil) -> Skill {
        Skill(id: id, name: name, isLanguage: language,
              primaryPrincipleID: primary, secondaryPrincipleID: secondary,
              level: level, wisdom: wisdom, notes: notes)
    }

    private var roster: [Skill] {
        [
            skill("Sky Stories", id: 1, primary: 12, secondary: 10, level: 3,
                  wisdom: "Birdsong", notes: "crafts Wind-in-Waiting"),
            skill("Vak", id: 2, primary: 5, secondary: 10, level: 2, language: true),
            skill("Edicts Martial", id: 3, primary: 7, secondary: 1, level: nil),
            skill("Sacra Limiae", id: 4, primary: 8, secondary: 12, level: 5),
        ]
    }

    private func options(search: String = "", principle: Int64? = nil,
                         kind: SkillKindFilter = .all,
                         sort: SkillSort = .name) -> SkillQueryOptions {
        var options = SkillQueryOptions()
        options.searchText = search
        options.principleID = principle
        options.kindFilter = kind
        options.sort = sort
        return options
    }

    private func apply(_ options: SkillQueryOptions) -> [String] {
        SkillFiltering.apply(roster, options: options, principleNames: principleNames)
            .map(\.name)
    }

    // MARK: Search

    func testSearchMatchesNameWisdomAndNotes() {
        XCTAssertEqual(apply(options(search: "stories")), ["Sky Stories"])
        XCTAssertEqual(apply(options(search: "birdsong")), ["Sky Stories"], "wisdom")
        XCTAssertEqual(apply(options(search: "wind-in-waiting")), ["Sky Stories"], "notes")
        XCTAssertEqual(apply(options(search: "vak")), ["Vak"])
    }

    // MARK: Filters

    func testPrincipleFilterMatchesPrimaryOrSecondary() {
        // Sky appears as Sky Stories' primary AND Sacra Limiae's secondary.
        XCTAssertEqual(apply(options(principle: 12)).sorted(),
                       ["Sacra Limiae", "Sky Stories"])
        XCTAssertEqual(apply(options(principle: 10)), ["Sky Stories", "Vak"].sorted())
    }

    func testKindFilterSeparatesLanguagesFromSkills() {
        XCTAssertEqual(apply(options(kind: .languages)), ["Vak"])
        XCTAssertEqual(apply(options(kind: .skills)).count, 3)
        XCTAssertEqual(apply(options(kind: .all)).count, 4)
    }

    // MARK: Sorts

    func testSortByLevelDescendingWithUnknownsLast() {
        XCTAssertEqual(apply(options(sort: .level)),
                       ["Sacra Limiae", "Sky Stories", "Vak", "Edicts Martial"],
                       "level 5, 3, 2, then the unleveled skill last")
    }

    func testSortByNameAndRecent() {
        XCTAssertEqual(apply(options(sort: .name)).first, "Edicts Martial")
        XCTAssertEqual(apply(options(sort: .recent)).first, "Sacra Limiae")
    }

    // MARK: Skill math

    func testLevelOneSkillShowsTwoPrimaryOneSecondary() {
        let contributions = SkillMath.contributions(level: 1)
        XCTAssertEqual(contributions.primary, 2)
        XCTAssertEqual(contributions.secondary, 1)
    }

    func testLevelNineSkillShowsTenAndNine() {
        let contributions = SkillMath.contributions(level: 9)
        XCTAssertEqual(contributions.primary, 10)
        XCTAssertEqual(contributions.secondary, 9)
    }
}