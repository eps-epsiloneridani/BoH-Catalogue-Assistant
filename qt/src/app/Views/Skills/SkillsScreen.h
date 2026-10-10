// Plan Task 15: the Skills screen — list (search, language/skill filter, sorts),
// detail with immediate level persistence and the Tree of Wisdoms commitment
// editor (Save/Revert reachable via a real dirty flag — the macOS
// remediation-1 fix), plus the add/edit dialog.
#pragma once

#include "Models.h"
#include "Stores/SkillsStore.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QWidget>

#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;
class QToolButton;
class QVBoxLayout;

namespace boh {

class SkillsScreen final : public QWidget {
    Q_OBJECT

public:
    SkillsScreen(SkillsStore* store, const std::vector<Principle>& principles,
                 QWidget* parent = nullptr);

    void addNew();
    void focusSearch();
    void setStore(SkillsStore* store);
    void selectSkill(qint64 id);

private:
    void rebuildList();
    void refreshDetail();
    void refreshMenus();
    void showError();

    SkillsStore* m_store;
    std::vector<Principle> m_principles;
    QLineEdit* m_search = nullptr;
    QToolButton* m_kindButton = nullptr;
    QToolButton* m_sortButton = nullptr;
    QListWidget* m_list = nullptr;
    QWidget* m_detailHost = nullptr;
    QVBoxLayout* m_detailLayout = nullptr;
    QWidget* m_emptyState = nullptr;
    QWidget* m_content = nullptr;
};

class SkillDetailView final : public QWidget {
    Q_OBJECT

public:
    explicit SkillDetailView(SkillsStore* store, QWidget* parent = nullptr);

    void showSkill(const Skill& skill);
    void clearSkill();

signals:
    void editRequested(const boh::Skill& skill);
    void deleteRequested(const boh::Skill& skill);

private:
    void rebuild(const Skill& skill);
    void saveNote();

    SkillsStore* m_store;
    QVBoxLayout* m_layout = nullptr;
    std::optional<Skill> m_skill;
    QPlainTextEdit* m_notesEditor = nullptr;
    bool m_notesDirty = false;
};

class SkillFormView final : public QDialog {
    Q_OBJECT

public:
    SkillFormView(const std::vector<Principle>& principles, QWidget* parent = nullptr);
    SkillFormView(const Skill& skill, const std::vector<Principle>& principles,
                  QWidget* parent = nullptr);

    SkillDraft draft() const;

private:
    void build();

private:
    std::vector<Principle> m_principles;
    std::optional<Skill> m_original;
    QLineEdit* m_name = nullptr;
    QCheckBox* m_isLanguage = nullptr;
    QComboBox* m_primary = nullptr;
    QComboBox* m_secondary = nullptr;
    QCheckBox* m_levelKnown = nullptr;
    QSpinBox* m_level = nullptr;
    QLineEdit* m_wisdom = nullptr;
    QLineEdit* m_element = nullptr;
    QPlainTextEdit* m_notes = nullptr;
};

} // namespace boh
