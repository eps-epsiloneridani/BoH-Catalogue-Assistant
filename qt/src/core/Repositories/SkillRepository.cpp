#include "SkillRepository.h"

namespace boh {

std::vector<Skill> SkillRepository::all() const
{
    return m_db.query<Skill>(QStringLiteral("SELECT * FROM Skills WHERE playthrough_id = ? ORDER BY name;"),
                             {SQLiteValue(m_playthroughID)},
                             [](const Row& row) { return mapRow(row); });
}

std::optional<Skill> SkillRepository::get(qint64 id) const
{
    const auto rows = m_db.query<Skill>(
        QStringLiteral("SELECT * FROM Skills WHERE playthrough_id = ? AND id = ?;"),
        {SQLiteValue(m_playthroughID), SQLiteValue(id)}, [](const Row& row) { return mapRow(row); });
    return rows.empty() ? std::nullopt : std::optional<Skill>(rows.front());
}

Skill SkillRepository::insert(const SkillDraft& draft) const
{
    m_db.execute(QStringLiteral(
                     "INSERT INTO Skills (name, is_language, primary_principle_id,"
                     "                    secondary_principle_id, level, wisdom, element, notes,"
                     "                    playthrough_id)"
                     " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);"),
                 {SQLiteValue(draft.name), SQLiteValue(draft.isLanguage),
                  sv(draft.primaryPrincipleID), sv(draft.secondaryPrincipleID), sv(draft.level),
                  sv(draft.wisdom), sv(draft.element), sv(draft.notes),
                  SQLiteValue(m_playthroughID)});
    return *get(m_db.lastInsertRowID());
}

void SkillRepository::update(const Skill& skill) const
{
    m_db.execute(QStringLiteral(
                     "UPDATE Skills"
                     " SET name = ?, is_language = ?, primary_principle_id = ?, secondary_principle_id = ?,"
                     "     level = ?, wisdom = ?, element = ?, notes = ?, updated_at = datetime('now')"
                     " WHERE playthrough_id = ? AND id = ?"),
                 {SQLiteValue(skill.name), SQLiteValue(skill.isLanguage),
                  sv(skill.primaryPrincipleID), sv(skill.secondaryPrincipleID), sv(skill.level),
                  sv(skill.wisdom), sv(skill.element), sv(skill.notes),
                  SQLiteValue(m_playthroughID), SQLiteValue(skill.id)});
}

void SkillRepository::remove(qint64 id) const
{
    m_db.execute(QStringLiteral("DELETE FROM Skills WHERE playthrough_id = ? AND id = ?;"),
                 {SQLiteValue(m_playthroughID), SQLiteValue(id)});
}

std::vector<SkillContribution> SkillRepository::contributions(qint64 principleID) const
{
    return m_db.query<SkillContribution>(
        QStringLiteral(
            "SELECT *,"
            "       CASE WHEN primary_principle_id = ? THEN level + 1 ELSE level END AS contributes"
            " FROM Skills"
            " WHERE playthrough_id = ? AND is_language = 0 AND level IS NOT NULL"
            "   AND (primary_principle_id = ? OR secondary_principle_id = ?)"
            " ORDER BY contributes DESC, name;"),
        {SQLiteValue(principleID), SQLiteValue(m_playthroughID), SQLiteValue(principleID),
         SQLiteValue(principleID)},
        [](const Row& row) {
            Skill skill = mapRow(row);
            // SELECT * plus the computed column: keep the real level.
            skill.level = row.integer(QStringLiteral("level"));
            return SkillContribution{skill, row.requireInteger(QStringLiteral("contributes"))};
        });
}

Skill SkillRepository::mapRow(const Row& row)
{
    Skill s;
    s.id = row.requireInt64(QStringLiteral("id"));
    s.name = row.requireString(QStringLiteral("name"));
    s.isLanguage = row.boolean(QStringLiteral("is_language"));
    s.primaryPrincipleID = row.int64(QStringLiteral("primary_principle_id"));
    s.secondaryPrincipleID = row.int64(QStringLiteral("secondary_principle_id"));
    s.level = row.integer(QStringLiteral("level"));
    s.wisdom = row.string(QStringLiteral("wisdom"));
    s.element = row.string(QStringLiteral("element"));
    s.notes = row.string(QStringLiteral("notes"));
    return s;
}

} // namespace boh
