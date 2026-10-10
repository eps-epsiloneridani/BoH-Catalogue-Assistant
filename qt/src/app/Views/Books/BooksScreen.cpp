#include "BooksScreen.h"

#include "AppController.h"
#include "RecordReadDialog.h"
#include "Shared/Badges.h"
#include "Shared/BookRowDelegate.h"

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
QString readStatusLabel(ReadStatus status)
{
    switch (status) {
    case ReadStatus::Uncatalogued: return QStringLiteral("uncatalogued");
    case ReadStatus::Catalogued: return QStringLiteral("catalogued");
    case ReadStatus::Mastered: return QStringLiteral("mastered");
    }
    return QString();
}

QString contaminationLabel(std::optional<Contamination> contamination)
{
    if (!contamination)
        return QStringLiteral("—");
    switch (*contamination) {
    case Contamination::None: return QStringLiteral("checked clean");
    case Contamination::Curse: return QStringLiteral("Curse");
    case Contamination::Theoplasm: return QStringLiteral("Theoplasm");
    case Contamination::Infestation: return QStringLiteral("Infestation");
    case Contamination::Corruption: return QStringLiteral("Corruption");
    case Contamination::Winkwell: return QStringLiteral("Winkwell");
    case Contamination::Witchworms: return QStringLiteral("Witchworms");
    }
    return QString();
}

QString orDash(const QString& text) { return text.isEmpty() ? QStringLiteral("—") : text; }
} // namespace

// MARK: - BooksScreen

BooksScreen::BooksScreen(BooksStore* store, const std::vector<Principle>& principles,
                         const std::vector<Language>& languages, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
    , m_principles(principles)
    , m_languages(languages)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* content = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(8, 8, 8, 0);

    // Toolbar: search + filter + sort + add.
    auto* bar = new QHBoxLayout();
    m_search = new QLineEdit(content);
    m_search->setPlaceholderText(QStringLiteral("Search title, set, notes…"));
    m_search->setClearButtonEnabled(true);
    m_search->setAccessibleName(QStringLiteral("Search books"));
    bar->addWidget(m_search, 1);

    m_filterButton = new QToolButton(content);
    m_filterButton->setText(QStringLiteral("Filter"));
    m_filterButton->setPopupMode(QToolButton::InstantPopup);
    m_filterButton->setAccessibleName(QStringLiteral("Filter books"));
    bar->addWidget(m_filterButton);

    m_sortButton = new QToolButton(content);
    m_sortButton->setText(QStringLiteral("Sort"));
    m_sortButton->setPopupMode(QToolButton::InstantPopup);
    m_sortButton->setAccessibleName(QStringLiteral("Sort books"));
    bar->addWidget(m_sortButton);

    auto* add = new QPushButton(QStringLiteral("Add Book"), content);
    add->setToolTip(QStringLiteral("Record a newly found book (Ctrl+N)"));
    bar->addWidget(add);
    contentLayout->addLayout(bar);

    // List + detail.
    auto* splitter = new QSplitter(content);
    m_list = new QListWidget(splitter);
    m_list->setItemDelegate(new BookRowDelegate(m_list));
    m_list->setUniformItemSizes(false);
    splitter->addWidget(m_list);

    m_detail = new BookDetailView(m_store, splitter);
    splitter->addWidget(m_detail);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    contentLayout->addWidget(splitter, 1);

    // Empty state (the whole-book list empty branch).
    m_emptyState = new QWidget(this);
    auto* emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->addStretch(1);
    auto* headline = new QLabel(QStringLiteral("No books recorded yet"), m_emptyState);
    headline->setAlignment(Qt::AlignCenter);
    QFont headlineFont = headline->font();
    headlineFont.setPointSizeF(headlineFont.pointSizeF() * 1.4);
    headlineFont.setBold(true);
    headline->setFont(headlineFont);
    emptyLayout->addWidget(headline);
    auto* hint = new QLabel(
        QStringLiteral("Catalogue the first book you find in Hush House — a title and its "
                       "mystery is enough to start."),
        m_emptyState);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    emptyLayout->addWidget(hint);
    auto* emptyAdd = new QPushButton(QStringLiteral("Add Book"), m_emptyState);
    connect(emptyAdd, &QPushButton::clicked, this, &BooksScreen::addNew);
    auto* emptyAddRow = new QHBoxLayout();
    emptyAddRow->addStretch(1);
    emptyAddRow->addWidget(emptyAdd);
    emptyAddRow->addStretch(1);
    emptyLayout->addLayout(emptyAddRow);
    emptyLayout->addStretch(2);

    layout->addWidget(m_content = content);
    layout->addWidget(m_emptyState);

    // Wiring.
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_store->options().searchText = text;
        rebuildList();
    });
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem*) {
        if (current)
            m_store->setSelectedBookID(current->data(BookRowDelegate::kBookIDRole).toLongLong());
        refreshDetail();
    });
    connect(add, &QPushButton::clicked, this, &BooksScreen::addNew);
    connect(m_detail, &BookDetailView::deleteRequested, this, [this](const Book& book) {
        m_store->remove(book.id);
        if (!m_store->lastError().isEmpty())
            showError();
    });
    connect(m_detail, &BookDetailView::markReadRequested, this, [this](const Book& book) {
        RecordReadDialog dialog(book, m_store, m_controller, this);
        dialog.exec();
        if (!m_store->lastError().isEmpty())
            showError();
    });
    connect(m_store, &BooksStore::changed, this, [this] {
        rebuildList();
        refreshDetail();
    });

    refreshFilterMenu();
    rebuildList();
    refreshDetail();
}

