#include "SkillQuery.h"

#include <QCollator>
#include <QStringList>

#include <algorithm>

namespace boh {

namespace {
bool titleLess(const QString& a, const QString& b)
{
    static const QCollator collator = [] {
        QCollator c;
        c.setNumericMode(true);
        return c;
    }();
    return collator.compare(a, b) < 0;
}
} // namespace

std::vector<Skill> SkillFiltering::apply(std::vector<Skill> skills, const SkillQueryOptions& options,
                                         const QHash<qint64, QString>& principleNames)
{
    std::vector<Skill>& result = skills;
    Q_UNUSED(principleNames);

    const QString query = options.searchText.trimmed();
    if (!query.isEmpty()) {
        const QString needle = query.toLower();
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [&](const Skill& skill) {
                                        QStringList parts;
                                        parts << skill.name << skill.wisdom.value_or(QString())
                                              << skill.element.value_or(QString())
                                              << skill.notes.value_or(QString());
                                        return !parts.join(QLatin1Char(' ')).toLower().contains(needle);
                                    }),
                     result.end());
    }

    if (options.principleID) {
        const qint64 principleID = *options.principleID;
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [&](const Skill& skill) {
                                        return skill.primaryPrincipleID != principleID
                                               && skill.secondaryPrincipleID != principleID;
                                    }),
                     result.end());
    }

    switch (options.kindFilter) {
    case SkillKindFilter::All:
        break;
    case SkillKindFilter::Languages:
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [](const Skill& s) { return !s.isLanguage; }),
                     result.end());
        break;
    case SkillKindFilter::Skills:
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [](const Skill& s) { return s.isLanguage; }),
                     result.end());
        break;
    }

    switch (options.sort) {
    case SkillSort::Name:
        std::stable_sort(result.begin(), result.end(),
                         [](const Skill& a, const Skill& b) { return titleLess(a.name, b.name); });
        break;
    case SkillSort::Level:
        std::stable_sort(result.begin(), result.end(), [](const Skill& a, const Skill& b) {
            const int ra = a.level.value_or(-1);
            const int rb = b.level.value_or(-1);
            return ra != rb ? ra > rb : titleLess(a.name, b.name);
        });
        break;
    case SkillSort::Recent:
        std::stable_sort(result.begin(), result.end(),
                         [](const Skill& a, const Skill& b) { return a.id > b.id; });
        break;
    }

    return result;
}

} // namespace boh
