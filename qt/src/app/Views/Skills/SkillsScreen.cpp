#include "SkillsScreen.h"

#include "ColorMath.h"
#include "Shared/Badges.h"
#include "Shared/FormA11y.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

namespace boh {

namespace {
QString orDash(const QString& text) { return text.isEmpty() ? QStringLiteral("—") : text; }
} // namespace

SkillsScreen::SkillsScreen(SkillsStore* store, const std::vector<Principle>& principles,
                           QWidget* parent)
    : QWidget(parent)
    , m_store(store)
    , m_principles(principles)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_content = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(8, 8, 8, 0);

    auto* bar = new QHBoxLayout();
    m_search = new QLineEdit(m_content);
    m_search->setPlaceholderText(QStringLiteral("Search name, wisdom, notes…"));
    m_search->setClearButtonEnabled(true);
    m_search->setAccessibleName(QStringLiteral("Search skills"));
    bar->addWidget(m_search, 1);
    m_kindButton = new QToolButton(m_content);
    m_kindButton->setText(QStringLiteral("Kind"));
    m_kindButton->setPopupMode(QToolButton::InstantPopup);
    m_kindButton->setAccessibleName(QStringLiteral("Filter languages and skills"));
    bar->addWidget(m_kindButton);
    m_sortButton = new QToolButton(m_content);
    m_sortButton->setText(QStringLiteral("Sort"));
    m_sortButton->setPopupMode(QToolButton::InstantPopup);
    m_sortButton->setAccessibleName(QStringLiteral("Sort skills"));
    bar->addWidget(m_sortButton);
    auto* add = new QPushButton(QStringLiteral("Add Skill"), m_content);
    bar->addWidget(add);
    contentLayout->addLayout(bar);

    auto* splitter = new QSplitter(m_content);
    m_list = new QListWidget(splitter);
    splitter->addWidget(m_list);
    m_detailHost = new QWidget(splitter);
    m_detailLayout = new QVBoxLayout(m_detailHost);
    m_detailLayout->setContentsMargins(0, 0, 0, 0);
    splitter->addWidget(m_detailHost);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    contentLayout->addWidget(splitter, 1);

    m_emptyState = new QWidget(this);
    auto* emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->addStretch(1);
    auto* headline = new QLabel(QStringLiteral("No skills recorded yet"), m_emptyState);
    headline->setAlignment(Qt::AlignCenter);
    QFont headlineFont = headline->font();
    headlineFont.setPointSizeF(headlineFont.pointSizeF() * 1.4);
    headlineFont.setBold(true);
    headline->setFont(headlineFont);
    emptyLayout->addWidget(headline);
    auto* hint = new QLabel(QStringLiteral("Skills arrive when you master books that teach them."),
                            m_emptyState);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    emptyLayout->addWidget(hint);
    emptyLayout->addStretch(2);

    layout->addWidget(m_content);
    layout->addWidget(m_emptyState);

    auto* detail = new SkillDetailView(m_store, m_detailHost);
    m_detailLayout->addWidget(detail);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_store->options().searchText = text;
        rebuildList();
    });
    connect(m_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* current, QListWidgetItem*) {
                if (current)
                    m_store->setSelectedSkillID(current->data(Qt::UserRole).toLongLong());
                refreshDetail();
            });
    connect(add, &QPushButton::clicked, this, [this] { addNew(); });
    connect(detail, &SkillDetailView::editRequested, this, [this](const Skill& skill) {
        SkillFormView dialog(skill, m_principles, this);
        if (dialog.exec() == QDialog::Accepted)
            m_store->update(skill, dialog.draft());
        if (!m_store->lastError().isEmpty())
            showError();
    });
    connect(detail, &SkillDetailView::deleteRequested, this, [this](const Skill& skill) {
        m_store->remove(skill.id);
        if (!m_store->lastError().isEmpty())
            showError();
    });
    connect(m_store, &SkillsStore::changed, this, [this] {
        rebuildList();
        refreshDetail();
    });

    refreshMenus();
    rebuildList();
    refreshDetail();
}

void SkillsScreen::setStore(SkillsStore* store)
{
    if (store == m_store)
        return;
    m_store = store;
    connect(m_store, &SkillsStore::changed, this, [this] {
        rebuildList();
        refreshDetail();
    });
    rebuildList();
    refreshDetail();
}

void SkillsScreen::selectSkill(qint64 id)
{
    m_store->setSelectedSkillID(id);
    rebuildList();
    refreshDetail();
}

