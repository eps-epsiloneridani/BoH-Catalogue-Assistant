#include "JournalScreen.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <functional>
#include <vector>

namespace boh {

namespace {
struct JournalChip {
    QString title;
    bool hasAction = false;
};

JournalChip bookChip(const QString& title) { return {title, true}; }
} // namespace

JournalScreen::JournalScreen(JournalStore* store, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 0);

    m_content = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    auto* search = new QLineEdit(m_content);
    search->setPlaceholderText(QStringLiteral("Search entries…"));
    search->setClearButtonEnabled(true);
    search->setAccessibleName(QStringLiteral("Search journal"));
    layout->addWidget(search);

    // Quick capture — always present (the macOS first-entry bug, structural now).
    auto* quick = new QWidget(m_content);
    auto* quickLayout = new QHBoxLayout(quick);
    quickLayout->setContentsMargins(0, 0, 0, 0);
    m_quickDay = new QLineEdit(quick);
    m_quickDay->setPlaceholderText(QStringLiteral("In-game day"));
    m_quickDay->setMaximumWidth(180);
    m_quickDay->setAccessibleName(QStringLiteral("Current in-game day"));
    m_quickAdd = new QLineEdit(quick);
    m_quickAdd->setPlaceholderText(QStringLiteral("Note today's finding…"));
    m_quickAdd->setAccessibleName(QStringLiteral("Quick journal entry"));
    auto* addNote = new QPushButton(QStringLiteral("Add Note"), quick);
    quickLayout->addWidget(m_quickDay);
    quickLayout->addWidget(m_quickAdd, 1);
    quickLayout->addWidget(addNote);
    layout->addWidget(quick);

    auto* scroll = new QScrollArea(m_content);
    scroll->setWidgetResizable(true);
    m_rowsHost = new QWidget(scroll);
    m_rowsLayout = new QVBoxLayout(m_rowsHost);
    m_rowsLayout->setAlignment(Qt::AlignTop);
    scroll->setWidget(m_rowsHost);
    layout->addWidget(scroll, 1);

    // Empty state keeps the quick capture usable.
    m_emptyState = new QWidget(this);
    auto* emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->addStretch(1);
    auto* headline = new QLabel(QStringLiteral("The journal is empty"), m_emptyState);
    headline->setAlignment(Qt::AlignCenter);
    QFont headlineFont = headline->font();
    headlineFont.setPointSizeF(headlineFont.pointSizeF() * 1.4);
    headlineFont.setBold(true);
    headline->setFont(headlineFont);
    emptyLayout->addWidget(headline);
    auto* hint = new QLabel(QStringLiteral("Note today's finding above — the first entry lands "
                                           "here."),
                            m_emptyState);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    emptyLayout->addWidget(hint);
    emptyLayout->addStretch(2);

    contentLayout->addWidget(scroll, 1);
    layout->addWidget(m_content, 1);
    layout->addWidget(m_emptyState);

    connect(search, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_store->searchText = text;
        rebuildRows();
    });
    connect(addNote, &QPushButton::clicked, this, [this] { submitQuickAdd(); });
    connect(m_quickAdd, &QLineEdit::returnPressed, this, [this] { submitQuickAdd(); });
    connect(m_store, &JournalStore::changed, this, [this] {
        rebuildRows();
        if (m_store->requestFocus) {
            m_store->requestFocus = false;
            focusQuickAdd();
        }
    });

    rebuildRows();
}

void JournalScreen::focusQuickAdd()
{
    m_quickAdd->setFocus();
    m_quickAdd->selectAll();
}

void JournalScreen::setStore(JournalStore* store)
{
    if (store == m_store)
        return;
    m_store = store;
    connect(m_store, &JournalStore::changed, this, [this] {
        rebuildRows();
        if (m_store->requestFocus) {
            m_store->requestFocus = false;
            focusQuickAdd();
        }
    });
    rebuildRows();
}

void JournalScreen::submitQuickAdd()
{
    const QString day = m_quickDay->text().trimmed();
    m_store->quickAdd(m_quickAdd->text(), day.isEmpty() ? std::nullopt
                                                        : std::optional<QString>(day));
    m_quickAdd->clear();
    if (!m_store->lastError().isEmpty())
        showError();
}

