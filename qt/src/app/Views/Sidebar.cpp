#include "Sidebar.h"

#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>

namespace boh {

namespace {
QStringList sectionTitles()
{
    return {QStringLiteral("Books"), QStringLiteral("Memories"), QStringLiteral("Skills"),
            QStringLiteral("Journal"), QStringLiteral("Reading Helper")};
}
} // namespace

Sidebar::Sidebar(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* libraryTitle = new QLabel(QStringLiteral("Library"), this);
    libraryTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 11px;"));
    layout->addWidget(libraryTitle);

    m_playthroughs = new QComboBox(this);
    m_playthroughs->setAccessibleName(QStringLiteral("Active playthrough"));
    m_playthroughs->setToolTip(QStringLiteral("Active playthrough"));
    connect(m_playthroughs, &QComboBox::activated, this, [this](int index) {
        emit playthroughSelected(index);
    });
    layout->addWidget(m_playthroughs);

    m_sections = new QListWidget(this);
    m_sections->setAccessibleName(QStringLiteral("Sections"));
    m_sections->setFrameShape(QFrame::NoFrame);
    m_sections->setSpacing(2);
    for (const QString& title : sectionTitles())
        new QListWidgetItem(title, m_sections);
    connect(m_sections, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0)
            emit sectionSelected(row);
    });
    layout->addWidget(m_sections, 1);
    layout->addStretch(0);
}

void Sidebar::setPlaythroughs(const QStringList& names, int activeIndex)
{
    QSignalBlocker blocker(m_playthroughs);
    m_playthroughs->clear();
    m_playthroughs->addItems(names);
    if (activeIndex >= 0 && activeIndex < m_playthroughs->count())
        m_playthroughs->setCurrentIndex(activeIndex);
}

void Sidebar::setCurrentSection(int index)
{
    QSignalBlocker blocker(m_sections);
    if (index >= 0 && index < m_sections->count())
        m_sections->setCurrentRow(index);
}

} // namespace boh
