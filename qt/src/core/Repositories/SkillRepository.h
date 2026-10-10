// Port of SkillRepository.swift (playthrough-scoped, fail-closed writes).
#pragma once

#include "Models.h"
#include "SQLiteDatabase.h"

#include <vector>

namespace boh {

class SkillRepository {
public:
    SkillRepository(SQLiteDatabase& db, qint64 playthroughID)
        : m_db(db), m_playthroughID(playthroughID)
    {
    }

    std::vector<Skill> all() const;
    std::optional<Skill> get(qint64 id) const;
    Skill insert(const SkillDraft& draft) const;
    void update(const Skill& skill) const;
    void remove(qint64 id) const;

    // MARK: Reading Helper (canonical query 2)

    /// Non-language skills with the principle, and how much of it they contribute.
    /// A level-L skill contributes L+1 to its primary principle, L to its secondary.
    std::vector<SkillContribution> contributions(qint64 principleID) const;

    static Skill mapRow(const Row& row);

private:
    SQLiteDatabase& m_db;
    qint64 m_playthroughID;
};

} // namespace boh