void BooksScreen::setController(AppController* controller)
{
    m_controller = controller;
}

void BooksScreen::setStore(BooksStore* store)
{
    if (store == m_store)
        return;
    m_store = store;
    connect(m_store, &BooksStore::changed, this, [this] {
        rebuildList();
        refreshDetail();
    });
    refreshFilterMenu();
    rebuildList();
    refreshDetail();
}

void BooksScreen::refreshFilterMenu()
{
    static const std::vector<std::pair<BookStatusFilter, QString>> filters = {
        {BookStatusFilter::All, QStringLiteral("All")},
        {BookStatusFilter::Unread, QStringLiteral("Unread")},
        {BookStatusFilter::Uncatalogued, QStringLiteral("Uncatalogued")},
        {BookStatusFilter::Catalogued, QStringLiteral("Catalogued")},
        {BookStatusFilter::Mastered, QStringLiteral("Mastered")},
        {BookStatusFilter::Contaminated, QStringLiteral("Contaminated")},
    };
    auto* menu = new QMenu(m_filterButton);
    for (const auto& [filter, label] : filters) {
        QAction* action = menu->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_store->options().statusFilter == filter);
        connect(action, &QAction::triggered, this, [this, filter] {
            m_store->options().statusFilter = filter;
            refreshFilterMenu();
            rebuildList();
        });
    }
    m_filterButton->setMenu(menu);

    auto* sortMenu = new QMenu(m_sortButton);
    static const std::vector<std::pair<BookSort, QString>> sorts = {
        {BookSort::Title, QStringLiteral("Title")},
        {BookSort::Difficulty, QStringLiteral("Difficulty")},
        {BookSort::Status, QStringLiteral("Status")},
        {BookSort::Easiest, QStringLiteral("Easiest first")},
        {BookSort::Recent, QStringLiteral("Recently added")},
    };
    for (const auto& [sort, label] : sorts) {
        QAction* action = sortMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_store->options().sort == sort);
        connect(action, &QAction::triggered, this, [this, sort] {
            m_store->options().sort = sort;
            refreshFilterMenu();
            rebuildList();
        });
    }
    m_sortButton->setMenu(sortMenu);
}

