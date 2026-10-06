import XCTest
@testable import BoHLibrarianCore

/// The Reading Helper's user-facing sentences, pinned by tests.
final class ReadingMathTests: XCTestCase {

    // MARK: Requirement line

    func testRequirementLineComplete() {
        XCTAssertEqual(ReadingMath.requirementLine(principleName: "Rose", difficulty: 6),
                       "You need Rose 6.")
    }

    func testRequirementLineMissingPrinciple() {
        XCTAssertEqual(ReadingMath.requirementLine(principleName: nil, difficulty: 10),
                       "Difficulty 10 recorded, but not its principle — "
                       + "note the mystery principle on the Books screen to compute candidates.")
    }

    func testRequirementLineMissingDifficulty() {
        XCTAssertEqual(ReadingMath.requirementLine(principleName: "Rose", difficulty: nil),
                       "The mystery is Rose, but its difficulty isn't recorded yet.")
    }

    func testRequirementLineNothingRecorded() {
        XCTAssertEqual(ReadingMath.requirementLine(principleName: nil, difficulty: nil),
                       "Not catalogued yet — record its mystery on the Books screen.")
    }

    // MARK: Reach line

    func testReachLineEnoughWithMemoryAndSkill() {
        XCTAssertEqual(ReadingMath.reachLine(difficulty: 6, memory: 4, skill: 3),
                       "Best recorded: memory 4 + skill 3 = 7 — enough, before souls, inks and tools.")
    }

    func testReachLineExactlyEnough() {
        XCTAssertEqual(ReadingMath.reachLine(difficulty: 6, memory: 6, skill: nil),
                       "Best recorded: memory 6 = 6 — enough, before souls, inks and tools.")
    }

    func testReachLineShortWithSkillOnly() {
        XCTAssertEqual(ReadingMath.reachLine(difficulty: 10, memory: nil, skill: 4),
                       "Best recorded: skill 4 = 4 — 6 short, before souls, inks and tools.")
    }

    func testReachLineNothingRecorded() {
        XCTAssertEqual(ReadingMath.reachLine(difficulty: 4, memory: nil, skill: nil),
                       "Nothing recorded yet reaches for it.")
    }

    func testReachLineNeedsADifficulty() {
        XCTAssertNil(ReadingMath.reachLine(difficulty: nil, memory: 4, skill: 3))
    }
}