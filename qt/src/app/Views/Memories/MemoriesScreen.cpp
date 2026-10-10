#include "MemoriesScreen.h"

#include "AppController.h"
#include "ColorMath.h"
#include "Shared/AspectEditor.h"
#include "Shared/Badges.h"
#include "Shared/BookRowDelegate.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

namespace boh {

namespace {
QString kindLabel(MemoryKind kind)
{
    switch (kind) {
    case MemoryKind::Memory: return QStringLiteral("memory");
    case MemoryKind::Weather: return QStringLiteral("weather");
    case MemoryKind::Numen: return QStringLiteral("numen");
    }
    return QString();
}

/// "Name:tint|Name:tint" — the row's badge list, parsed by MemoryRowDelegate.
QString encodeBadges(const Memory& memory)
{
    QStringList badges;
    for (const Aspect& aspect : memory.aspects)
        badges << aspect.principleName; // tint resolved by the view from the store
    return badges.join(QLatin1Char('|'));
}
} // namespace

// MARK: - MemoryRowDelegate (Shared)

class MemoryRowDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        painter->save();
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(option.rect, option.palette.color(QPalette::Highlight));
            painter->setPen(option.palette.color(QPalette::HighlightedText));
        } else {
            painter->setPen(option.palette.color(QPalette::WindowText));
        }
        const QRect row = option.rect.adjusted(8, 4, -8, -4);
        const bool dark = Badges::isDarkMode(option.widget);

        QFont titleFont = option.font;
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->drawText(row.adjusted(0, 2, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                          option.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                                         Qt::ElideRight, row.width()));

        const QString subtitle = index.data(kSubtitleRole).toString();
        QFont subFont = option.font;
        subFont.setPointSizeF(subFont.pointSizeF() * 0.9);
        painter->setFont(subFont);
        painter->setPen(option.state & QStyle::State_Selected
                            ? option.palette.color(QPalette::HighlightedText)
                            : option.palette.color(QPalette::Mid));
        painter->drawText(row.adjusted(0, option.fontMetrics.height() + 6, 0, 0),
                          Qt::AlignLeft | Qt::AlignTop,
                          option.fontMetrics.elidedText(subtitle, Qt::ElideRight, row.width()));

        // Aspect badges on the right, tint resolved by the view per badge name.
        QStringList badges = index.data(kBadgesRole).toString().split(QLatin1Char('|'),
                                                                      Qt::SkipEmptyParts);
        const QStringList tints = index.data(kTintsRole).toString().split(QLatin1Char('|'));
        int x = row.right();
        for (int i = badges.size() - 1; i >= 0; --i) {
            const QSize size = Badges::badgeSize(badges[i], option.fontMetrics);
            x -= size.width() + 4;
            if (x < row.left())
                break;
            Badges::paint(painter, QRect(x, row.top() + (row.height() - size.height()) / 2,
                                         size.width(), size.height()),
                          badges[i], i < tints.size() ? tints[i] : QString(), dark);
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override
    {
        return {240, option.fontMetrics.height() * 2 + 14};
    }

    static constexpr int kSubtitleRole = Qt::UserRole + 1;
    static constexpr int kBadgesRole = Qt::UserRole + 2;
    static constexpr int kTintsRole = Qt::UserRole + 3;
};

// MARK: - MemoriesScreen