void BooksScreen::rebuildList()
{
    const qint64 previousSelection = m_store->selectedBookID().value_or(-1);
    QSignalBlocker blocker(m_list);
    m_list->clear();
    for (const Book& book : m_store->displayed()) {
        auto* item = new QListWidgetItem(m_list);
        item->setData(BookRowDelegate::kBookIDRole, book.id);
        item->setData(BookRowDelegate::kTitleRole, book.title);
        QStringList subtitleParts;
        subtitleParts << readStatusLabel(book.readStatus);
        if (const QString language = m_store->languageName(book.languageID); !language.isEmpty())
            subtitleParts << language;
        if (book.setName)
            subtitleParts << *book.setName;
        if (book.timesRead > 0)
            subtitleParts << QStringLiteral("read ×%1").arg(book.timesRead);
        item->setData(BookRowDelegate::kSubtitleRole, subtitleParts.join(QStringLiteral(" · ")));
        if (book.mysteryPrincipleID) {
            const QString name = m_store->principleName(book.mysteryPrincipleID);
            if (!name.isEmpty()) {
                item->setData(BookRowDelegate::kBadgeTextRole,
                              book.difficulty ? QStringLiteral("%1 %2").arg(name).arg(*book.difficulty)
                                              : name);
                item->setData(BookRowDelegate::kBadgeTintRole, m_store->principleColor(book.mysteryPrincipleID));
            }
        }
        // D12: the composite row announces as one crafted element.
        item->setData(Qt::AccessibleTextRole,
                      QStringLiteral("%1. %2.").arg(book.title,
                                                    item->data(BookRowDelegate::kSubtitleRole).toString()));
        if (book.id == previousSelection)
            m_list->setCurrentItem(item);
    }
    m_content->setVisible(!m_store->books().empty());
    m_emptyState->setVisible(m_store->books().empty());
}

void BooksScreen::refreshDetail()
{
    if (const auto book = m_store->selectedBook())
        m_detail->showBook(*book);
    else
        m_detail->clearBook();
}

void BooksScreen::addNew()
{
    BookFormView dialog(m_principles, m_languages, this);
    if (dialog.exec() == QDialog::Accepted)
        m_store->add(dialog.draft());
    if (!m_store->lastError().isEmpty())
        showError();
}

void BooksScreen::selectBook(qint64 bookID)
{
    m_store->setSelectedBookID(bookID);
    rebuildList();
    refreshDetail();
}

void BooksScreen::focusSearch()
{
    m_search->setFocus();
    m_search->selectAll();
}

void BooksScreen::showError()
{
    QMessageBox::warning(this, QStringLiteral("Something went wrong"), m_store->lastError());
    m_store->clearError();
}

// MARK: - BookDetailView

BookDetailView::BookDetailView(BooksStore* store, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_layout = new QVBoxLayout();
    m_layout->setAlignment(Qt::AlignTop);
    outer->addLayout(m_layout);
    clearBook();
}

void BookDetailView::clearBook()
{
    QLayoutItem* child;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }
    m_book.reset();
    auto* empty = new QLabel(QStringLiteral("Select a book from the list to see and edit "
                                             "everything recorded about it."),
                             this);
    empty->setWordWrap(true);
    m_layout->addWidget(empty);
}

