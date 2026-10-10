#include "AspectEditor.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace boh {

namespace {
constexpr int kNoPrinciple = 0;
}

AspectEditor::AspectEditor(const std::vector<Principle>& principles, QWidget* parent)
    : QWidget(parent)
    , m_principles(principles)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_rows = new QVBoxLayout();
    m_rows->setSpacing(4);
    layout->addLayout(m_rows);
    auto* add = new QPushButton(QStringLiteral("Add aspect"), this);
    add->setAccessibleName(QStringLiteral("Add aspect"));
    connect(add, &QPushButton::clicked, this, [this] { addRow(); });
    layout->addWidget(add);
    addRow(); // one empty row to start (Swift's initial AspectDraftRow())
}

void AspectEditor::addRow()
{
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* principle = new QComboBox(row);
    principle->setAccessibleName(QStringLiteral("Aspect principle"));
    principle->addItem(QStringLiteral("—"), kNoPrinciple);
    for (const Principle& p : m_principles)
        principle->addItem(p.name, qint64(p.id));
    layout->addWidget(principle, 1);

    auto* level = new QSpinBox(row);
    level->setRange(1, 25);
    level->setValue(1);
    level->setAccessibleName(QStringLiteral("Aspect level"));
    layout->addWidget(level);

    auto* remove = new QPushButton(QStringLiteral("✕"), row);
    remove->setAccessibleName(QStringLiteral("Remove aspect"));
    remove->setToolTip(QStringLiteral("Remove aspect"));
    remove->setFixedWidth(32);
    connect(remove, &QPushButton::clicked, this, [this, row] { removeRow(row); });
    layout->addWidget(remove);

    m_rows->addWidget(row);
    row->setProperty("principleCombo", QVariant::fromValue(principle));
    row->setProperty("levelSpin", QVariant::fromValue(level));
}

void AspectEditor::removeRow(QWidget* row)
{
    m_rows->removeWidget(row);
    row->deleteLater();
    if (m_rows->count() == 0)
        addRow();
}

void AspectEditor::setAspects(const std::vector<AspectDraft>& aspects)
{
    QLayoutItem* child;
    while ((child = m_rows->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }
    for (const AspectDraft& aspect : aspects) {
        addRow();
        auto* row = m_rows->itemAt(m_rows->count() - 1)->widget();
        auto* principle = row->property("principleCombo").value<QComboBox*>();
        auto* level = row->property("levelSpin").value<QSpinBox*>();
        if (!principle || !level)
            continue;
        principle->setCurrentIndex(principle->findData(aspect.principleID));
        level->setValue(aspect.level);
    }
    if (m_rows->count() == 0)
        addRow();
}

std::vector<AspectDraft> AspectEditor::aspects() const
{
    std::vector<AspectDraft> out;
    for (int i = 0; i < m_rows->count(); ++i) {
        auto* row = m_rows->itemAt(i)->widget();
        if (!row)
            continue;
        auto* principle = row->property("principleCombo").value<QComboBox*>();
        auto* level = row->property("levelSpin").value<QSpinBox*>();
        if (!principle || !level)
            continue;
        const qint64 id = principle->currentData().toLongLong();
        if (id != kNoPrinciple)
            out.push_back(AspectDraft{id, level->value()});
    }
    return out;
}

} // namespace boh
