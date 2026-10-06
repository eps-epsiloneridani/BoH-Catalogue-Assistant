import Foundation

// Client-side list queries for the Memories screen — pure functions, unit-tested
// (same pattern as BookQuery; the executable target has no test target).

public enum MemorySort: String, CaseIterable, Identifiable {
    case name = "Name"
    case level = "Level"
    case kind = "Kind"
    case recent = "Recently added"

    public var id: String { rawValue }
}

public struct MemoryQueryOptions {
    public var searchText: String = ""
    /// Only memories with an aspect in this principle.
    public var principleID: Int64?
    /// Together with `principleID`: aspect level in that principle ≥ minLevel.
    /// Alone: the memory's highest aspect ≥ minLevel.
    public var minLevel: Int?
    public var sort: MemorySort = .name

    public init() {}
}

public enum MemoryFiltering {

    public static func apply(
        _ memories: [Memory],
        options: MemoryQueryOptions,
        principleNames: [Int64: String] = [:]
    ) -> [Memory] {
        var result = memories

        // Search across everything a player knows about the memory.
        let query = options.searchText.trimmingCharacters(in: .whitespacesAndNewlines)
        if !query.isEmpty {
            let needle = query.lowercased()
            result = result.filter { memory in
                let aspectsText = memory.aspects
                    .map { (principleNames[$0.principleID] ?? "?") + " \($0.level)" }
                    .joined(separator: " ")
                let haystack = [memory.name, memory.kind.rawValue, memory.notes, aspectsText]
                    .compactMap { $0 }
                    .joined(separator: " ")
                    .lowercased()
                return haystack.contains(needle)
            }
        }

        // Principle / level filters.
        if let principleID = options.principleID {
            let minLevel = options.minLevel ?? 1
            result = result.filter { memory in
                memory.aspects.contains { $0.principleID == principleID && $0.level >= minLevel }
            }
        } else if let minLevel = options.minLevel {
            result = result.filter { ($0.aspects.map(\.level).max() ?? 0) >= minLevel }
        }

        switch options.sort {
        case .name:
            result.sort { $0.name.localizedStandardCompare($1.name) == .orderedAscending }
        case .level:
            let rank: (Memory) -> Int = { memory in
                options.principleID.flatMap { principleID in
                    memory.aspects.first { $0.principleID == principleID }?.level
                } ?? memory.aspects.map(\.level).max() ?? 0
            }
            result.sort {
                let lhs = rank($0), rhs = rank($1)
                return lhs == rhs
                    ? $0.name.localizedStandardCompare($1.name) == .orderedAscending
                    : lhs > rhs
            }
        case .kind:
            result.sort {
                let lhs = kindRank($0.kind), rhs = kindRank($1.kind)
                return lhs == rhs
                    ? $0.name.localizedStandardCompare($1.name) == .orderedAscending
                    : lhs < rhs
            }
        case .recent:
            result.sort { $0.id > $1.id }
        }

        return result
    }

    /// Numina first (they're the victory items), then weather, then plain memories.
    private static func kindRank(_ kind: MemoryKind) -> Int {
        switch kind {
        case .numen: return 0
        case .weather: return 1
        case .memory: return 2
        }
    }
}