import Foundation

// Client-side list queries for the Skills screen — same pure-function pattern as
// BookQuery/MemoryQuery (docs/GUI_PLAN.md §Testing).

public enum SkillSort: String, CaseIterable, Identifiable {
    case name = "Name"
    case level = "Level"
    case recent = "Recently added"

    public var id: String { rawValue }
}

public enum SkillKindFilter: String, CaseIterable, Identifiable {
    case all = "All"
    case languages = "Languages"
    case skills = "Skills"

    public var id: String { rawValue }
}

public struct SkillQueryOptions {
    public var searchText: String = ""
    /// Skills with this principle as primary OR secondary.
    public var principleID: Int64?
    public var kindFilter: SkillKindFilter = .all
    public var sort: SkillSort = .name

    public init() {}
}

public enum SkillFiltering {

    public static func apply(
        _ skills: [Skill],
        options: SkillQueryOptions,
        principleNames: [Int64: String] = [:]
    ) -> [Skill] {
        var result = skills

        let query = options.searchText.trimmingCharacters(in: .whitespacesAndNewlines)
        if !query.isEmpty {
            let needle = query.lowercased()
            result = result.filter { skill in
                [skill.name, skill.wisdom, skill.element, skill.notes]
                    .compactMap { $0 }
                    .joined(separator: " ")
                    .lowercased()
                    .contains(needle)
            }
        }

        if let principleID = options.principleID {
            result = result.filter {
                $0.primaryPrincipleID == principleID || $0.secondaryPrincipleID == principleID
            }
        }

        switch options.kindFilter {
        case .all:
            break
        case .languages:
            result = result.filter(\.isLanguage)
        case .skills:
            result = result.filter { !$0.isLanguage }
        }

        switch options.sort {
        case .name:
            result.sort { $0.name.localizedStandardCompare($1.name) == .orderedAscending }
        case .level:
            result.sort {
                let lhs = $0.level ?? -1
                let rhs = $1.level ?? -1
                return lhs == rhs
                    ? $0.name.localizedStandardCompare($1.name) == .orderedAscending
                    : lhs > rhs
            }
        case .recent:
            result.sort { $0.id > $1.id }
        }

        return result
    }
}

/// The desk math for a single skill: a level-L card carries L+1 of its primary
/// principle and L of its secondary (docs/GAME_MECHANICS.md §Skills — "level 9
/// gives 10 of one aspect and 9 of the other").
public enum SkillMath {
    public static func contributions(level: Int) -> (primary: Int, secondary: Int) {
        (primary: level + 1, secondary: level)
    }
}