MemoriesScreen::MemoriesScreen(MemoriesStore* store, const std::vector<Principle>& principles,
                               AppController* controller, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
    , m_principles(principles)
    , m_controller(controller)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_content = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(8, 8, 8, 0);

    auto* bar = new QHBoxLayout();
    m_search = new QLineEdit(m_content);
    m_search->setPlaceholderText(QStringLiteral("Search name, notes, aspects…"));
    m_search->setClearButtonEnabled(true);
    m_search->setAccessibleName(QStringLiteral("Search memories"));
    bar->addWidget(m_search, 1);

    m_principleButton = new QToolButton(m_content);
    m_principleButton->setText(QStringLiteral("Principle"));
    m_principleButton->setPopupMode(QToolButton::InstantPopup);
    m_principleButton->setAccessibleName(QStringLiteral("Filter by principle"));
    bar->addWidget(m_principleButton);

    m_levelButton = new QToolButton(m_content);
    m_levelButton->setText(QStringLiteral("Level"));
    m_levelButton->setPopupMode(QToolButton::InstantPopup);
    m_levelButton->setAccessibleName(QStringLiteral("Filter by level"));
    bar->addWidget(m_levelButton);

    m_sortButton = new QToolButton(m_content);
    m_sortButton->setText(QStringLiteral("Sort"));
    m_sortButton->setPopupMode(QToolButton::InstantPopup);
    m_sortButton->setAccessibleName(QStringLiteral("Sort memories"));
    bar->addWidget(m_sortButton);

    auto* add = new QPushButton(QStringLiteral("Add Memory"), m_content);
    bar->addWidget(add);
    contentLayout->addLayout(bar);

    auto* splitter = new QSplitter(m_content);
    m_list = new QListWidget(splitter);
    m_list->setItemDelegate(new MemoryRowDelegate(m_list));
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
    auto* headline = new QLabel(QStringLiteral("No memories known yet"), m_emptyState);
    headline->setAlignment(Qt::AlignCenter);
    QFont headlineFont = headline->font();
    headlineFont.setPointSizeF(headlineFont.pointSizeF() * 1.4);
    headlineFont.setBold(true);
    headline->setFont(headlineFont);
    emptyLayout->addWidget(headline);
    auto* hint = new QLabel(
        QStringLiteral("Memories appear as you earn them — record a read on a book, and what "
                       "it taught lands here. Imports only surface once their book is mastered."),
        m_emptyState);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    emptyLayout->addWidget(hint);
    emptyLayout->addStretch(2);

    layout->addWidget(m_content);
    layout->addWidget(m_emptyState);

    auto* detail = new MemoryDetailView(m_store, m_detailHost);
    m_detailLayout->addWidget(detail);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_store->options().searchText = text;
        rebuildList();
    });
    connect(m_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* current, QListWidgetItem*) {
                if (current)
                    m_store->setSelectedMemoryID(current->data(Qt::UserRole).toLongLong());
                refreshDetail();
            });
    connect(add, &QPushButton::clicked, this, [this] { addNewRequested(); });
    connect(detail, &MemoryDetailView::editRequested, this, [this, detail](const Memory& memory) {
        MemoryFormView dialog(memory, m_store->sources(memory.id), m_store->allYielding(memory.id),
                              m_store->booksForLinking(), m_principles, this);
        if (dialog.exec() == QDialog::Accepted) {
            m_store->update(memory, dialog.draft());
            m_store->setSources(memory.id, dialog.sources());
            m_store->setYieldingBooks(memory.id, dialog.yieldingBookIDs());
        }
        if (!m_store->lastError().isEmpty())
            showError();
    });
    connect(detail, &MemoryDetailView::deleteRequested, this, [this](const Memory& memory) {
        m_store->remove(memory.id);
        if (!m_store->lastError().isEmpty())
            showError();
    });
    connect(detail, &MemoryDetailView::showBookRequested, this,
            [this](qint64 bookID) { emit showBookRequested(bookID); });
    connect(m_store, &MemoriesStore::changed, this, [this] {
        rebuildList();
        refreshDetail();
    });

    refreshMenus();
    rebuildList();
    refreshDetail();
}

void MemoriesScreen::setStore(MemoriesStore* store)
{
    if (store == m_store)
        return;
    m_store = store;
    connect(m_store, &MemoriesStore::changed, this, [this] {
        rebuildList();
        refreshDetail();
    });
    rebuildList();
    refreshDetail();
}

void MemoriesScreen::selectMemory(qint64 id)
{
    m_store->setSelectedMemoryID(id);
    rebuildList();
    refreshDetail();
}