void SkillsScreen::refreshMenus()
{
    auto* kindMenu = new QMenu(m_kindButton);
    static const std::vector<std::pair<SkillKindFilter, QString>> kinds = {
        {SkillKindFilter::All, QStringLiteral("All")},
        {SkillKindFilter::Languages, QStringLiteral("Languages")},
        {SkillKindFilter::Skills, QStringLiteral("Skills")},
    };
    for (const auto& [kind, label] : kinds) {
        QAction* action = kindMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_store->options().kindFilter == kind);
        connect(action, &QAction::triggered, this, [this, kind] {
            m_store->options().kindFilter = kind;
            refreshMenus();
            rebuildList();
        });
    }
    m_kindButton->setMenu(kindMenu);

    auto* sortMenu = new QMenu(m_sortButton);
    static const std::vector<std::pair<SkillSort, QString>> sorts = {
        {SkillSort::Name, QStringLiteral("Name")},
        {SkillSort::Level, QStringLiteral("Level")},
        {SkillSort::Recent, QStringLiteral("Recently added")},
    };
    for (const auto& [sort, label] : sorts) {
        QAction* action = sortMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_store->options().sort == sort);
        connect(action, &QAction::triggered, this, [this, sort] {
            m_store->options().sort = sort;
            refreshMenus();
            rebuildList();
        });
    }
    m_sortButton->setMenu(sortMenu);
}

void SkillsScreen::rebuildList()
{
    const qint64 previous = m_store->selectedSkillID().value_or(-1);
    QSignalBlocker blocker(m_list);
    m_list->clear();
    for (const Skill& skill : m_store->displayed()) {
        auto* item = new QListWidgetItem(m_list);
        QStringList parts;
        parts << skill.name << (skill.isLanguage ? QStringLiteral("language") : QStringLiteral("skill"));
        if (skill.level)
            parts << QStringLiteral("level %1").arg(*skill.level);
        item->setText(parts.join(QStringLiteral(" · ")));
        item->setData(Qt::UserRole, skill.id);
        item->setData(Qt::AccessibleTextRole,
                      QStringLiteral("%1, %2%3.")
                          .arg(skill.name, skill.isLanguage ? QStringLiteral("language")
                                                            : QStringLiteral("skill"),
                               skill.level ? QStringLiteral(", level %1").arg(*skill.level)
                                           : QString()));
        if (skill.id == previous)
            m_list->setCurrentItem(item);
    }
    m_content->setVisible(!m_store->skills().empty());
    m_emptyState->setVisible(m_store->skills().empty());
}

void SkillsScreen::refreshDetail()
{
    QLayoutItem* child;
    while ((child = m_detailLayout->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }
    if (const auto skill = m_store->selectedSkill()) {
        auto* detail = new SkillDetailView(m_store, m_detailHost);
        connect(detail, &SkillDetailView::editRequested, this, [this](const Skill& skill) {
            SkillFormView dialog(skill, m_principles, this);
            if (dialog.exec() == QDialog::Accepted)
                m_store->update(skill, dialog.draft());
            if (!m_store->lastError().isEmpty())
                showError();
        });
        connect(detail, &SkillDetailView::deleteRequested, this, [this](const Skill& skill) {
            m_store->remove(skill.id);
            if (!m_store->lastError().isEmpty())
                showError();
        });
        detail->showSkill(*skill);
        m_detailLayout->addWidget(detail);
    } else if (!m_store->skills().empty()) {
        m_detailLayout->addWidget(new QLabel(QStringLiteral("Select a skill."), m_detailHost));
    }
}

void SkillsScreen::showError()
{
    QMessageBox::warning(this, QStringLiteral("Something went wrong"), m_store->lastError());
    m_store->clearError();
}

void SkillsScreen::addNew()
{
    SkillFormView dialog(m_principles, this);
    if (dialog.exec() == QDialog::Accepted)
        m_store->add(dialog.draft());
    if (!m_store->lastError().isEmpty())
        showError();
}

void SkillsScreen::focusSearch()
{
    m_search->setFocus();
    m_search->selectAll();
}

// MARK: - SkillDetailView

SkillDetailView::SkillDetailView(SkillsStore* store, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_layout = new QVBoxLayout();
    m_layout->setAlignment(Qt::AlignTop);
    outer->addLayout(m_layout);
    clearSkill();
}

void SkillDetailView::clearSkill()
{
    QLayoutItem* child;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }
    m_skill.reset();
    m_layout->addWidget(new QLabel(QStringLiteral("Select a skill."), this));
}

