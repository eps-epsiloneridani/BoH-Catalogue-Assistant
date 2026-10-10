#include "MemoryQuery.h"

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

/// Numina first (they're the victory items), then weather, then plain memories.
int kindRank(MemoryKind kind)
{
    switch (kind) {
    case MemoryKind::Numen: return 0;
    case MemoryKind::Weather: return 1;
    case MemoryKind::Memory: return 2;
    }
    return 3;
}

int levelRank(const Memory& memory, const std::optional<qint64>& principleID)
{
    if (principleID) {
        for (const Aspect& aspect : memory.aspects) {
            if (aspect.principleID == *principleID)
                return aspect.level;
        }
    }
    int highest = 0;
    for (const Aspect& aspect : memory.aspects)
        highest = std::max(highest, aspect.level);
    return highest;
}
} // namespace

std::vector<Memory> MemoryFiltering::apply(std::vector<Memory> memories,
                                           const MemoryQueryOptions& options,
                                           const QHash<qint64, QString>& principleNames)
{
    std::vector<Memory>& result = memories;

    // Search across everything a player knows about the memory.
    const QString query = options.searchText.trimmed();
    if (!query.isEmpty()) {
        const QString needle = query.toLower();
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [&](const Memory& memory) {
                                        QStringList aspectParts;
                                        for (const Aspect& aspect : memory.aspects)
                                            aspectParts
                                                << principleNames.value(aspect.principleID,
                                                                        QStringLiteral("?"))
                                                << QString::number(aspect.level);
                                        QStringList parts;
                                        parts << memory.name << memoryKindToString(memory.kind)
                                              << memory.notes.value_or(QString()) << aspectParts;
                                        return !parts.join(QLatin1Char(' ')).toLower().contains(needle);
                                    }),
                    result.end());
    }

    // Principle / level filters.
    if (options.principleID) {
        const int minLevel = options.minLevel.value_or(1);
        const qint64 principleID = *options.principleID;
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [&](const Memory& memory) {
                                        for (const Aspect& aspect : memory.aspects) {
                                            if (aspect.principleID == principleID
                                                && aspect.level >= minLevel)
                                                return false;
                                        }
                                        return true;
                                    }),
                     result.end());
    } else if (options.minLevel) {
        const int minLevel = *options.minLevel;
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [&](const Memory& memory) {
                                        int highest = 0;
                                        for (const Aspect& aspect : memory.aspects)
                                            highest = std::max(highest, aspect.level);
                                        return highest < minLevel;
                                    }),
                     result.end());
    }

    switch (options.sort) {
    case MemorySort::Name:
        std::stable_sort(result.begin(), result.end(),
                         [](const Memory& a, const Memory& b) { return titleLess(a.name, b.name); });
        break;
    case MemorySort::Level: {
        const std::optional<qint64> principleID = options.principleID;
        std::stable_sort(result.begin(), result.end(),
                         [&](const Memory& a, const Memory& b) {
                             const int ra = levelRank(a, principleID);
                             const int rb = levelRank(b, principleID);
                             return ra != rb ? ra > rb : titleLess(a.name, b.name);
                         });
        break;
    }
    case MemorySort::Kind:
        std::stable_sort(result.begin(), result.end(), [](const Memory& a, const Memory& b) {
            const int ra = kindRank(a.kind);
            const int rb = kindRank(b.kind);
            return ra != rb ? ra < rb : titleLess(a.name, b.name);
        });
        break;
    case MemorySort::Recent:
        std::stable_sort(result.begin(), result.end(),
                         [](const Memory& a, const Memory& b) { return a.id > b.id; });
        break;
    }

    return result;
}

} // namespace boh
