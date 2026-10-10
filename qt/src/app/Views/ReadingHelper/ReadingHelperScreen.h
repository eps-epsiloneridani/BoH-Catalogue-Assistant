// Plan Task 14: the Reading Helper — pick the book you're about to read; the
// helper works out what it demands (requirement sentence + reach) and what
// you've recorded that could meet it (earned memory candidates, skill desk
// math). Mastering flows open the record-read dialog with the memory in hand.
#pragma once

#include "Stores/ReadingHelperStore.h"

#include <QWidget>

#include <vector>

class QLabel;
class QLineEdit;
class QListWidget;
class QToolButton;
class QVBoxLayout;

namespace boh {

class AppController;

class ReadingHelperScreen final : public QWidget {
    Q_OBJECT

public:
    ReadingHelperScreen(ReadingHelperStore* store, AppController* controller,
                        const std::vector<Principle>& principles, QWidget* parent = nullptr);

    void focusSearch(); // Ctrl+F
    void setStore(ReadingHelperStore* store);
    void selectBook(qint64 bookID); // backlink navigation

signals:
    void showMemoryRequested(qint64 memoryID);
    void showBookRequested(qint64 bookID);

private:
    void rebuildPicker();
    void refreshPanel();
    void refreshMenus();
    void recordRead(const Book& book, qint64 preselectedMemoryID);
    void showError();

    ReadingHelperStore* m_store;
    AppController* m_controller;
    std::vector<Principle> m_principles;
    QLineEdit* m_search = nullptr;
    QToolButton* m_filterButton = nullptr;
    QToolButton* m_mysteryButton = nullptr;
    QToolButton* m_sortButton = nullptr;
    QListWidget* m_picker = nullptr;
    QWidget* m_panelHost = nullptr;
    QVBoxLayout* m_panelLayout = nullptr;
    QWidget* m_emptyState = nullptr;
    QWidget* m_content = nullptr;
};

} // namespace boh
