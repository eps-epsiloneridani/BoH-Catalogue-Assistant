#include "ReadingHelperScreen.h"

#include "AppController.h"
#include "ReadingMath.h"
#include "Books/RecordReadDialog.h"
#include "Shared/Badges.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

namespace boh {

namespace {
QString readStatusLabel(ReadStatus status)
{
    switch (status) {
    case ReadStatus::Uncatalogued: return QStringLiteral("uncatalogued");
    case ReadStatus::Catalogued: return QStringLiteral("catalogued");
    case ReadStatus::Mastered: return QStringLiteral("mastered");
    }
    return QString();
}
} // namespace

ReadingHelperScreen::ReadingHelperScreen(ReadingHelperStore* store, AppController* controller,
                                         const std::vector<Principle>& principles, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
    , m_controller(controller)
    , m_principles(principles)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_content = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(8, 8, 8, 0);

    auto* bar = new QHBoxLayout();
    m_search = new QLineEdit(m_content);
    m_search->setPlaceholderText(QStringLiteral("Search the picker…"));
    m_search->setClearButtonEnabled(true);
    m_search->setAccessibleName(QStringLiteral("Search the book picker"));
    bar->addWidget(m_search, 1);
    m_filterButton = new QToolButton(m_content);
    m_filterButton->setText(QStringLiteral("Filter"));
    m_filterButton->setPopupMode(QToolButton::InstantPopup);
    m_filterButton->setAccessibleName(QStringLiteral("Filter read status"));
    bar->addWidget(m_filterButton);
    m_mysteryButton = new QToolButton(m_content);
    m_mysteryButton->setText(QStringLiteral("Mystery"));
    m_mysteryButton->setPopupMode(QToolButton::InstantPopup);
    m_mysteryButton->setAccessibleName(QStringLiteral("Filter by mystery"));
    bar->addWidget(m_mysteryButton);
    m_sortButton = new QToolButton(m_content);
    m_sortButton->setText(QStringLiteral("Sort"));
    m_sortButton->setPopupMode(QToolButton::InstantPopup);
    m_sortButton->setAccessibleName(QStringLiteral("Sort the picker"));
    bar->addWidget(m_sortButton);
    contentLayout->addLayout(bar);

    auto* splitter = new QSplitter(m_content);
    m_picker = new QListWidget(splitter);
    m_picker->setAccessibleName(QStringLiteral("Books to read"));
    splitter->addWidget(m_picker);
    m_panelHost = new QWidget(splitter);
    m_panelLayout = new QVBoxLayout(m_panelHost);
    m_panelLayout->setContentsMargins(0, 0, 0, 0);
    splitter->addWidget(m_panelHost);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    contentLayout->addWidget(splitter, 1);

    m_emptyState = new QWidget(this);
    auto* emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->addStretch(1);
    auto* headline = new QLabel(QStringLiteral("Nothing to plan yet"), m_emptyState);
    headline->setAlignment(Qt::AlignCenter);
    QFont headlineFont = headline->font();
    headlineFont.setPointSizeF(headlineFont.pointSizeF() * 1.4);
    headlineFont.setBold(true);
    headline->setFont(headlineFont);
    emptyLayout->addWidget(headline);
    auto* hint = new QLabel(
        QStringLiteral("Add books on the Books screen as you catalogue them — then come here to "
                       "plan each read."),
        m_emptyState);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    emptyLayout->addWidget(hint);
    emptyLayout->addStretch(2);

    layout->addWidget(m_content);
    layout->addWidget(m_emptyState);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_store->searchText = text;
        rebuildPicker();
    });
    connect(m_picker, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* current, QListWidgetItem*) {
                if (current) {
                    m_store->setSelectedBookID(current->data(Qt::UserRole).toLongLong());
                    refreshPanel();
                }
            });
    connect(m_store, &ReadingHelperStore::changed, this, [this] {
        m_store->ensureSelection(); // at body level — never lost on empty-state timing
        rebuildPicker();
        refreshPanel();
    });

    refreshMenus();
    m_store->ensureSelection();
    rebuildPicker();
    refreshPanel();
}