void MemoriesScreen::refreshMenus()
{
    auto* principleMenu = new QMenu(m_principleButton);
    QAction* any = principleMenu->addAction(QStringLiteral("Any principle"));
    any->setCheckable(true);
    any->setChecked(!m_store->options().principleID.has_value());
    connect(any, &QAction::triggered, this, [this] {
        m_store->options().principleID.reset();
        refreshMenus();
        rebuildList();
    });
    for (const Principle& principle : m_principles) {
        QAction* action = principleMenu->addAction(principle.name);
        action->setCheckable(true);
        action->setChecked(m_store->options().principleID == std::optional<qint64>(principle.id));
        connect(action, &QAction::triggered, this, [this, principle] {
            m_store->options().principleID = principle.id;
            refreshMenus();
            rebuildList();
        });
    }
    m_principleButton->setMenu(principleMenu);

    auto* levelMenu = new QMenu(m_levelButton);
    QAction* anyLevel = levelMenu->addAction(QStringLiteral("Any level"));
    anyLevel->setCheckable(true);
    anyLevel->setChecked(!m_store->options().minLevel.has_value());
    connect(anyLevel, &QAction::triggered, this, [this] {
        m_store->options().minLevel.reset();
        refreshMenus();
        rebuildList();
    });
    for (int level = 1; level <= 9; ++level) {
        QAction* action = levelMenu->addAction(QStringLiteral("Level %1+").arg(level));
        action->setCheckable(true);
        action->setChecked(m_store->options().minLevel == std::optional<int>(level));
        connect(action, &QAction::triggered, this, [this, level] {
            m_store->options().minLevel = level;
            refreshMenus();
            rebuildList();
        });
    }
    m_levelButton->setMenu(levelMenu);

    auto* sortMenu = new QMenu(m_sortButton);
    static const std::vector<std::pair<MemorySort, QString>> sorts = {
        {MemorySort::Name, QStringLiteral("Name")},
        {MemorySort::Level, QStringLiteral("Level")},
        {MemorySort::Kind, QStringLiteral("Kind")},
        {MemorySort::Recent, QStringLiteral("Recently added")},
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

void MemoriesScreen::rebuildList()
{
    const qint64 previous = m_store->selectedMemoryID().value_or(-1);
    QSignalBlocker blocker(m_list);
    m_list->clear();
    QHash<QString, QString> tints;
    for (const Principle& principle : m_principles) {
        if (principle.color)
            tints.insert(principle.name, *principle.color);
    }
    for (const Memory& memory : m_store->displayed()) {
        auto* item = new QListWidgetItem(m_list);
        item->setText(memory.name);
        item->setData(Qt::UserRole, memory.id);
        QStringList subtitleParts;
        subtitleParts << kindLabel(memory.kind);
        if (memory.persistent)
            subtitleParts << QStringLiteral("persistent");
        item->setData(MemoryRowDelegate::kSubtitleRole,
                      subtitleParts.join(QStringLiteral(" · ")));
        QStringList badges;
        QStringList tintList;
        for (const Aspect& aspect : memory.aspects) {
            badges << QStringLiteral("%1 %2").arg(aspect.principleName).arg(aspect.level);
            tintList << tints.value(aspect.principleName);
        }
        item->setData(MemoryRowDelegate::kBadgesRole, badges.join(QLatin1Char('|')));
        item->setData(MemoryRowDelegate::kTintsRole, tintList.join(QLatin1Char('|')));
        item->setData(Qt::AccessibleTextRole,
                      QStringLiteral("%1, %2%3.")
                          .arg(memory.name, kindLabel(memory.kind),
                               memory.persistent ? QStringLiteral(", persistent") : QString()));
        if (memory.id == previous)
            m_list->setCurrentItem(item);
    }
    m_content->setVisible(!m_store->memories().empty());
    m_emptyState->setVisible(m_store->memories().empty());
}

void MemoriesScreen::refreshDetail()
{
    QLayoutItem* child;
    while ((child = m_detailLayout->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }
    if (const auto memory = m_store->selectedMemory()) {
        auto* detail = new MemoryDetailView(m_store, m_detailHost);
        connect(detail, &MemoryDetailView::editRequested, this, [this](const Memory& memory) {
            MemoryFormView dialog(memory, m_store->sources(memory.id),
                                  m_store->allYielding(memory.id), m_store->booksForLinking(),
                                  m_principles, this);
            if (dialog.exec() == QDialog::Accepted) {
                m_store->update(memory, dialog.draft());
                m_store->setSources(memory.id, dialog.sources());
                m_store->setYieldingBooks(memory.id, dialog.yieldingBookIDs());
            }
            if (!m_store->lastError().isEmpty())
                showError();
        });
        connect(detail, &MemoryDetailView::deleteRequested, this, [this](const Memory& memory) {
            m_store->remove(memory.id);
            if (!m_store->lastError().isEmpty())
                showError();
        });
        connect(detail, &MemoryDetailView::showBookRequested, this,
                [this](qint64 bookID) { emit showBookRequested(bookID); });
        detail->showMemory(*memory);
        m_detailLayout->addWidget(detail);
    } else if (!m_store->memories().empty()) {
        auto* empty = new QLabel(QStringLiteral("Select a memory to see everything recorded about it."),
                                 m_detailHost);
        m_detailLayout->addWidget(empty);
    }
}

void MemoriesScreen::showError()
{
    QMessageBox::warning(this, QStringLiteral("Something went wrong"), m_store->lastError());
    m_store->clearError();
}

void MemoriesScreen::addNew()
{
    addNewRequested();
}

void MemoriesScreen::addNewRequested()
{
    MemoryFormView dialog(m_principles, this);
    if (dialog.exec() == QDialog::Accepted)
        m_store->add(dialog.draft());
    if (!m_store->lastError().isEmpty())
        showError();
}

void MemoriesScreen::focusSearch()
{
    m_search->setFocus();
    m_search->selectAll();
}

// MARK: - MemoryDetailView (display-only; editing is the Edit… form's job)

MemoryDetailView::MemoryDetailView(MemoriesStore* store, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_layout = new QVBoxLayout();
    m_layout->setAlignment(Qt::AlignTop);
    outer->addLayout(m_layout);
    clearMemory();
}

void MemoryDetailView::clearMemory()
{
    QLayoutItem* child;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }
    m_memory.reset();
    m_layout->addWidget(new QLabel(QStringLiteral("Select a memory to see everything recorded "
                                                   "about it."),
                                   this));
}

void MemoryDetailView::rebuild(const Memory& memory)
{
    const bool sameMemory = m_memory && m_memory->id == memory.id;
    const bool notesPristine = !m_notesDirty;
    m_memory = memory;

    QLayoutItem* child;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }

    auto* headerRow = new QHBoxLayout();
    auto* title = new QLabel(memory.name, this);
    title->setWordWrap(true);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.5);
    titleFont.setBold(true);
    title->setFont(titleFont);
    headerRow->addWidget(title, 1);
    auto* editButton = new QPushButton(QStringLiteral("Edit…"), this);
    connect(editButton, &QPushButton::clicked, this,
            [this] { emit editRequested(*m_memory); });
    headerRow->addWidget(editButton);
    m_layout->addLayout(headerRow);
    m_layout->addWidget(new QLabel(QStringLiteral("%1%2")
                                       .arg(kindLabel(memory.kind),
                                            memory.persistent
                                                ? QStringLiteral(" · persistent (survives dawn)")
                                                : QString()),
                                   this));

    // Aspects as read-only badges.
    if (!memory.aspects.empty()) {
        auto* aspectsTitle = new QLabel(QStringLiteral("Aspects"), this);
        aspectsTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
        m_layout->addWidget(aspectsTitle);
        auto* badgeRow = new QHBoxLayout();
        const bool dark = Badges::isDarkMode(this);
        for (const Aspect& aspect : memory.aspects) {
            auto* badge = new QLabel(aspect.principleName, this);
            badge->setAlignment(Qt::AlignCenter);
            const QString backdrop =
                ColorMath::blend(m_store->principleColor(aspect.principleID),
                                 Badges::isDarkMode(this) ? ColorMath::darkWindowBackground()
                                                          : ColorMath::lightWindowBackground(),
                                 0.18)
                    .value_or(QStringLiteral("#888888"));
            const QString textColor =
                ColorMath::readableTextHex(m_store->principleColor(aspect.principleID), dark)
                    .value_or(QStringLiteral("#000000"));
            badge->setStyleSheet(
                QStringLiteral("background: %1; color: %2; border-radius: 6px; padding: 2px 8px;")
                    .arg(backdrop, textColor));
            badge->setAccessibleName(QStringLiteral("%1 %2")
                                         .arg(aspect.principleName)
                                         .arg(aspect.level));
            badgeRow->addWidget(badge);
        }
        badgeRow->addStretch(1);
        m_layout->addLayout(badgeRow);
        Q_UNUSED(sameMemory);
    }

    // Sources (display-only; editing lives in the Edit… form).
    auto* sourcesTitle = new QLabel(QStringLiteral("How to obtain"), this);
    sourcesTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
    m_layout->addWidget(sourcesTitle);
    const auto sources = m_store->sources(memory.id);
    if (sources.empty()) {
        m_layout->addWidget(new QLabel(QStringLiteral("none recorded"), this));
    } else {
        for (const MemorySource& source : sources) {
            m_layout->addWidget(new QLabel(
                QStringLiteral("• %1%2")
                    .arg(source.kind,
                         source.detail ? QStringLiteral(" — %1").arg(*source.detail) : QString()),
                this));
        }
    }

    // Mastered-only yielding backlinks (live cache).
    auto* yieldsTitle = new QLabel(QStringLiteral("Books that yield this"), this);
    yieldsTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
    m_layout->addWidget(yieldsTitle);
    const auto yielding = m_store->yielding(memory.id);
    if (yielding.empty()) {
        m_layout->addWidget(new QLabel(QStringLiteral("No mastered book yields this yet."), this));
    } else {
        for (const BookRef& ref : yielding) {
            auto* link = new QLabel(QStringLiteral("<a href=\"book:%1\">%2</a>")
                                        .arg(ref.id)
                                        .arg(ref.title.toHtmlEscaped()),
                                    this);
            link->setTextFormat(Qt::RichText);
            link->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard
                                          | Qt::LinksAccessibleByMouse);
            connect(link, &QLabel::linkActivated, this, [this](const QString& link) {
                bool ok = false;
                const qint64 bookID = link.mid(5).toLongLong(&ok);
                if (ok)
                    emit showBookRequested(bookID);
            });
            m_layout->addWidget(link);
        }
    }

    // Notes: explicit save flow, guarded re-seed.
    auto* notesTitle = new QLabel(QStringLiteral("Notes"), this);
    notesTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
    m_layout->addWidget(notesTitle);
    m_notesEditor = new QPlainTextEdit(this);
    m_notesEditor->setAccessibleName(QStringLiteral("Memory notes"));
    if (!sameMemory || notesPristine) {
        const QString noteText = memory.notes.value_or(QString());
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
        if (!m_memory)
            return;
        m_notesEditor->setPlainText(m_memory->notes.value_or(QString()));
        m_notesDirty = false;
    });
    notesRow->addWidget(save);
    notesRow->addWidget(revert);
    notesRow->addStretch(1);
    m_layout->addLayout(notesRow);

    auto* deleteRow = new QHBoxLayout();
    auto* deleteButton = new QPushButton(QStringLiteral("Delete Memory…"), this);
    deleteButton->setStyleSheet(QStringLiteral("color: #C0392B;"));
    connect(deleteButton, &QPushButton::clicked, this, [this] {
        if (!m_memory)
            return;
        if (QMessageBox::question(this, QStringLiteral("Delete Memory"),
                                  QStringLiteral("Delete “%1”? Books that yield it keep their "
                                                 "data but lose the link.")
                                      .arg(m_memory->name))
            == QMessageBox::Yes)
            emit deleteRequested(*m_memory);
    });
    deleteRow->addWidget(deleteButton);
    deleteRow->addStretch(1);
    m_layout->addLayout(deleteRow);
}