void SkillDetailView::rebuild(const Skill& skill)
{
    const bool sameSkill = m_skill && m_skill->id == skill.id;
    const bool notesPristine = !m_notesDirty;
    m_skill = skill;

    QLayoutItem* child;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }

    auto* headerRow = new QHBoxLayout();
    auto* title = new QLabel(skill.name, this);
    title->setWordWrap(true);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.5);
    titleFont.setBold(true);
    title->setFont(titleFont);
    headerRow->addWidget(title, 1);
    auto* editButton = new QPushButton(QStringLiteral("Edit…"), this);
    connect(editButton, &QPushButton::clicked, this, [this] { emit editRequested(*m_skill); });
    headerRow->addWidget(editButton);
    m_layout->addLayout(headerRow);
    m_layout->addWidget(new QLabel(skill.isLanguage ? QStringLiteral("language skill")
                                                    : QStringLiteral("skill"),
                                    this));

    // Aspects as badges.
    auto* badgeRow = new QHBoxLayout();
    const bool dark = Badges::isDarkMode(this);
    for (const auto& [label, id] :
         {std::pair<const char*, std::optional<qint64>>{"primary", skill.primaryPrincipleID},
          std::pair<const char*, std::optional<qint64>>{"secondary", skill.secondaryPrincipleID}}) {
        Q_UNUSED(label);
        const QString name = m_store->principleName(id);
        if (name.isEmpty())
            continue;
        auto* badge = new QLabel(name, this);
        badge->setAlignment(Qt::AlignCenter);
        const QString backdrop = ColorMath::blend(m_store->principleColor(id),
                                                  dark ? ColorMath::darkWindowBackground()
                                                       : ColorMath::lightWindowBackground(),
                                                  0.18)
                                     .value_or(QStringLiteral("#888888"));
        const QString textColor =
            ColorMath::readableTextHex(m_store->principleColor(id), dark)
                .value_or(QStringLiteral("#000000"));
        badge->setStyleSheet(
            QStringLiteral("background: %1; color: %2; border-radius: 6px; padding: 2px 8px;")
                .arg(backdrop, textColor));
        badgeRow->addWidget(badge);
    }
    badgeRow->addStretch(1);
    m_layout->addLayout(badgeRow);
    m_layout->addWidget(new QLabel(
        QStringLiteral("A level-L skill contributes L+1 to its primary principle and L to its "
                       "secondary. Level 9 maxes at 10 and 9."),
        this));

    // Level: stepper persists each change immediately.
    auto* levelRow = new QHBoxLayout();
    levelRow->addWidget(new QLabel(QStringLiteral("Level"), this));
    auto* level = new QSpinBox(this);
    level->setRange(1, 9);
    if (skill.level)
        level->setValue(*skill.level);
    else {
        level->setSpecialValueText(QStringLiteral("unknown"));
        level->setMinimum(0);
        level->setValue(0);
    }
    connect(level, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_skill || value == 0)
            return;
        m_store->setLevel(*m_skill, value);
    });
    levelRow->addWidget(level);
    levelRow->addStretch(1);
    m_layout->addLayout(levelRow);
    if (!skill.level) {
        auto* learned = new QPushButton(QStringLiteral("Learned — set level 1"), this);
        connect(learned, &QPushButton::clicked, this, [this] {
            if (m_skill)
                m_store->setLevel(*m_skill, 1);
        });
        m_layout->addWidget(learned);
    }

    // Tree of Wisdoms commitment: dirty-flag tracked via live widget edits —
    // Save/Revert are REACHABLE (the macOS remediation-1 fix ported structurally).
    auto* commitmentTitle = new QLabel(QStringLiteral("Tree of Wisdoms commitment"), this);
    commitmentTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
    m_layout->addWidget(commitmentTitle);
    auto* wisdom = new QLineEdit(skill.wisdom.value_or(QString()), this);
    wisdom->setPlaceholderText(QStringLiteral("Wisdom (once committed to the Tree)"));
    wisdom->setAccessibleName(QStringLiteral("Wisdom"));
    auto* element = new QLineEdit(skill.element.value_or(QString()), this);
    element->setPlaceholderText(QStringLiteral("Element of the Soul gained"));
    element->setAccessibleName(QStringLiteral("Element of the Soul"));
    m_layout->addWidget(wisdom);
    m_layout->addWidget(element);
    auto* commitmentButtons = new QHBoxLayout();
    auto* saveCommitment = new QPushButton(QStringLiteral("Save"), this);
    auto* revertCommitment = new QPushButton(QStringLiteral("Revert"), this);
    commitmentButtons->addWidget(saveCommitment);
    commitmentButtons->addWidget(revertCommitment);
    commitmentButtons->addStretch(1);
    auto* commitmentButtonsHost = new QWidget(this);
    commitmentButtonsHost->setLayout(commitmentButtons);
    commitmentButtonsHost->setVisible(false);
    m_layout->addWidget(commitmentButtonsHost);
    const auto markDirty = [commitmentButtonsHost] { commitmentButtonsHost->setVisible(true); };
    connect(wisdom, &QLineEdit::textChanged, this, markDirty);
    connect(element, &QLineEdit::textChanged, this, markDirty);
    connect(saveCommitment, &QPushButton::clicked, this, [this, wisdom, element, commitmentButtonsHost] {
        if (!m_skill)
            return;
        m_store->updateWisdomAndElement(*m_skill,
                                        wisdom->text().trimmed().isEmpty()
                                            ? std::nullopt
                                            : std::optional<QString>(wisdom->text().trimmed()),
                                        element->text().trimmed().isEmpty()
                                            ? std::nullopt
                                            : std::optional<QString>(element->text().trimmed()));
        commitmentButtonsHost->setVisible(false);
    });
    connect(revertCommitment, &QPushButton::clicked, this, [this, wisdom, element, commitmentButtonsHost] {
        if (!m_skill)
            return;
        wisdom->setText(m_skill->wisdom.value_or(QString()));
        element->setText(m_skill->element.value_or(QString()));
        commitmentButtonsHost->setVisible(false);
    });

    // Notes: explicit save flow, guarded re-seed.
    auto* notesTitle = new QLabel(QStringLiteral("Notes"), this);
    notesTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
    m_layout->addWidget(notesTitle);
    m_notesEditor = new QPlainTextEdit(this);
    m_notesEditor->setAccessibleName(QStringLiteral("Skill notes"));
    if (!sameSkill || notesPristine) {
        const QString noteText = skill.notes.value_or(QString());
        if (m_notesEditor->toPlainText() != noteText)
            m_notesEditor->setPlainText(noteText);
    }
    m_notesDirty = false;
    m_notesEditor->setMinimumHeight(56);
    connect(m_notesEditor, &QPlainTextEdit::textChanged, this, [this] { m_notesDirty = true; });
    m_layout->addWidget(m_notesEditor);
    auto* notesRow = new QHBoxLayout();
    auto* save = new QPushButton(QStringLiteral("Save note"), this);
    connect(save, &QPushButton::clicked, this, [this] { saveNote(); });
    auto* revert = new QPushButton(QStringLiteral("Revert"), this);
    connect(revert, &QPushButton::clicked, this, [this] {
        if (!m_skill)
            return;
        m_notesEditor->setPlainText(m_skill->notes.value_or(QString()));
        m_notesDirty = false;
    });
    notesRow->addWidget(save);
    notesRow->addWidget(revert);
    notesRow->addStretch(1);
    m_layout->addLayout(notesRow);

    auto* deleteRow = new QHBoxLayout();
    auto* deleteButton = new QPushButton(QStringLiteral("Delete Skill…"), this);
    deleteButton->setStyleSheet(QStringLiteral("color: #C0392B;"));
    connect(deleteButton, &QPushButton::clicked, this, [this] {
        if (!m_skill)
            return;
        if (QMessageBox::question(this, QStringLiteral("Delete Skill"),
                                  QStringLiteral("Delete “%1”? Books that listed its lessons keep "
                                                 "their count but lose the named link; journal "
                                                 "entries keep their text.")
                                      .arg(m_skill->name))
            == QMessageBox::Yes)
            emit deleteRequested(*m_skill);
    });
    deleteRow->addWidget(deleteButton);
    deleteRow->addStretch(1);
    m_layout->addLayout(deleteRow);
}

