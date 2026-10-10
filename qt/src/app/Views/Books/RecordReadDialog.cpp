#include "RecordReadDialog.h"

#include "AppController.h"
#include "Shared/AspectEditor.h"
#include "Shared/FormA11y.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace boh {

RecordReadDialog::RecordReadDialog(const Book& book, BooksStore* store, AppController* controller,
                                   QWidget* parent)
    : QDialog(parent)
    , m_book(book)
    , m_store(store)
    , m_controller(controller)
{
    setWindowTitle(QStringLiteral("Record read"));
    m_memories = m_store->allMemories();
    m_earnedMemories = m_store->earnedMemories();
    QSet<qint64> earnedIDs;
    for (const Memory& memory : m_earnedMemories)
        earnedIDs.insert(memory.id);
    // This book's unearned yield, if any — gained pickers show a placeholder.
    if (book.yieldedMemoryID && !earnedIDs.contains(*book.yieldedMemoryID))
        m_hiddenYield = *book.yieldedMemoryID;

    auto* layout = new QVBoxLayout(this);

    auto* header = new QLabel(QStringLiteral("“%1”").arg(book.title), this);
    QFont headerFont = header->font();
    headerFont.setBold(true);
    headerFont.setPointSizeF(headerFont.pointSizeF() * 1.2);
    header->setFont(headerFont);
    layout->addWidget(header);

    m_mastering = new QCheckBox(QStringLiteral("Mastering read (first complete read)"), this);
    // Mastering defaults ON (user request): the common gap-fill case — flip off
    // deliberately for a pure re-read.
    m_mastering->setChecked(true);
    m_mastering->setAccessibleName(QStringLiteral("Mastering read"));
    layout->addWidget(m_mastering);
    if (book.readStatus == ReadStatus::Mastered)
        layout->addWidget(new QLabel(
            QStringLiteral("Already mastered — recording a re-read for the memory."), this));

    auto* form = new QFormLayout();
    m_usedMemory = new QComboBox(this);
    m_usedMemory->addItem(QStringLiteral("—"), 0);
    for (const Memory& memory : m_earnedMemories)
        m_usedMemory->addItem(memoryLabel(memory), qint64(memory.id));
    form->addRow(QStringLiteral("Memory used"), m_usedMemory);
    attachFormBuddies(form);
    layout->addLayout(form);

    layout->addWidget(new QLabel(QStringLiteral("What did you gain?"), this));
    m_gainedChoice = new QComboBox(this);
    m_gainedExisting = new QComboBox(this);
    rebuildGainedChoices();
    connect(m_gainedChoice, &QComboBox::currentIndexChanged, this, [this](int index) {
        const qint64 data = m_gainedChoice->itemData(index).toLongLong();
        m_gainedExisting->setVisible(data != 0 && data != kGainedNew);
        m_newName->setVisible(data == kGainedNew);
        m_newKind->setVisible(data == kGainedNew);
        m_newPersistent->setVisible(data == kGainedNew);
        m_newAspects->setVisible(data == kGainedNew);
    });
    layout->addWidget(m_gainedChoice);
    layout->addWidget(m_gainedExisting);
    m_newName = new QLineEdit(this);
    m_newName->setPlaceholderText(QStringLiteral("Memory name"));
    layout->addWidget(m_newName);
    m_newKind = new QComboBox(this);
    for (MemoryKind kind : {MemoryKind::Memory, MemoryKind::Weather, MemoryKind::Numen})
        m_newKind->addItem(memoryKindToString(kind), int(kind));
    layout->addWidget(m_newKind);
    m_newPersistent = new QCheckBox(QStringLiteral("Persistent (survives dawn)"), this);
    layout->addWidget(m_newPersistent);
    m_newAspects = new AspectEditor(controller ? controller->principles()
                                               : std::vector<Principle>{},
                                    this);
    layout->addWidget(m_newAspects);

    auto* lessonsRow = new QHBoxLayout();
    lessonsRow->addWidget(new QLabel(QStringLiteral("Lessons granted"), this));
    m_lessons = new QSpinBox(this);
    m_lessons->setRange(0, 3);
    m_lessons->setSpecialValueText(QStringLiteral("—"));
    lessonsRow->addWidget(m_lessons);
    lessonsRow->addStretch(1);
    layout->addLayout(lessonsRow);
    auto* lessonsNote = new QLabel(
        QStringLiteral("Marks the book as mastered and records lessons, if noted. Re-reads "
                       "always give the book's memory, regardless of mystery."),
        this);
    lessonsNote->setWordWrap(true);
    lessonsNote->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 11px;"));
    layout->addWidget(lessonsNote);

    m_gameDay = new QLineEdit(this);
    m_gameDay->setPlaceholderText(QStringLiteral("In-game day (e.g. Year 1, Autumn, day 3)"));
    if (m_controller)
        m_gameDay->setText(m_controller->currentGameDay());
    layout->addWidget(m_gameDay);
    m_note = new QLineEdit(this);
    m_note->setPlaceholderText(QStringLiteral("Note (optional)"));
    layout->addWidget(m_note);

    auto* buttons = new QHBoxLayout();
    auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* recordButton = new QPushButton(QStringLiteral("Record read"), this);
    recordButton->setDefault(true);
    connect(recordButton, &QPushButton::clicked, this, [this] { record(); });
    connect(m_newName, &QLineEdit::textChanged, this,
            [this, recordButton] { recordButton->setEnabled(canRecord()); });
    connect(m_gainedChoice, &QComboBox::currentIndexChanged, this,
            [this, recordButton] { recordButton->setEnabled(canRecord()); });
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(recordButton);
    layout->addLayout(buttons);
}

