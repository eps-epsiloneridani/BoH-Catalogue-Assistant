// Plan Task 13: the Memories screen — earned-only list (search, principle/level
// filter, sort), display-only detail (badges, sources, mastered-only backlinks —
// all store-cache-fed), and the edit dialog (aspects + sources + yielding links).
#pragma once

#include "Models.h"
#include "Stores/MemoriesStore.h"

#include <QDialog>
#include <QSet>
#include <QWidget>

#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;
class QToolButton;
class QVBoxLayout;

namespace boh {

class AppController;

class MemoriesScreen final : public QWidget {
    Q_OBJECT

public:
    MemoriesScreen(MemoriesStore* store, const std::vector<Principle>& principles,
                   AppController* controller, QWidget* parent = nullptr);

    void addNew();      // Ctrl+N
    void focusSearch(); // Ctrl+F
    void setStore(MemoriesStore* store);
    void selectMemory(qint64 id); // clickable backlinks from other sections

signals:
    /// Backlink navigation: a yielding book → the Books section with it selected.
    void showBookRequested(qint64 bookID);

private:
    void rebuildList();
    void refreshDetail();
    void refreshMenus();
    void showError();
    void addNewRequested();

    MemoriesStore* m_store;
    std::vector<Principle> m_principles;
    AppController* m_controller;
    QLineEdit* m_search = nullptr;
    QToolButton* m_principleButton = nullptr;
    QToolButton* m_levelButton = nullptr;
    QToolButton* m_sortButton = nullptr;
    QListWidget* m_list = nullptr;
    QWidget* m_detailHost = nullptr;
    QVBoxLayout* m_detailLayout = nullptr;
    QWidget* m_emptyState = nullptr;
    QWidget* m_content = nullptr;
};

class MemoryDetailView final : public QWidget {
    Q_OBJECT

public:
    explicit MemoryDetailView(MemoriesStore* store, QWidget* parent = nullptr);

    void showMemory(const Memory& memory);
    void clearMemory();

signals:
    void editRequested(const boh::Memory& memory);
    void deleteRequested(const boh::Memory& memory);
    void showBookRequested(qint64 bookID);

private:
    void rebuild(const Memory& memory);
    void saveNote();

    MemoriesStore* m_store;
    QVBoxLayout* m_layout = nullptr;
    std::optional<Memory> m_memory;
    QPlainTextEdit* m_notesEditor = nullptr;
    bool m_notesDirty = false;
};

class MemoryFormView final : public QDialog {
    Q_OBJECT

public:
    explicit MemoryFormView(const std::vector<Principle>& principles, QWidget* parent = nullptr);
    MemoryFormView(const Memory& memory, const std::vector<MemorySource>& sources,
                   const std::vector<BookRef>& yielding, const std::vector<BookRef>& bookLinks,
                   const std::vector<Principle>& principles, QWidget* parent = nullptr);

    MemoryDraft draft() const;
    std::vector<MemorySource> sources() const;
    std::vector<qint64> yieldingBookIDs() const;

private:
    void build();
    void buildSourceRows();  // edit mode only

    std::vector<Principle> m_principles;
    std::optional<Memory> m_original;
    QLineEdit* m_name = nullptr;
    QComboBox* m_kind = nullptr;
    QCheckBox* m_persistent = nullptr;
    QWidget* m_aspectsEditor = nullptr;  // AspectEditor
    QPlainTextEdit* m_notes = nullptr;
    // Edit-mode editors: sources + yielding links (plain blocks, not Form rows).
    QVBoxLayout* m_sourceRowsLayout = nullptr;
    struct SourceRow {
        QComboBox* kind = nullptr;
        QLineEdit* detail = nullptr;
        QString kindValue = QStringLiteral("first read");
        QString detailValue;
    };
    std::vector<SourceRow> m_sourceRows;
    QListWidget* m_linksList = nullptr;
    std::vector<BookRef> m_linkCandidates;
    QSet<qint64> m_currentLinkIDs;
};

} // namespace boh
