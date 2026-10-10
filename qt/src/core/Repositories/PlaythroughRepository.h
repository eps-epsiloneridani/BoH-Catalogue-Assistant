// Port of PlaythroughRepository.swift — one row per saved game, plus the Meta
// table holding which playthrough is currently loaded. Not itself scoped.
#pragma once

#include "Models.h"
#include "SQLiteDatabase.h"

namespace boh {

class PlaythroughRepository {
public:
    explicit PlaythroughRepository(SQLiteDatabase& db) : m_db(db) {}

    std::vector<Playthrough> all() const;
    std::optional<Playthrough> get(qint64 id) const;
    Playthrough insert(const QString& name, std::optional<QString> notes = {}) const;
    void update(const Playthrough& playthrough) const;
    /// Deletes the playthrough and (via ON DELETE CASCADE) every book, memory,
    /// skill and journal row scoped to it. The caller guards: never the active
    /// one, never the last one, and always confirmed by the user.
    void remove(qint64 id) const;

    std::optional<qint64> activeID() const;
    void setActiveID(qint64 id) const;
    /// The loaded playthrough; the first one if Meta is missing or stale.
    std::optional<Playthrough> active() const;

    static Playthrough mapRow(const Row& row);

private:
    SQLiteDatabase& m_db;
};

} // namespace boh
