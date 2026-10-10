// Port of SkillQuery.swift — client-side list queries for the Skills screen.
#pragma once

#include "Models.h"

#include <QHash>
#include <QString>
#include <QtGlobal>

#include <optional>
#include <vector>

namespace boh {

enum class SkillSort { Name, Level, Recent };
enum class SkillKindFilter { All, Languages, Skills };

struct SkillQueryOptions {
    QString searchText;
    /// Skills with this principle as primary OR secondary.
    std::optional<qint64> principleID;
    SkillKindFilter kindFilter = SkillKindFilter::All;
    SkillSort sort = SkillSort::Name;
};

class SkillFiltering {
public:
    static std::vector<Skill> apply(std::vector<Skill> skills, const SkillQueryOptions& options,
                                    const QHash<qint64, QString>& principleNames = {});
};

/// The desk math for a single skill: a level-L card carries L+1 of its primary
/// principle and L of its secondary (docs/GAME_MECHANICS.md §Skills — "level 9
/// gives 10 of one aspect and 9 of the other").
struct SkillMath {
    struct Contributions {
        int primary;
        int secondary;
    };
    static Contributions contributions(int level) { return {level + 1, level}; }
};

} // namespace boh