void BookDetailView::showBook(const Book& book)
{
    const bool sameBook = m_book && m_book->id == book.id;
    const bool notesPristine = !m_notesDirty;
    m_book = book;

    QLayoutItem* child;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }

    // Header + actions.
    auto* headerRow = new QHBoxLayout();
    auto* title = new QLabel(book.title, this);
    title->setWordWrap(true);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.5);
    titleFont.setBold(true);
    title->setFont(titleFont);
    headerRow->addWidget(title, 1);
    auto* editButton = new QPushButton(QStringLiteral("Edit…"), this);
    headerRow->addWidget(editButton);
    auto* recordButton = new QPushButton(QStringLiteral("Record read…"), this);
    headerRow->addWidget(recordButton);
    m_layout->addLayout(headerRow);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    form->addRow(QStringLiteral("Kind"), new QLabel(orDash(bookKindToString(book.bookKind)), this));
    if (book.difficulty)
        form->addRow(QStringLiteral("Difficulty"), new QLabel(QString::number(*book.difficulty), this));
    else
        form->addRow(QStringLiteral("Difficulty"), new QLabel(QStringLiteral("not recorded"), this));

    QString languageText = QStringLiteral("none / not recorded");
    if (const QString name = m_store->languageName(book.languageID); !name.isEmpty()) {
        const auto known = m_store->isLanguageKnown(book.languageID);
        languageText = known.has_value() ? QStringLiteral("%1 — %2").arg(name, *known ? QStringLiteral("known")
                                                                                            : QStringLiteral("NOT known"))
                                         : name;
    }
    form->addRow(QStringLiteral("Language"), new QLabel(languageText, this));
    form->addRow(QStringLiteral("Set"), new QLabel(orDash(book.setName.value_or(QString())), this));
    form->addRow(QStringLiteral("Volume"), new QLabel(orDash(book.volume.value_or(QString())), this));
    form->addRow(QStringLiteral("Location"), new QLabel(orDash(book.location.value_or(QString())), this));
    form->addRow(QStringLiteral("Contamination"), new QLabel(contaminationLabel(book.contamination), this));
    const QStringList lessons = m_store->lessonSkillNames(book.id);
    if (!lessons.isEmpty())
        form->addRow(QStringLiteral("Lessons teach"),
                     new QLabel(lessons.join(QStringLiteral(", ")), this));
    m_layout->addLayout(form);

    auto* readingTitle = new QLabel(QStringLiteral("Reading"), this);
    readingTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
    m_layout->addWidget(readingTitle);
    auto* readingForm = new QFormLayout();
    readingForm->setLabelAlignment(Qt::AlignRight);

    // Read status as a direct combo (immediate persistence — setReadStatus).
    auto* statusCombo = new QComboBox(this);
    for (ReadStatus status : {ReadStatus::Uncatalogued, ReadStatus::Catalogued, ReadStatus::Mastered}) {
        statusCombo->addItem(readStatusLabel(status), int(status));
        if (book.readStatus == status)
            statusCombo->setCurrentIndex(statusCombo->count() - 1);
    }
    connect(statusCombo, &QComboBox::activated, this, [this, statusCombo](int index) {
        if (!m_book)
            return;
        m_store->setReadStatus(*m_book, ReadStatus(statusCombo->itemData(index).toInt()));
    });
    readingForm->addRow(QStringLiteral("Status"), statusCombo);
    readingForm->addRow(QStringLiteral("Times read"),
                        new QLabel(book.timesRead == 0 ? QStringLiteral("never")
                                                       : QString::number(book.timesRead),
                                   this));
    readingForm->addRow(QStringLiteral("First read"),
                        new QLabel(orDash(book.firstReadAt.value_or(QString())), this));
    readingForm->addRow(QStringLiteral("Last read"),
                        new QLabel(orDash(book.lastReadAt.value_or(QString())), this));
    readingForm->addRow(QStringLiteral("Lessons granted"),
                        new QLabel(book.lessons ? QString::number(*book.lessons) : QStringLiteral("—"),
                                   this));
    m_layout->addLayout(readingForm);

    // Yields (spoiler posture: the yield name is only revealed by mastery).
    auto* yieldsTitle = new QLabel(QStringLiteral("Yields"), this);
    yieldsTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
    m_layout->addWidget(yieldsTitle);
    if (book.yieldedMemoryID) {
        if (book.readStatus == ReadStatus::Mastered) {
            const QString name = m_store->memoryName(book.yieldedMemoryID);
            auto* link = new QLabel(QStringLiteral("<a href=\"memory:%1\">%2</a>")
                                        .arg(*book.yieldedMemoryID)
                                        .arg(name.toHtmlEscaped()),
                                    this);
            link->setTextFormat(Qt::RichText);
            link->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard | Qt::LinksAccessibleByMouse);
            link->setOpenExternalLinks(false);
            connect(link, &QLabel::linkActivated, this,
                    [this](const QString&) { emit showMemoryRequested(*m_book->yieldedMemoryID); });
            auto* row = new QHBoxLayout();
            row->addWidget(new QLabel(QStringLiteral("Memory"), this));
            row->addWidget(link);
            row->addStretch(1);
            m_layout->addLayout(row);
        } else {
            m_layout->addWidget(new QLabel(QStringLiteral("revealed by mastering the book"), this));
        }
    } else if (book.readStatus == ReadStatus::Mastered) {
        m_layout->addWidget(new QLabel(QStringLiteral("Not recorded — every read of this book "
                                                       "gives the same memory; use “Record read…” to note it."),
                                       this));
    } else {
        m_layout->addWidget(new QLabel(QStringLiteral("Mastering this book is what teaches you its yield."), this));
    }

    // Journal (live cache, newest first).
    auto* journalTitle = new QLabel(QStringLiteral("Journal"), this);
    journalTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
    m_layout->addWidget(journalTitle);
    const auto entries = m_store->journalEntries(book.id);
    if (entries.empty()) {
        m_layout->addWidget(new QLabel(QStringLiteral("No entries link this book yet."), this));
    } else {
        for (const JournalEntry& entry : entries) {
            auto* entryLabel = new QLabel(QStringLiteral("• %1").arg(entry.entry.toHtmlEscaped()), this);
            entryLabel->setWordWrap(true);
            entryLabel->setTextFormat(Qt::RichText);
            m_layout->addWidget(entryLabel);
            QString caption = entry.loggedAt;
            if (entry.gameDay)
                caption += QStringLiteral(" · %1").arg(*entry.gameDay);
            auto* captionLabel = new QLabel(caption, this);
            captionLabel->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 11px;"));
            m_layout->addWidget(captionLabel);
        }
    }

    // Notes: pane-local editor + explicit Save/Revert, with guarded re-seed
    // (an Edit…-sheet save + a later "Save note" must not clobber fresh notes).
    auto* notesTitle = new QLabel(QStringLiteral("Notes"), this);
    notesTitle->setStyleSheet(QStringLiteral("color: palette(mid); font-weight: bold;"));
    m_layout->addWidget(notesTitle);
    m_notesEditor = new QPlainTextEdit(this);
    m_notesEditor->setAccessibleName(QStringLiteral("Book notes"));
    if (!sameBook || notesPristine) {
        const QString noteText = book.notes.value_or(QString());
        if (m_notesEditor->toPlainText() != noteText)
            m_notesEditor->setPlainText(noteText);
    }
    m_notesDirty = false;
    m_notesEditor->setMinimumHeight(64);
    connect(m_notesEditor, &QPlainTextEdit::textChanged, this, [this] { m_notesDirty = true; });
    m_layout->addWidget(m_notesEditor);
    auto* notesRow = new QHBoxLayout();
    auto* saveNote = new QPushButton(QStringLiteral("Save note"), this);
    connect(saveNote, &QPushButton::clicked, this, [this] { saveNoteClicked(); });
    auto* revertNote = new QPushButton(QStringLiteral("Revert"), this);
    connect(revertNote, &QPushButton::clicked, this, [this] {
        if (!m_book)
            return;
        m_notesEditor->setPlainText(m_book->notes.value_or(QString()));
        m_notesDirty = false;
    });
    notesRow->addWidget(saveNote);
    notesRow->addWidget(revertNote);
    notesRow->addStretch(1);
    m_layout->addLayout(notesRow);

    auto* deleteRow = new QHBoxLayout();
    auto* deleteButton = new QPushButton(QStringLiteral("Delete Book…"), this);
    deleteButton->setStyleSheet(QStringLiteral("color: #C0392B;"));
    connect(deleteButton, &QPushButton::clicked, this, [this] {
        if (!m_book)
            return;
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this, QStringLiteral("Delete Book"),
            QStringLiteral("Delete “%1”? Journal entries keep their text but lose the link "
                           "to this book.")
                .arg(m_book->title));
        if (answer == QMessageBox::Yes)
            emit deleteRequested(*m_book);
    });
    deleteRow->addWidget(deleteButton);
    deleteRow->addStretch(1);
    m_layout->addLayout(deleteRow);

    connect(editButton, &QPushButton::clicked, this, [this] {
        if (m_book)
            emit editRequested(*m_book);
    });
    connect(recordButton, &QPushButton::clicked, this, [this] {
        if (m_book)
            emit markReadRequested(*m_book); // the record-read dialog lands in Task 12
    });
}

