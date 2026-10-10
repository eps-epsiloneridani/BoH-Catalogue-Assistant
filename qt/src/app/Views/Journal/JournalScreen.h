// Plan Task 15: the Journal — reverse-chronological findings with in-game day
// headers, a quick-capture box present in the EMPTY state too (Ctrl+Shift+J),
// entity-link chips, edit and delete.
#pragma once

#include "Stores/JournalStore.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QVBoxLayout;

namespace boh {

class JournalScreen final : public QWidget {
    Q_OBJECT

public:
    JournalScreen(JournalStore* store, QWidget* parent = nullptr);

    void focusQuickAdd(); // Ctrl+Shift+J
    void setStore(JournalStore* store);

signals:
    void showBookRequested(qint64 bookID);
    void showMemoryRequested(qint64 memoryID);
    void showSkillRequested(qint64 skillID);

private:
    void rebuildRows();
    void refreshEmptyState();
    void submitQuickAdd();
    void editEntry(const JournalEntry& entry);
    void showError();

    JournalStore* m_store;
    QLineEdit* m_search = nullptr;
    QLineEdit* m_quickAdd = nullptr;
    QLineEdit* m_quickDay = nullptr;
    QWidget* m_content = nullptr;
    QVBoxLayout* m_rowsLayout = nullptr;
    QWidget* m_emptyState = nullptr;
    QWidget* m_rowsHost = nullptr;
};

} // namespace boh
