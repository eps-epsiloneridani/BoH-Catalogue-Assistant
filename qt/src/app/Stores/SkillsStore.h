// Port of SkillsStore.swift — UI state for the Skills screen.
#pragma once

#include "Models.h"
#include "Repositories/LookupRepositories.h"
#include "Repositories/SkillRepository.h"
#include "SkillQuery.h"
#include "SQLiteDatabase.h"

#include <QHash>
#include <QObject>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace boh {

class SkillsStore final : public QObject {
    Q_OBJECT

public:
    SkillsStore(SQLiteDatabase& db, qint64 playthroughID, QObject* parent = nullptr);

    void reload();
    bool perform(const QString& label, const std::function<void()>& operation);
    QString lastError() const { return m_lastError; }

    std::vector<Skill> displayed() const;
    std::optional<Skill> selectedSkill() const;

    SkillQueryOptions& options() { return m_options; }
    std::optional<qint64> selectedSkillID() const { return m_selectedSkillID; }
    void setSelectedSkillID(qint64 id) { m_selectedSkillID = id; }
    void clearSelectedSkillID() { m_selectedSkillID.reset(); }

    QString principleName(std::optional<qint64> id) const;
    QString principleColor(std::optional<qint64> id) const;

    const std::vector<Skill>& skills() const { return m_skills; }

    void add(const SkillDraft& draft);
    void update(const Skill& skill);
    void update(const Skill& original, const SkillDraft& draft);
    void setLevel(const Skill& skill, int level);
    void updateWisdomAndElement(const Skill& skill, std::optional<QString> wisdom,
                                std::optional<QString> element);
    void updateNotes(const Skill& skill, const QString& notes);
    void remove(qint64 id);

signals:
    void changed();

private:
    SQLiteDatabase& m_db;
    std::unique_ptr<SkillRepository> m_repo;
    QHash<qint64, Principle> m_principlesByID;
    std::vector<Skill> m_skills;
    QString m_lastError;
    SkillQueryOptions m_options;
    std::optional<qint64> m_selectedSkillID;
};

} // namespace boh
