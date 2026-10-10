#include "SkillsStore.h"

namespace boh {

SkillsStore::SkillsStore(SQLiteDatabase& db, qint64 playthroughID, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_repo(std::make_unique<SkillRepository>(db, playthroughID))
{
    for (const Principle& p : PrincipleRepository(db).all())
        m_principlesByID.insert(p.id, p);
    reload();
}

std::vector<Skill> SkillsStore::displayed() const
{
    QHash<qint64, QString> principleNames;
    for (auto it = m_principlesByID.cbegin(); it != m_principlesByID.cend(); ++it)
        principleNames.insert(it.key(), it.value().name);
    return SkillFiltering::apply(m_skills, m_options, principleNames);
}

std::optional<Skill> SkillsStore::selectedSkill() const
{
    if (!m_selectedSkillID)
        return std::nullopt;
    for (const Skill& skill : displayed()) {
        if (skill.id == *m_selectedSkillID)
            return skill;
    }
    return std::nullopt;
}

QString SkillsStore::principleName(std::optional<qint64> id) const
{
    return id && m_principlesByID.contains(*id) ? m_principlesByID.value(*id).name : QString();
}

QString SkillsStore::principleColor(std::optional<qint64> id) const
{
    if (const auto color = (id && m_principlesByID.contains(*id))
                               ? m_principlesByID.value(*id).color
                               : std::nullopt)
        return *color;
    return QString();
}

void SkillsStore::reload()
{
    try {
        m_skills = m_repo->all();
    } catch (const std::exception& e) {
        m_lastError = QString::fromUtf8(e.what());
    }
    emit changed();
}

bool SkillsStore::perform(const QString& label, const std::function<void()>& operation)
{
    try {
        operation();
        reload();
        return true;
    } catch (const std::exception& e) {
        m_lastError = QStringLiteral("%1 failed: %2").arg(label, QString::fromUtf8(e.what()));
        return false;
    }
}

void SkillsStore::add(const SkillDraft& draft)
{
    perform(QStringLiteral("Adding skill"), [&] { m_selectedSkillID = m_repo->insert(draft).id; });
}

void SkillsStore::update(const Skill& skill)
{
    perform(QStringLiteral("Saving skill"), [&] { m_repo->update(skill); });
}

void SkillsStore::update(const Skill& original, const SkillDraft& draft)
{
    Skill skill = original;
    skill.name = draft.name;
    skill.isLanguage = draft.isLanguage;
    skill.primaryPrincipleID = draft.primaryPrincipleID;
    skill.secondaryPrincipleID = draft.secondaryPrincipleID;
    skill.level = draft.level;
    skill.wisdom = draft.wisdom;
    skill.element = draft.element;
    skill.notes = draft.notes;
    update(skill);
}

void SkillsStore::setLevel(const Skill& skill, int level)
{
    Skill edited = skill;
    edited.level = level;
    update(edited);
}

void SkillsStore::updateWisdomAndElement(const Skill& skill, std::optional<QString> wisdom,
                                         std::optional<QString> element)
{
    Skill edited = skill;
    edited.wisdom = wisdom;
    edited.element = element;
    update(edited);
}

void SkillsStore::updateNotes(const Skill& skill, const QString& notes)
{
    Skill edited = skill;
    edited.notes = notes.isEmpty() ? std::nullopt : std::optional<QString>(notes);
    update(edited);
}

void SkillsStore::remove(qint64 id)
{
    perform(QStringLiteral("Deleting skill"), [&] {
        m_repo->remove(id);
        if (m_selectedSkillID == id)
            m_selectedSkillID.reset();
    });
}

} // namespace boh