void MemoryDetailView::saveNote()
{
    if (!m_memory)
        return;
    m_store->updateNotes(*m_memory, m_notesEditor->toPlainText());
    m_notesDirty = false;
}

void MemoryDetailView::showMemory(const Memory& memory)
{
    rebuild(memory);
}

// MARK: - MemoryFormView

MemoryFormView::MemoryFormView(const std::vector<Principle>& principles, QWidget* parent)
    : QDialog(parent)
    , m_principles(principles)
{
    setWindowTitle(QStringLiteral("Add Memory"));
    build();
}

MemoryFormView::MemoryFormView(const Memory& memory, const std::vector<MemorySource>& sources,
                               const std::vector<BookRef>& yielding,
                               const std::vector<BookRef>& bookLinks,
                               const std::vector<Principle>& principles, QWidget* parent)
    : QDialog(parent)
    , m_principles(principles)
    , m_original(memory)
    , m_linkCandidates(bookLinks)
{
    setWindowTitle(QStringLiteral("Edit Memory"));
    for (const MemorySource& source : sources)
        m_sourceRows.push_back(SourceRow{nullptr, nullptr, source.kind,
                                         source.detail.value_or(QString())});
    m_currentLinkIDs.reserve(yielding.size());
    for (const BookRef& ref : yielding)
        m_currentLinkIDs.insert(ref.id);
    build();
}

