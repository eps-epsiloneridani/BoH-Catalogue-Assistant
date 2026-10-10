#include "ReadingMath.h"

#include <QStringList>

namespace boh {

QString ReadingMath::requirementLine(std::optional<QString> principleName,
                                     std::optional<int> difficulty)
{
    if (principleName && difficulty)
        return QStringLiteral("You need %1 %2.").arg(*principleName).arg(*difficulty);
    if (!principleName && difficulty)
        return QStringLiteral("Difficulty %1 recorded, but not its principle — "
                              "note the mystery principle on the Books screen to compute candidates.")
            .arg(*difficulty);
    if (principleName && !difficulty)
        return QStringLiteral("The mystery is %1, but its difficulty isn't recorded yet.")
            .arg(*principleName);
    return QStringLiteral("Not catalogued yet — record its mystery on the Books screen.");
}

std::optional<QString> ReadingMath::reachLine(std::optional<int> difficulty,
                                              std::optional<int> memory, std::optional<int> skill)
{
    if (!difficulty)
        return std::nullopt;
    const int memoryPoints = memory.value_or(0);
    const int skillPoints = skill.value_or(0);
    const int total = memoryPoints + skillPoints;

    QStringList parts;
    if (memoryPoints > 0)
        parts << QStringLiteral("memory %1").arg(memoryPoints);
    if (skillPoints > 0)
        parts << QStringLiteral("skill %1").arg(skillPoints);
    if (parts.isEmpty())
        return QStringLiteral("Nothing recorded yet reaches for it.");
    const QString recorded = QStringLiteral("Best recorded: ") + parts.join(QStringLiteral(" + "))
                             + QStringLiteral(" = %1").arg(total);
    if (total >= *difficulty)
        return QStringLiteral("%1 — enough, before souls, inks and tools.").arg(recorded);
    return QStringLiteral("%1 — %2 short, before souls, inks and tools.")
        .arg(recorded)
        .arg(*difficulty - total);
}

} // namespace boh
