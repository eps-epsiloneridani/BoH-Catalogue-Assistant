// Port of ReadingMath.swift — plain-English summaries for the Reading Helper.
// Pure and tested — the wording is user-facing, so changes show up as test diffs.
#pragma once

#include <QString>

#include <optional>

namespace boh {

class ReadingMath {
public:
    /// "You need Rose 6." — or a hint about what's missing to compute candidates.
    static QString requirementLine(std::optional<QString> principleName,
                                   std::optional<int> difficulty);

    /// The best recorded reach toward the difficulty, from memories and skills
    /// alone (souls, inks, tools and helpers are deliberately not tracked yet).
    /// Nullopt when there's no difficulty to reach for.
    static std::optional<QString> reachLine(std::optional<int> difficulty,
                                            std::optional<int> memory, std::optional<int> skill);
};

} // namespace boh
