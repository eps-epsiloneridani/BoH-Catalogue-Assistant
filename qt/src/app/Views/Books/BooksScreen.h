// Plan Task 11: the Books screen — master list (search, filter, sort, add) on
// the left, the selected book's everything on the right.
#pragma once

#include "Models.h"
#include "Stores/BooksStore.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QToolButton;

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

namespace boh {

class AppController;
class BookDetailView;
class BookFormView;

class BooksScreen final : public QWidget {
    Q_OBJECT

public:
    BooksScreen(BooksStore* store, const std::vector<Principle>& principles,
                const std::vector<Language>& languages, QWidget* parent = nullptr);

    void addNew();     // Ctrl+N
    void focusSearch(); // Ctrl+F
    void selectBook(qint64 bookID); // backlink navigation
    /// The controller rebuilds stores on playthrough switch — re-point at the
    /// new instance (connections to the old store die with it).
    void setStore(BooksStore* store);
    void setController(AppController* controller);

signals:
    /// Clickable yield backlink → the app switches to the Memories section.
    void showMemoryRequested(qint64 memoryID);

private:
    void rebuildList();
    void refreshDetail();
    void refreshFilterMenu();
    void showError();

    std::vector<Principle> m_principles;
    std::vector<Language> m_languages;
    AppController* m_controller = nullptr;

    BooksStore* m_store;
    QLineEdit* m_search = nullptr;
    QToolButton* m_filterButton = nullptr;
    QToolButton* m_sortButton = nullptr;
    QListWidget* m_list = nullptr;
    BookDetailView* m_detail = nullptr;
    QWidget* m_emptyState = nullptr;
    QWidget* m_content = nullptr;
};

// Plan Task 11: the detail pane — everything recorded about one book, read
// from the store's live caches. Notes stay pane-local (explicit save flow,
// guarded re-seed when the editor is pristine — the macOS remediation-2 fix).
class BookDetailView final : public QWidget {
    Q_OBJECT

public:
    explicit BookDetailView(BooksStore* store, QWidget* parent = nullptr);

    void showBook(const Book& book);
    void clearBook();
    void setActionsEnabled(bool enabled); // actions land with their dialogs/tasks

signals:
    void editRequested(const boh::Book& book);
    void markReadRequested(const boh::Book& book);
    void deleteRequested(const boh::Book& book);
    void showMemoryRequested(qint64 memoryID);

private:
    void rebuild(const Book& book);
    void saveNoteClicked();

    BooksStore* m_store;
    QVBoxLayout* m_layout = nullptr;
    std::optional<Book> m_book;
    QPlainTextEdit* m_notesEditor = nullptr;
    bool m_notesDirty = false;
};

// Plan Task 11: the add/edit dialog — deliberately quick (title + mystery is
// enough to be useful); saving goes through the store (form master = a read).
class BookFormView final : public QDialog {
    Q_OBJECT

public:
    BookFormView(const std::vector<Principle>& principles,
                 const std::vector<Language>& languages, QWidget* parent = nullptr);
    BookFormView(const Book& book, const std::vector<Principle>& principles,
                 const std::vector<Language>& languages, QWidget* parent = nullptr);

    BookDraft draft() const;

private:
    void build();

    std::vector<Principle> m_principles;
    std::vector<Language> m_languages;
    AppController* m_controller = nullptr;
    std::optional<Book> m_original;
    QLineEdit* m_title = nullptr;
    QComboBox* m_kind = nullptr;
    QLineEdit* m_setName = nullptr;
    QLineEdit* m_volume = nullptr;
    QLineEdit* m_location = nullptr;
    QComboBox* m_mysteryPrinciple = nullptr;
    QSpinBox* m_difficulty = nullptr;
    QComboBox* m_language = nullptr;
    QComboBox* m_readStatus = nullptr;
    QComboBox* m_contamination = nullptr;
    QComboBox* m_lessons = nullptr;
    QPlainTextEdit* m_notes = nullptr;
};

} // namespace boh