void ReadingHelperScreen::setStore(ReadingHelperStore* store)
{
    if (store == m_store)
        return;
    m_store = store;
    connect(m_store, &ReadingHelperStore::changed, this, [this] {
        m_store->ensureSelection();
        rebuildPicker();
        refreshPanel();
    });
    rebuildPicker();
    refreshPanel();
}

void ReadingHelperScreen::selectBook(qint64 bookID)
{
    m_store->setSelectedBookID(bookID);
    rebuildPicker();
    refreshPanel();
}

void ReadingHelperScreen::focusSearch()
{
    m_search->setFocus();
    m_search->selectAll();
}

void ReadingHelperScreen::refreshMenus()
{
    auto* filterMenu = new QMenu(m_filterButton);
    static const std::vector<std::pair<BookStatusFilter, QString>> filters = {
        {BookStatusFilter::All, QStringLiteral("All")},
        {BookStatusFilter::Unread, QStringLiteral("Unread")},
        {BookStatusFilter::Uncatalogued, QStringLiteral("Uncatalogued")},
        {BookStatusFilter::Catalogued, QStringLiteral("Catalogued")},
        {BookStatusFilter::Mastered, QStringLiteral("Mastered")},
        {BookStatusFilter::Contaminated, QStringLiteral("Contaminated")},
    };
    for (const auto& [filter, label] : filters) {
        QAction* action = filterMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_store->statusFilter == filter);
        connect(action, &QAction::triggered, this, [this, filter] {
            m_store->statusFilter = filter;
            refreshMenus();
            rebuildPicker();
        });
    }
    m_filterButton->setMenu(filterMenu);

    auto* mysteryMenu = new QMenu(m_mysteryButton);
    QAction* anyMystery = mysteryMenu->addAction(QStringLiteral("Any mystery"));
    anyMystery->setCheckable(true);
    anyMystery->setChecked(!m_store->mysteryPrincipleID.has_value());
    connect(anyMystery, &QAction::triggered, this, [this] {
        m_store->mysteryPrincipleID.reset();
        refreshMenus();
        rebuildPicker();
    });
    for (const Principle& principle : m_principles) {
        QAction* action = mysteryMenu->addAction(principle.name);
        action->setCheckable(true);
        action->setChecked(m_store->mysteryPrincipleID == std::optional<qint64>(principle.id));
        connect(action, &QAction::triggered, this, [this, principle] {
            m_store->mysteryPrincipleID = principle.id;
            refreshMenus();
            rebuildPicker();
        });
    }
    m_mysteryButton->setMenu(mysteryMenu);

    auto* sortMenu = new QMenu(m_sortButton);
    static const std::vector<std::pair<BookSort, QString>> sorts = {
        {BookSort::Status, QStringLiteral("Status (unread first)")},
        {BookSort::Title, QStringLiteral("Title")},
        {BookSort::Difficulty, QStringLiteral("Difficulty")},
        {BookSort::Easiest, QStringLiteral("Easiest first")},
        {BookSort::Recent, QStringLiteral("Recently added")},
    };
    for (const auto& [sort, label] : sorts) {
        QAction* action = sortMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_store->sort == sort);
        connect(action, &QAction::triggered, this, [this, sort] {
            m_store->sort = sort;
            refreshMenus();
            rebuildPicker();
        });
    }
    m_sortButton->setMenu(sortMenu);
}

void ReadingHelperScreen::rebuildPicker()
{
    const qint64 previous = m_store->selectedBookID().value_or(-1);
    QSignalBlocker blocker(m_picker);
    m_picker->clear();
    for (const Book& book : m_store->displayed()) {
        auto* item = new QListWidgetItem(m_picker);
        QString line = book.title;
        if (const auto difficulty = book.difficulty)
            line += QStringLiteral(" — %1").arg(*difficulty);
        item->setText(line);
        item->setData(Qt::UserRole, book.id);
        QStringList subtitleParts;
        subtitleParts << readStatusLabel(book.readStatus);
        if (const QString language = m_store->languageName(book.languageID); !language.isEmpty())
            subtitleParts << language;
        // Composite-row a11y (D12).
        item->setData(Qt::AccessibleTextRole,
                      QStringLiteral("%1. %2.").arg(book.title,
                                                    subtitleParts.join(QStringLiteral(" · "))));
        if (book.id == previous)
            m_picker->setCurrentItem(item);
    }
    m_content->setVisible(!m_store->books().empty());
    m_emptyState->setVisible(m_store->books().empty());
}

