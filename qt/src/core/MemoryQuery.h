// Port of MemoryQuery.swift — client-side list queries for the Memories screen.
#pragma once

#include "Models.h"

#include <QHash>
#include <QString>
#include <QtGlobal>

#include <optional>
#include <vector>

namespace boh {

enum class MemorySort { Name, Level, Kind, Recent };

struct MemoryQueryOptions {
    QString searchText;
    /// Only memories with an aspect in this principle.
    std::optional<qint64> principleID;
    /// Together with `principleID`: aspect level in that principle ≥ minLevel.
    /// Alone: the memory's highest aspect ≥ minLevel.
    std::optional<int> minLevel;
    MemorySort sort = MemorySort::Name;
};

class MemoryFiltering {
public:
    static std::vector<Memory> apply(std::vector<Memory> memories, const MemoryQueryOptions& options,
                                     const QHash<qint64, QString>& principleNames = {});
};

} // namespace boh
