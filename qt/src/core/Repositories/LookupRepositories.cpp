#include "LookupRepositories.h"

namespace boh {

std::vector<Principle> PrincipleRepository::all() const
{
    return m_db.query<Principle>(QStringLiteral("SELECT * FROM Principles ORDER BY sort_order;"), {},
                                 [](const Row& row) { return mapRow(row); });
}

std::optional<Principle> PrincipleRepository::get(qint64 id) const
{
    const auto rows = m_db.query<Principle>(QStringLiteral("SELECT * FROM Principles WHERE id = ?;"),
                                            {SQLiteValue(id)},
                                            [](const Row& row) { return mapRow(row); });
    return rows.empty() ? std::nullopt : std::optional<Principle>(rows.front());
}

void PrincipleRepository::update(const Principle& principle) const
{
    m_db.execute(QStringLiteral(
                     "UPDATE Principles SET name = ?, sort_order = ?, color = ?, notes = ? WHERE id = ?;"),
                 {SQLiteValue(principle.name), SQLiteValue(principle.sortOrder), sv(principle.color),
                  sv(principle.notes), SQLiteValue(principle.id)});
}

Principle PrincipleRepository::mapRow(const Row& row)
{
    Principle p;
    p.id = row.requireInt64(QStringLiteral("id"));
    p.name = row.requireString(QStringLiteral("name"));
    p.sortOrder = row.requireInteger(QStringLiteral("sort_order"));
    p.color = row.string(QStringLiteral("color"));
    p.notes = row.string(QStringLiteral("notes"));
    return p;
}

std::vector<Language> LanguageRepository::all() const
{
    return m_db.query<Language>(QStringLiteral("SELECT * FROM Languages ORDER BY native DESC, name;"),
                                {}, [](const Row& row) { return mapRow(row); });
}

std::optional<Language> LanguageRepository::get(qint64 id) const
{
    const auto rows = m_db.query<Language>(QStringLiteral("SELECT * FROM Languages WHERE id = ?;"),
                                           {SQLiteValue(id)},
                                           [](const Row& row) { return mapRow(row); });
    return rows.empty() ? std::nullopt : std::optional<Language>(rows.front());
}

Language LanguageRepository::insert(const QString& name, bool native,
                                    std::optional<QString> notes) const
{
    m_db.execute(QStringLiteral("INSERT INTO Languages (name, native, notes) VALUES (?, ?, ?);"),
                 {SQLiteValue(name), SQLiteValue(native), sv(notes)});
    Language language;
    language.id = m_db.lastInsertRowID();
    language.name = name;
    language.native = native;
    language.notes = notes;
    return language;
}

void LanguageRepository::update(const Language& language) const
{
    m_db.execute(QStringLiteral(
                     "UPDATE Languages SET name = ?, native = ?, notes = ? WHERE id = ?;"),
                 {SQLiteValue(language.name), SQLiteValue(language.native), sv(language.notes),
                  SQLiteValue(language.id)});
}

void LanguageRepository::remove(qint64 id) const
{
    m_db.execute(QStringLiteral("DELETE FROM Languages WHERE id = ?;"), {SQLiteValue(id)});
}

Language LanguageRepository::mapRow(const Row& row)
{
    Language l;
    l.id = row.requireInt64(QStringLiteral("id"));
    l.name = row.requireString(QStringLiteral("name"));
    l.native = row.boolean(QStringLiteral("native"));
    l.notes = row.string(QStringLiteral("notes"));
    return l;
}

} // namespace boh
