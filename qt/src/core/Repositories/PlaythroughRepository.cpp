#include "PlaythroughRepository.h"

namespace boh {

namespace {
constexpr const char* kActiveKey = "active_playthrough";
} // namespace

std::vector<Playthrough> PlaythroughRepository::all() const
{
    return m_db.query<Playthrough>(QStringLiteral("SELECT * FROM Playthroughs ORDER BY id;"), {},
                                   [](const Row& row) { return mapRow(row); });
}

std::optional<Playthrough> PlaythroughRepository::get(qint64 id) const
{
    const auto rows = m_db.query<Playthrough>(QStringLiteral("SELECT * FROM Playthroughs WHERE id = ?;"),
                                              {SQLiteValue(id)},
                                              [](const Row& row) { return mapRow(row); });
    return rows.empty() ? std::nullopt : std::optional<Playthrough>(rows.front());
}

Playthrough PlaythroughRepository::insert(const QString& name, std::optional<QString> notes) const
{
    m_db.execute(QStringLiteral("INSERT INTO Playthroughs (name, notes) VALUES (?, ?);"),
                 {SQLiteValue(name), sv(notes)});
    return *get(m_db.lastInsertRowID());
}

void PlaythroughRepository::update(const Playthrough& playthrough) const
{
    m_db.execute(QStringLiteral("UPDATE Playthroughs SET name = ?, notes = ? WHERE id = ?;"),
                 {SQLiteValue(playthrough.name), sv(playthrough.notes), SQLiteValue(playthrough.id)});
}

void PlaythroughRepository::remove(qint64 id) const
{
    m_db.execute(QStringLiteral("DELETE FROM Playthroughs WHERE id = ?;"), {SQLiteValue(id)});
}

std::optional<qint64> PlaythroughRepository::activeID() const
{
    const auto rows = m_db.query<std::optional<QString>>(
        QStringLiteral("SELECT value FROM Meta WHERE key = ?;"), {SQLiteValue(kActiveKey)},
        [](const Row& row) { return row.string(QStringLiteral("value")); });
    if (rows.empty() || !rows.front())
        return std::nullopt;
    bool ok = false;
    const qint64 id = rows.front()->toLongLong(&ok);
    return ok ? std::optional<qint64>(id) : std::nullopt;
}

void PlaythroughRepository::setActiveID(qint64 id) const
{
    m_db.execute(QStringLiteral(
                     "INSERT INTO Meta (key, value) VALUES (?, ?)"
                     " ON CONFLICT(key) DO UPDATE SET value = excluded.value;"),
                 {SQLiteValue(kActiveKey), SQLiteValue(QString::number(id))});
}

std::optional<Playthrough> PlaythroughRepository::active() const
{
    if (const auto id = activeID()) {
        if (const auto playthrough = get(*id))
            return playthrough;
    }
    const auto allRows = all();
    return allRows.empty() ? std::nullopt : std::optional<Playthrough>(allRows.front());
}

Playthrough PlaythroughRepository::mapRow(const Row& row)
{
    Playthrough p;
    p.id = row.requireInt64(QStringLiteral("id"));
    p.name = row.requireString(QStringLiteral("name"));
    p.createdAt = row.requireString(QStringLiteral("created_at"));
    p.notes = row.string(QStringLiteral("notes"));
    return p;
}

} // namespace boh