void RecordReadDialog::setPreselectedMemoryID(qint64 id)
{
    if (id == 0)
        return;
    const int index = m_usedMemory->findData(id);
    if (index >= 0)
        m_usedMemory->setCurrentIndex(index);
}

void RecordReadDialog::rebuildGainedChoices()
{
    QSignalBlocker blocker(m_gainedChoice);
    m_gainedChoice->clear();
    m_gainedChoice->addItem(QStringLiteral("None / not noted"), 0);
    for (const Memory& memory : m_earnedMemories)
        m_gainedChoice->addItem(memoryLabel(memory), qint64(memory.id));
    if (m_hiddenYield)
        m_gainedChoice->addItem(QStringLiteral("Unrevealed memory — this read earns it"),
                                *m_hiddenYield);
    m_gainedChoice->addItem(QStringLiteral("New memory…"), kGainedNew);

    m_gainedExisting->clear();
    m_gainedExisting->addItem(QStringLiteral("—"), 0);
    for (const Memory& memory : m_earnedMemories)
        m_gainedExisting->addItem(memoryLabel(memory), qint64(memory.id));
    if (m_hiddenYield)
        m_gainedExisting->addItem(QStringLiteral("Unrevealed memory — this read earns it"),
                                  *m_hiddenYield);
    m_gainedExisting->setVisible(false);
    m_newName->setVisible(false);
    m_newKind->setVisible(false);
    m_newPersistent->setVisible(false);
    m_newAspects->setVisible(false);
}

QString RecordReadDialog::memoryLabel(const Memory& memory) const
{
    QString label = memory.name;
    if (!memory.aspects.empty()) {
        QStringList parts;
        for (const Aspect& aspect : memory.aspects)
            parts << QStringLiteral("%1 %2").arg(aspect.principleName).arg(aspect.level);
        label += QStringLiteral("  (%1)").arg(parts.join(QStringLiteral(", ")));
    }
    if (memory.persistent)
        label += QStringLiteral("  ∙ persistent");
    return label;
}

bool RecordReadDialog::canRecord() const
{
    if (m_gainedChoice->currentData().toLongLong() != kGainedNew)
        return true;
    return !m_newName->text().trimmed().isEmpty();
}

void RecordReadDialog::record()
{
    std::optional<Memory> gainedMemory;
    const qint64 choice = m_gainedChoice->currentData().toLongLong();
    if (choice != 0 && choice != kGainedNew) {
        const qint64 existingChoice = m_gainedExisting->isVisible()
                                          ? m_gainedExisting->currentData().toLongLong()
                                          : choice;
        for (const Memory& memory : m_memories) {
            if (memory.id == (existingChoice != 0 ? existingChoice : choice)) {
                gainedMemory = memory;
                break;
            }
        }
    } else if (choice == kGainedNew) {
        Memory created = m_store->createMemory(MemoryDraft{
            m_newName->text().trimmed(), MemoryKind(m_newKind->currentData().toInt()),
            m_newPersistent->isChecked(), {}, m_newAspects->aspects()});
        if (created.id == 0)
            return; // store.lastError set; alert shows at screen level
        gainedMemory = created;
    }

    if (m_controller)
        m_controller->setCurrentGameDay(m_gameDay->text().trimmed());

    const QString day = m_gameDay->text().trimmed();
    m_store->recordRead(m_book, m_mastering->isChecked(),
                        m_usedMemory->currentData().toLongLong() != 0
                            ? std::optional<qint64>(m_usedMemory->currentData().toLongLong())
                            : std::nullopt,
                        gainedMemory,
                        m_mastering->isChecked() && m_lessons->value() > 0
                            ? std::optional<int>(m_lessons->value())
                            : std::nullopt,
                        day.isEmpty() ? std::nullopt : std::optional<QString>(day),
                        m_note->text().trimmed().isEmpty() ? std::nullopt
                                                           : std::optional<QString>(m_note->text().trimmed()));
    accept();
}

} // namespace boh