void ReadingHelperScreen::refreshPanel()
{
    QLayoutItem* child;
    while ((child = m_panelLayout->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }
    const auto book = m_store->selectedBook();
    if (!book) {
        m_panelLayout->addWidget(new QLabel(QStringLiteral("Pick a book to plan its read."), this));
        return;
    }

    // Header + record button.
    auto* headerRow = new QHBoxLayout();
    auto* title = new QLabel(book->title, this);
    title->setWordWrap(true);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.4);
    titleFont.setBold(true);
    title->setFont(titleFont);
    headerRow->addWidget(title, 1);
    auto* recordButton = new QPushButton(QStringLiteral("Record read…"), this);
    connect(recordButton, &QPushButton::clicked, this,
            [this] { recordRead(*m_store->selectedBook(), 0); });
    headerRow->addWidget(recordButton);
    m_panelLayout->addLayout(headerRow);
    QStringList subtitleParts;
    subtitleParts << readStatusLabel(book->readStatus);
    if (const QString language = m_store->languageName(book->languageID); !language.isEmpty())
        subtitleParts << language;
    m_panelLayout->addWidget(new QLabel(subtitleParts.join(QStringLiteral(" · ")), this));

    // Mastered books: the re-reading section with the yield backlink.
    if (book->readStatus == ReadStatus::Mastered) {
        m_panelLayout->addWidget(new QLabel(
            QStringLiteral("Mastered — re-reads take 60 seconds with any soul, no mystery check."),
            this));
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(QStringLiteral("Always yields"), this));
        if (book->yieldedMemoryID) {
            const QString name = m_store->yieldedMemoryName(*book);
            if (!name.isEmpty()) {
                auto* link = new QLabel(QStringLiteral("<a href=\"memory:%1\">%2</a>")
                                            .arg(*book->yieldedMemoryID)
                                            .arg(name.toHtmlEscaped()),
                                        this);
                link->setTextFormat(Qt::RichText);
                link->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard
                                              | Qt::LinksAccessibleByMouse);
                const qint64 memoryID = *book->yieldedMemoryID;
                connect(link, &QLabel::linkActivated, this,
                        [this, memoryID](const QString&) { emit showMemoryRequested(memoryID); });
                row->addWidget(link);
            } else {
                row->addWidget(new QLabel(QStringLiteral("not recorded"), this));
            }
        } else {
            row->addWidget(new QLabel(QStringLiteral("not recorded"), this));
        }
        row->addStretch(1);
        m_panelLayout->addLayout(row);
    } else {
        m_panelLayout->addWidget(new QLabel(
            QStringLiteral("Every read gives this book's memory — use “Record read…” to note "
                           "which one."),
            this));
    }

    // The requirement + recorded reach.
    auto* requirementTitle = new QLabel(QStringLiteral("THE REQUIREMENT"), this);
    requirementTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
    m_panelLayout->addWidget(requirementTitle);
    m_panelLayout->addWidget(new QLabel(
        ReadingMath::requirementLine(
            m_store->principleName(book->mysteryPrincipleID), book->difficulty),
        this));
    const std::optional<QString> reach =
        ReadingMath::reachLine(book->difficulty,
                               [&] {
                                   auto candidates =
                                       m_store->memoryCandidates(*book);
                                   if (candidates.satisfying.empty()
                                       && candidates.nearMisses.empty())
                                       return std::optional<int>{};
                                   int best = 0;
                                   for (const MemoryCandidate& c : candidates.satisfying)
                                       best = std::max(best, c.level);
                                   for (const MemoryCandidate& c : candidates.nearMisses)
                                       best = std::max(best, c.level);
                                   return std::optional<int>(best);
                               }(),
                               [&]() -> std::optional<int> {
                                   const auto contributions =
                                       m_store->skillContributions(*book);
                                   if (contributions.empty())
                                       return std::nullopt;
                                   int best = 0;
                                   for (const SkillContribution& c : contributions)
                                       best = std::max(best, c.contributes);
                                   return best;
                               }());
    if (reach)
        m_panelLayout->addWidget(new QLabel(*reach, this));

    if (book->languageID) {
        const auto known = m_store->isLanguageKnown(book->languageID);
        m_panelLayout->addWidget(new QLabel(
            QStringLiteral("Language: %1%2")
                .arg(m_store->languageName(book->languageID),
                     known.has_value()
                         ? QStringLiteral(" — %1").arg(*known ? QStringLiteral("known")
                                                              : QStringLiteral("NOT known"))
                         : QString()),
            this));
    }

    // Memory candidates: earned-only, satisfying first, then near-misses.
    const auto candidates = m_store->memoryCandidates(*book);
    auto* candidatesTitle = new QLabel(
        candidates.satisfying.empty()
            ? QStringLiteral("CLOSEST RECORDED")
            : QStringLiteral("USE ONE OF THESE"),
        this);
    candidatesTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
    m_panelLayout->addWidget(candidatesTitle);
    if (book->mysteryPrincipleID && book->difficulty) {
        if (candidates.satisfying.empty() && candidates.nearMisses.empty()) {
            m_panelLayout->addWidget(new QLabel(
                QStringLiteral("No recorded memory carries this principle yet — record one when "
                               "you gain it, or lean on skills, weather and inks."),
                this));
        }
        for (const MemoryCandidate& candidate : candidates.satisfying) {
            auto* button = new QPushButton(
                QStringLiteral("%1 (level %2)").arg(candidate.name).arg(candidate.level), this);
            button->setAccessibleName(QStringLiteral("Record the read with %1")
                                          .arg(candidate.name));
            const qint64 id = candidate.id;
            connect(button, &QPushButton::clicked, this,
                    [this, id] { recordRead(*m_store->selectedBook(), id); });
            m_panelLayout->addWidget(button);
        }
        for (const MemoryCandidate& candidate : candidates.nearMisses) {
            m_panelLayout->addWidget(new QLabel(
                QStringLiteral("%1 (level %2 — short)").arg(candidate.name).arg(candidate.level),
                this));
        }
    } else {
        m_panelLayout->addWidget(new QLabel(
            QStringLiteral("Candidates appear once the book's principle and difficulty are "
                           "recorded."),
            this));
    }

    // Skills that help.
    auto* skillsTitle = new QLabel(QStringLiteral("SKILLS THAT HELP"), this);
    skillsTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
    m_panelLayout->addWidget(skillsTitle);
    const auto contributions = m_store->skillContributions(*book);
    if (contributions.empty()) {
        m_panelLayout->addWidget(new QLabel(
            QStringLiteral("No recorded skill carries this principle yet. A level-L skill "
                           "contributes L+1 to its primary principle, L to its secondary."),
            this));
    } else {
        for (const SkillContribution& contribution : contributions) {
            const QString role = contribution.skill.primaryPrincipleID == book->mysteryPrincipleID
                                     ? QStringLiteral("primary")
                                     : QStringLiteral("secondary");
            m_panelLayout->addWidget(new QLabel(
                QStringLiteral("%1 — %2 + %3 (level %4)")
                    .arg(contribution.skill.name, role)
                    .arg(contribution.contributes)
                    .arg(contribution.skill.level.value_or(0)),
                this));
        }
    }
    m_panelLayout->addStretch(1);
}

void ReadingHelperScreen::recordRead(const Book& book, qint64 preselectedMemoryID)
{
    RecordReadDialog dialog(book, m_controller ? m_controller->booksStore() : nullptr,
                            m_controller, this);
    dialog.setPreselectedMemoryID(preselectedMemoryID);
    dialog.exec();
}

void ReadingHelperScreen::showError()
{
    // the helper has no lastError of its own (Core calls are try?-wrapped upstream)
}

} // namespace boh