void BookDetailView::saveNoteClicked()
{
    if (!m_book)
        return;
    m_store->updateNotes(*m_book, m_notesEditor->toPlainText());
    m_notesDirty = false;
}

void BookDetailView::setActionsEnabled(bool)
{
    // no-op for now; dialogs enable their own widgets
}

// MARK: - BookFormView

BookFormView::BookFormView(const std::vector<Principle>& principles,
                           const std::vector<Language>& languages, QWidget* parent)
    : QDialog(parent)
    , m_principles(principles)
    , m_languages(languages)
{
    setWindowTitle(QStringLiteral("Add Book"));
    build();
}

BookFormView::BookFormView(const Book& book, const std::vector<Principle>& principles,
                           const std::vector<Language>& languages, QWidget* parent)
    : QDialog(parent)
    , m_principles(principles)
    , m_languages(languages)
    , m_original(book)
{
    setWindowTitle(QStringLiteral("Edit Book"));
    build();
}

void BookFormView::build()
{
    auto* form = new QFormLayout(this);
    m_title = new QLineEdit(this);
    form->addRow(QStringLiteral("Title"), m_title);

    m_kind = new QComboBox(this);
    for (BookKind kind : {BookKind::Book, BookKind::Scroll, BookKind::Film, BookKind::Record})
        m_kind->addItem(bookKindToString(kind), int(kind));
    form->addRow(QStringLiteral("Kind"), m_kind);

    m_setName = new QLineEdit(this);
    m_setName->setPlaceholderText(QStringLiteral("Series / set (optional)"));
    form->addRow(QStringLiteral("Set"), m_setName);
    m_volume = new QLineEdit(this);
    m_volume->setPlaceholderText(QStringLiteral("Volume / edition (optional)"));
    form->addRow(QStringLiteral("Volume"), m_volume);
    m_location = new QLineEdit(this);
    m_location->setPlaceholderText(QStringLiteral("Room, shelf… (optional)"));
    form->addRow(QStringLiteral("Location"), m_location);

    m_mysteryPrinciple = new QComboBox(this);
    m_mysteryPrinciple->addItem(QStringLiteral("—"), 0);
    for (const Principle& principle : m_principles)
        m_mysteryPrinciple->addItem(principle.name, qint64(principle.id));
    form->addRow(QStringLiteral("Mystery principle"), m_mysteryPrinciple);
    m_difficulty = new QSpinBox(this);
    m_difficulty->setRange(0, 25);
    m_difficulty->setSpecialValueText(QStringLiteral("—"));
    form->addRow(QStringLiteral("Difficulty"), m_difficulty);
    connect(m_mysteryPrinciple, &QComboBox::currentIndexChanged, this, [this](int index) {
        // A difficulty without a principle is recordable (D10) — keep the spin
        // enabled either way, but surface the relationship in the placeholder.
        m_difficulty->setEnabled(true);
    });

    m_language = new QComboBox(this);
    m_language->addItem(QStringLiteral("—"), 0);
    for (const Language& language : m_languages)
        m_language->addItem(language.name, qint64(language.id));
    form->addRow(QStringLiteral("Language"), m_language);

    m_readStatus = new QComboBox(this);
    for (ReadStatus status : {ReadStatus::Uncatalogued, ReadStatus::Catalogued, ReadStatus::Mastered})
        m_readStatus->addItem(readStatusLabel(status), int(status));
    form->addRow(QStringLiteral("Read status"), m_readStatus);

    m_contamination = new QComboBox(this);
    m_contamination->addItem(QStringLiteral("—"), 0);
    m_contamination->addItem(QStringLiteral("checked clean"), int(Contamination::None));
    m_contamination->addItem(QStringLiteral("Curse"), int(Contamination::Curse));
    m_contamination->addItem(QStringLiteral("Theoplasm"), int(Contamination::Theoplasm));
    m_contamination->addItem(QStringLiteral("Infestation"), int(Contamination::Infestation));
    m_contamination->addItem(QStringLiteral("Corruption"), int(Contamination::Corruption));
    m_contamination->addItem(QStringLiteral("Winkwell"), int(Contamination::Winkwell));
    m_contamination->addItem(QStringLiteral("Witchworms"), int(Contamination::Witchworms));
    form->addRow(QStringLiteral("Contamination"), m_contamination);

    m_lessons = new QComboBox(this);
    m_lessons->addItem(QStringLiteral("—"), 0);
    for (int lessons : {1, 2, 3})
        m_lessons->addItem(QString::number(lessons), lessons);
    form->addRow(QStringLiteral("Lessons granted"), m_lessons);

    m_notes = new QPlainTextEdit(this);
    m_notes->setMinimumHeight(56);
    form->addRow(QStringLiteral("Notes"), m_notes);

    auto* buttons = new QHBoxLayout();
    auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* save = new QPushButton(QStringLiteral("Save"), this);
    save->setDefault(true);
    connect(save, &QPushButton::clicked, this, [this] {
        if (m_title->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Add Book"),
                                 QStringLiteral("A title is the one required field."));
            return;
        }
        accept();
    });
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    form->addRow(buttons);

    if (m_original) {
        const Book& book = *m_original;
        m_title->setText(book.title);
        m_kind->setCurrentIndex(int(book.bookKind));
        m_setName->setText(book.setName.value_or(QString()));
        m_volume->setText(book.volume.value_or(QString()));
        m_location->setText(book.location.value_or(QString()));
        m_mysteryPrinciple->setCurrentIndex(book.mysteryPrincipleID
                                                ? m_mysteryPrinciple->findData(qint64(*book.mysteryPrincipleID))
                                                : 0);
        m_difficulty->setValue(book.difficulty.value_or(0));
        m_language->setCurrentIndex(book.languageID ? m_language->findData(qint64(*book.languageID)) : 0);
        m_readStatus->setCurrentIndex(int(book.readStatus));
        m_contamination->setCurrentIndex(book.contamination ? m_contamination->findData(int(*book.contamination)) + 1
                                                            : 0);
        m_lessons->setCurrentIndex(book.lessons ? m_lessons->findData(*book.lessons) : 0);
        m_notes->setPlainText(book.notes.value_or(QString()));
    }
}