void JournalScreen::rebuildRows()
{
    QLayoutItem* child;
    while ((child = m_rowsLayout->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }

    for (const auto& row : m_store->rows()) {
        if (row.header) {
            auto* header = new QLabel((*row.header).toUpper(), m_rowsHost);
            header->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
            m_rowsLayout->addWidget(header);
        }
        auto* card = new QWidget(m_rowsHost);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(8, 6, 8, 6);
        auto* text = new QLabel(row.entry.entry.toHtmlEscaped(), card);
        text->setWordWrap(true);
        text->setTextFormat(Qt::RichText);
        cardLayout->addWidget(text);
        if (row.entry.gameDay) {
            auto* day = new QLabel(*row.entry.gameDay, card);
            day->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 11px;"));
            cardLayout->addWidget(day);
        }

        // Entity-link chips: book/skill always navigate; a masked memory chip
        // (unrevealed placeholder) carries no action.
        struct Chip {
            QString title;
            bool hasAction = false;
            qint64 id = 0;
            int kind = 0; // 1 book, 2 memory, 3 skill
        };
        std::vector<Chip> chips;
        if (row.entry.bookID) {
            const QString title = m_store->bookTitle(row.entry.bookID);
            if (!title.isEmpty())
                chips.push_back({title, true, *row.entry.bookID, 1});
        }
        if (row.entry.memoryID) {
            const QString title = m_store->memoryName(row.entry.memoryID);
            if (!title.isEmpty())
                chips.push_back({title, !title.startsWith(QStringLiteral("unrevealed")),
                                 *row.entry.memoryID, 2});
        }
        if (row.entry.skillID) {
            const QString title = m_store->skillName(row.entry.skillID);
            if (!title.isEmpty())
                chips.push_back({title, true, *row.entry.skillID, 3});
        }
        if (!chips.empty()) {
            auto* chipRow = new QHBoxLayout();
            for (const Chip& chip : chips) {
                auto* button = new QPushButton(chip.title, card);
                button->setFlat(true);
                button->setStyleSheet(QStringLiteral("text-decoration: underline;"));
                button->setAccessibleName(QStringLiteral("Open %1").arg(chip.title));
                if (chip.hasAction) {
                    const qint64 id = chip.id;
                    const int kind = chip.kind;
                    connect(button, &QPushButton::clicked, this, [this, id, kind] {
                        switch (kind) {
                        case 1: emit showBookRequested(id); break;
                        case 2: emit showMemoryRequested(id); break;
                        case 3: emit showSkillRequested(id); break;
                        }
                    });
                } else {
                    button->setEnabled(false);
                    button->setToolTip(QStringLiteral("Unrevealed — earn this memory first"));
                }
                chipRow->addWidget(button);
            }
            chipRow->addStretch(1);
            cardLayout->addLayout(chipRow);
        }

        auto* buttonRow = new QHBoxLayout();
        auto* edit = new QPushButton(QStringLiteral("Edit"), card);
        const JournalEntry entry = row.entry;
        connect(edit, &QPushButton::clicked, this, [this, entry] { editEntry(entry); });
        auto* remove = new QPushButton(QStringLiteral("Delete"), card);
        connect(remove, &QPushButton::clicked, this, [this, id = row.entry.id] {
            if (QMessageBox::question(this, QStringLiteral("Delete entry"),
                                      QStringLiteral("Delete this journal entry?"))
                == QMessageBox::Yes)
                m_store->remove(id);
        });
        buttonRow->addWidget(edit);
        buttonRow->addWidget(remove);
        buttonRow->addStretch(1);
        cardLayout->addLayout(buttonRow);
        m_rowsLayout->addWidget(card);
    }
    m_rowsLayout->addStretch(1);
    refreshEmptyState();
}

void JournalScreen::refreshEmptyState()
{
    m_emptyState->setVisible(m_store->rows().empty());
    m_rowsHost->parentWidget()->setVisible(!m_store->rows().empty());
}

void JournalScreen::editEntry(const JournalEntry& entry)
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Edit entry"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    auto* gameDay = new QLineEdit(entry.gameDay.value_or(QString()), &dialog);
    form->addRow(QStringLiteral("In-game day (optional)"), gameDay);
    auto* book = new QComboBox(&dialog);
    book->addItem(QStringLiteral("—"), 0);
    for (const BookRef& ref : m_store->bookPickerList())
        book->addItem(ref.title, qint64(ref.id));
    if (entry.bookID)
        book->setCurrentIndex(book->findData(*entry.bookID));
    form->addRow(QStringLiteral("Book"), book);
    auto* memory = new QComboBox(&dialog);
    memory->addItem(QStringLiteral("—"), 0);
    for (const Memory& memory2 : m_store->memoryPickerList(entry.memoryID))
        memory->addItem(memory2.name, qint64(memory2.id));
    if (entry.memoryID)
        memory->setCurrentIndex(memory->findData(*entry.memoryID));
    form->addRow(QStringLiteral("Memory"), memory);
    auto* skill = new QComboBox(&dialog);
    skill->addItem(QStringLiteral("—"), 0);
    for (const Skill& skill2 : m_store->skillPickerList())
        skill->addItem(skill2.name, qint64(skill2.id));
    if (entry.skillID)
        skill->setCurrentIndex(skill->findData(*entry.skillID));
    form->addRow(QStringLiteral("Skill"), skill);
    auto* entryEdit = new QLineEdit(entry.entry, &dialog);
    form->addRow(QStringLiteral("Entry"), entryEdit);
    layout->addLayout(form);
    auto* buttons = new QHBoxLayout();
    auto* cancel = new QPushButton(QStringLiteral("Cancel"), &dialog);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    auto* save = new QPushButton(QStringLiteral("Save"), &dialog);
    save->setDefault(true);
    connect(save, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    layout->addLayout(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        JournalEntry edited = entry;
        edited.gameDay = gameDay->text().trimmed().isEmpty()
                             ? std::nullopt
                             : std::optional<QString>(gameDay->text().trimmed());
        edited.bookID = book->currentData().toInt() != 0
                            ? std::optional<qint64>(book->currentData().toLongLong())
                            : std::nullopt;
        edited.memoryID = memory->currentData().toInt() != 0
                              ? std::optional<qint64>(memory->currentData().toLongLong())
                              : std::nullopt;
        edited.skillID = skill->currentData().toInt() != 0
                             ? std::optional<qint64>(skill->currentData().toLongLong())
                             : std::nullopt;
        edited.entry = entryEdit->text();
        m_store->update(edited);
        if (!m_store->lastError().isEmpty())
            showError();
    }
}

void JournalScreen::showError()
{
    QMessageBox::warning(this, QStringLiteral("Something went wrong"), m_store->lastError());
    m_store->clearError();
}

} // namespace boh