void MemoryFormView::build()
{
    auto* layout = new QVBoxLayout(this);

    // The memory (plain rows — the grouped-Form lesson is structural now, but
    // Qt lets us keep everything in plain vertical blocks anyway).
    auto* form = new QFormLayout();
    m_name = new QLineEdit(this);
    form->addRow(QStringLiteral("Name"), m_name);
    m_kind = new QComboBox(this);
    for (MemoryKind kind : {MemoryKind::Memory, MemoryKind::Weather, MemoryKind::Numen})
        m_kind->addItem(kindLabel(kind), int(kind));
    form->addRow(QStringLiteral("Kind"), m_kind);
    m_persistent = new QCheckBox(QStringLiteral("Persistent (survives dawn)"), this);
    form->addRow(QString(), m_persistent);
    layout->addLayout(form);

    layout->addWidget(new QLabel(QStringLiteral("Aspects"), this));
    auto* aspectEditor = new AspectEditor(m_principles, this);
    m_aspectsEditor = aspectEditor;
    layout->addWidget(aspectEditor);

    layout->addWidget(new QLabel(QStringLiteral("Notes"), this));
    m_notes = new QPlainTextEdit(this);
    m_notes->setMinimumHeight(48);
    layout->addWidget(m_notes);

    // Edit mode: sources + yielding links, plain editor blocks (NOT Form rows).
    if (m_original) {
        layout->addWidget(new QLabel(QStringLiteral("How to obtain (source rows)"), this));
        m_sourceRowsLayout = new QVBoxLayout();
        m_sourceRowsLayout->setSpacing(4);
        layout->addLayout(m_sourceRowsLayout);
        auto* addSource = new QPushButton(QStringLiteral("Add source"), this);
        connect(addSource, &QPushButton::clicked, this, [this] {
            m_sourceRows.push_back(SourceRow{nullptr, nullptr, QStringLiteral("first read"), QString()});
            buildSourceRows();
        });
        layout->addWidget(addSource);

        layout->addWidget(new QLabel(QStringLiteral("Books that yield this (any status — "
                                                     "checking/unchecking syncs the links)"),
                                     this));
        m_linksList = new QListWidget(this);
        m_linksList->setAccessibleName(QStringLiteral("Books that yield this memory"));
        layout->addWidget(m_linksList);
    }

    auto* buttons = new QHBoxLayout();
    auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* save = new QPushButton(QStringLiteral("Save"), this);
    save->setDefault(true);
    connect(save, &QPushButton::clicked, this, [this] {
        if (m_name->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Memory"),
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
        const Memory& memory = *m_original;
        m_name->setText(memory.name);
        m_kind->setCurrentIndex(int(memory.kind));
        m_persistent->setChecked(memory.persistent);
        std::vector<AspectDraft> aspectDrafts;
        aspectDrafts.reserve(memory.aspects.size());
        for (const Aspect& aspect : memory.aspects)
            aspectDrafts.push_back(AspectDraft{aspect.principleID, aspect.level});
        aspectEditor->setAspects(aspectDrafts);
        m_notes->setPlainText(memory.notes.value_or(QString()));

        // Links list: every book; checked = currently linked.
        for (const BookRef& candidate : m_linkCandidates) {
            auto* item = new QListWidgetItem(candidate.title, m_linksList);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(m_currentLinkIDs.contains(candidate.id) ? Qt::Checked
                                                                        : Qt::Unchecked);
            item->setData(Qt::UserRole, candidate.id);
        }
    }
    buildSourceRows();
}

void MemoryFormView::buildSourceRows()
{
    if (!m_sourceRowsLayout)
        return;
    QLayoutItem* child;
    while ((child = m_sourceRowsLayout->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }
    static const std::vector<const char*> kinds = {
        "re-read book", "first read", "weather", "talk", "consider",
        "consume", "craft", "gather", "numa", "other",
    };
    if (m_sourceRows.empty()) {
        m_sourceRowsLayout->addWidget(new QLabel(QStringLiteral("none"), this));
        return;
    }
    for (SourceRow& row : m_sourceRows) {
        auto* widget = new QWidget(this);
        auto* layout = new QHBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        row.kind = new QComboBox(widget);
        for (const char* kind : kinds)
            row.kind->addItem(QString::fromUtf8(kind));
        row.detail = new QLineEdit(widget);
        row.detail->setPlaceholderText(QStringLiteral("Detail — e.g. Talk with the Rector (17%)"));
        const int kindIndex = row.kind->findText(row.kindValue);
        row.kind->setCurrentIndex(kindIndex < 0 ? 0 : kindIndex);
        row.detail->setText(row.detailValue);
        // Live edits survive rebuilds (remove-row reflows).
        connect(row.kind, &QComboBox::currentTextChanged, this, [&row](const QString& text) {
            row.kindValue = text;
        });
        connect(row.detail, &QLineEdit::textChanged, this, [&row](const QString& text) {
            row.detailValue = text;
        });
        auto* remove = new QPushButton(QStringLiteral("✕"), widget);
        remove->setAccessibleName(QStringLiteral("Remove source"));
        connect(remove, &QPushButton::clicked, this, [this, widget] {
            for (auto it = m_sourceRows.begin(); it != m_sourceRows.end(); ++it) {
                if (it->kind && it->kind->parentWidget() == widget) {
                    m_sourceRows.erase(it);
                    break;
                }
            }
            buildSourceRows();
        });
        Q_UNUSED(remove);
        layout->addWidget(row.kind, 0);
        layout->addWidget(row.detail, 1);
        layout->addWidget(remove);
        widget->setLayout(layout);
        m_sourceRowsLayout->addWidget(widget);
    }
}

MemoryDraft MemoryFormView::draft() const
{
    MemoryDraft draft;
    draft.name = m_name->text().trimmed();
    draft.kind = MemoryKind(m_kind->currentData().toInt());
    draft.persistent = m_persistent->isChecked();
    if (m_notes->toPlainText().trimmed().isEmpty())
        draft.notes = m_original ? m_original->notes : std::nullopt;
    else
        draft.notes = m_notes->toPlainText().trimmed();
    auto* aspectEditor = qobject_cast<AspectEditor*>(m_aspectsEditor);
    if (aspectEditor)
        draft.aspects = aspectEditor->aspects();
    return draft;
}


std::vector<MemorySource> MemoryFormView::sources() const
{
    std::vector<MemorySource> out;
    for (const SourceRow& row : m_sourceRows) {
        MemorySource source;
        source.kind = row.kind ? row.kind->currentText() : row.kindValue;
        if (row.detail && !row.detail->text().trimmed().isEmpty())
            source.detail = row.detail->text().trimmed();
        out.push_back(source);
    }
    return out;
}

std::vector<qint64> MemoryFormView::yieldingBookIDs() const
{
    std::vector<qint64> out;
    if (!m_linksList)
        return out;
    for (int i = 0; i < m_linksList->count(); ++i) {
        if (m_linksList->item(i)->checkState() == Qt::Checked)
            out.push_back(m_linksList->item(i)->data(Qt::UserRole).toLongLong());
    }
    return out;
}

} // namespace boh