BookDraft BookFormView::draft() const
{
    BookDraft draft;
    draft.title = m_title->text().trimmed();
    draft.bookKind = BookKind(m_kind->currentData().toInt());
    if (!m_setName->text().trimmed().isEmpty())
        draft.setName = m_setName->text().trimmed();
    if (!m_volume->text().trimmed().isEmpty())
        draft.volume = m_volume->text().trimmed();
    if (!m_location->text().trimmed().isEmpty())
        draft.location = m_location->text().trimmed();
    if (m_mysteryPrinciple->currentData().toInt() != 0)
        draft.mysteryPrincipleID = m_mysteryPrinciple->currentData().toLongLong();
    if (m_difficulty->value() > 0)
        draft.difficulty = m_difficulty->value();
    if (m_language->currentData().toInt() != 0)
        draft.languageID = m_language->currentData().toLongLong();
    draft.readStatus = ReadStatus(m_readStatus->currentData().toInt());
    if (m_contamination->currentData().toInt() != 0)
        draft.contamination = Contamination(m_contamination->currentData().toInt());
    if (m_lessons->currentData().toInt() != 0)
        draft.lessons = m_lessons->currentData().toInt();
    if (!m_notes->toPlainText().trimmed().isEmpty())
        draft.notes = m_notes->toPlainText().trimmed();
    return draft;
}

} // namespace boh
