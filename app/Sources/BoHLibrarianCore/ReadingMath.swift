import Foundation

/// Plain-English summaries for the Reading Helper. Pure and tested — the wording
/// is user-facing, so changes should show up as test diffs.
public enum ReadingMath {

    /// "You need Rose 6." — or a hint about what's missing to compute candidates.
    public static func requirementLine(principleName: String?, difficulty: Int?) -> String {
        switch (principleName, difficulty) {
        case let (principle?, difficulty?):
            return "You need \(principle) \(difficulty)."
        case (nil, let difficulty?):
            return "Difficulty \(difficulty) recorded, but not its principle — "
                + "note the mystery principle on the Books screen to compute candidates."
        case (let principle?, nil):
            return "The mystery is \(principle), but its difficulty isn't recorded yet."
        case (nil, nil):
            return "Not catalogued yet — record its mystery on the Books screen."
        }
    }

    /// The best recorded reach toward the difficulty, from memories and skills
    /// alone (souls, inks, tools and helpers are deliberately not tracked yet).
    /// `memory` is the best recorded level in the principle, `skill` the best
    /// skill contribution. Returns nil when there's no difficulty to reach for.
    public static func reachLine(difficulty: Int?, memory: Int?, skill: Int?) -> String? {
        guard let difficulty else { return nil }
        let memoryPoints = memory ?? 0
        let skillPoints = skill ?? 0
        let total = memoryPoints + skillPoints

        var parts: [String] = []
        if memoryPoints > 0 { parts.append("memory \(memoryPoints)") }
        if skillPoints > 0 { parts.append("skill \(skillPoints)") }
        guard !parts.isEmpty else {
            return "Nothing recorded yet reaches for it."
        }
        let recorded = "Best recorded: " + parts.joined(separator: " + ") + " = \(total)"
        if total >= difficulty {
            return "\(recorded) — enough, before souls, inks and tools."
        }
        return "\(recorded) — \(difficulty - total) short, before souls, inks and tools."
    }
}