void SkillDetailView::saveNote()
{
    if (!m_skill)
        return;
    m_store->updateNotes(*m_skill, m_notesEditor->toPlainText());
    m_notesDirty = false;
}

void SkillDetailView::showSkill(const Skill& skill)
{
    rebuild(skill);
}

// MARK: - SkillFormView

SkillFormView::SkillFormView(const std::vector<Principle>& principles, QWidget* parent)
    : QDialog(parent)
    , m_principles(principles)
{
    setWindowTitle(QStringLiteral("Add Skill"));
    build();
}

SkillFormView::SkillFormView(const Skill& skill, const std::vector<Principle>& principles,
                             QWidget* parent)
    : QDialog(parent)
    , m_principles(principles)
    , m_original(skill)
{
    setWindowTitle(QStringLiteral("Edit Skill"));
    build();
}

void SkillFormView::build()
{
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    m_name = new QLineEdit(this);
    form->addRow(QStringLiteral("Name"), m_name);
    m_isLanguage = new QCheckBox(QStringLiteral("Language skill"), this);
    form->addRow(QString(), m_isLanguage);

    m_primary = new QComboBox(this);
    m_secondary = new QComboBox(this);
    for (QComboBox* combo : {m_primary, m_secondary}) {
        combo->addItem(QStringLiteral("—"), 0);
        for (const Principle& principle : m_principles)
            combo->addItem(principle.name, qint64(principle.id));
    }
    form->addRow(QStringLiteral("Primary (contributes level+1)"), m_primary);
    form->addRow(QStringLiteral("Secondary (contributes level)"), m_secondary);

    m_levelKnown = new QCheckBox(QStringLiteral("Level known"), this);
    form->addRow(QString(), m_levelKnown);
    m_level = new QSpinBox(this);
    m_level->setRange(1, 9);
    m_level->setValue(1);
    m_level->setEnabled(false);
    form->addRow(QStringLiteral("Level"), m_level);
    connect(m_levelKnown, &QCheckBox::toggled, m_level, &QSpinBox::setEnabled);
    m_wisdom = new QLineEdit(this);
    m_wisdom->setPlaceholderText(QStringLiteral("Wisdom (once committed to the Tree)"));
    form->addRow(QStringLiteral("Wisdom"), m_wisdom);
    m_element = new QLineEdit(this);
    m_element->setPlaceholderText(QStringLiteral("Element of the Soul gained"));
    form->addRow(QStringLiteral("Element"), m_element);
    attachFormBuddies(form);
    layout->addLayout(form);

    layout->addWidget(new QLabel(QStringLiteral("Notes"), this));
    m_notes = new QPlainTextEdit(this);
    m_notes->setMinimumHeight(48);
    layout->addWidget(m_notes);

    auto* buttons = new QHBoxLayout();
    auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* save = new QPushButton(QStringLiteral("Save"), this);
    save->setDefault(true);
    connect(save, &QPushButton::clicked, this, [this] {
        if (m_name->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Skill"),
                                 QStringLiteral("A name is the one required field."));
            return;
        }
        accept();
    });
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    layout->addLayout(buttons);

    if (m_original) {
        const Skill& skill = *m_original;
        m_name->setText(skill.name);
        m_isLanguage->setChecked(skill.isLanguage);
        m_primary->setCurrentIndex(skill.primaryPrincipleID
                                       ? m_primary->findData(qint64(*skill.primaryPrincipleID))
                                       : 0);
        m_secondary->setCurrentIndex(skill.secondaryPrincipleID
                                         ? m_secondary->findData(qint64(*skill.secondaryPrincipleID))
                                         : 0);
        m_levelKnown->setChecked(skill.level.has_value());
        m_level->setEnabled(skill.level.has_value());
        m_level->setValue(skill.level.value_or(1));
        m_wisdom->setText(skill.wisdom.value_or(QString()));
        m_element->setText(skill.element.value_or(QString()));
        m_notes->setPlainText(skill.notes.value_or(QString()));
    }
}

SkillDraft SkillFormView::draft() const
{
    SkillDraft draft;
    draft.name = m_name->text().trimmed();
    draft.isLanguage = m_isLanguage->isChecked();
    if (m_primary->currentData().toInt() != 0)
        draft.primaryPrincipleID = m_primary->currentData().toLongLong();
    if (m_secondary->currentData().toInt() != 0)
        draft.secondaryPrincipleID = m_secondary->currentData().toLongLong();
    if (m_levelKnown->isChecked())
        draft.level = m_level->value();
    if (!m_wisdom->text().trimmed().isEmpty())
        draft.wisdom = m_wisdom->text().trimmed();
    if (!m_element->text().trimmed().isEmpty())
        draft.element = m_element->text().trimmed();
    if (!m_notes->toPlainText().trimmed().isEmpty())
        draft.notes = m_notes->toPlainText().trimmed();
    return draft;
}

} // namespace boh
