// Plan Task 9: the main window — sidebar + section stack + status footer +
// shortcuts (Ctrl+1…5, Ctrl+R, Ctrl+N, Ctrl+F, Ctrl+Shift+J). Screens arrive
// in Tasks 11–16; sections show empty-state placeholders until then.
#pragma once

#include "AppController.h"

#include <QMainWindow>
#include <QStackedWidget>

class QLabel;

namespace boh {

class Sidebar;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(AppController& controller, QWidget* parent = nullptr);

    /// Ctrl+N in the current section (screens wire this up from Task 11 on).
    void focusSearch(); // Ctrl+F — search field arrives with the Books screen

protected:
    void changeEvent(QEvent* event) override; // window activation → reloadAll()

private:
    void buildMenus();
    void buildBody();
    void refreshFooter();
    void refreshPlaythroughs();
    void selectSection(int index);

    AppController& m_controller;
    class BooksScreen* m_booksScreen = nullptr;
    class MemoriesScreen* m_memoriesScreen = nullptr;
    class ReadingHelperScreen* m_helperScreen = nullptr;
    Sidebar* m_sidebar = nullptr;
    QStackedWidget* m_sections = nullptr;
    QLabel* m_footerLeft = nullptr;
    QLabel* m_footerRight = nullptr;
    int m_currentSection = 0;
};

} // namespace boh
