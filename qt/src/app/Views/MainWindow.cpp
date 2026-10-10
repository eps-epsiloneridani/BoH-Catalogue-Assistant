#include "MainWindow.h"
#include "Sidebar.h"

#include <QAction>
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QVBoxLayout>

namespace boh {

namespace {
constexpr int kSectionCount = 5;

QString sectionTitle(int index)
{
    static const QStringList titles = {
        QStringLiteral("Books"), QStringLiteral("Memories"), QStringLiteral("Skills"),
        QStringLiteral("Journal"), QStringLiteral("Reading Helper"),
    };
    return titles.value(index);
}
} // namespace

MainWindow::MainWindow(AppController& controller, QWidget* parent)
    : QMainWindow(parent)
    , m_controller(controller)
{
    setWindowTitle(QStringLiteral("BoH Librarian"));
    resize(1100, 720);
    buildBody();
    buildMenus();
    refreshPlaythroughs();
    refreshFooter();

    connect(&m_controller, &AppController::changed, this, &MainWindow::refreshFooter);
    connect(&m_controller, &AppController::playthroughsChanged, this, [this] {
        refreshPlaythroughs();
        refreshFooter();
    });
    selectSection(0);
}

void MainWindow::buildBody()
{
    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_sidebar = new Sidebar(central);
    m_sidebar->setFixedWidth(210);
    layout->addWidget(m_sidebar);

    m_sections = new QStackedWidget(central);
    for (int i = 0; i < kSectionCount; ++i) {
        auto* placeholder = new QLabel(
            QStringLiteral("%1 — arrives with its screen (plan Tasks 11–16).")
                .arg(sectionTitle(i)),
            m_sections);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setAccessibleName(sectionTitle(i));
        m_sections->addWidget(placeholder);
    }
    layout->addWidget(m_sections, 1);
    setCentralWidget(central);

    // Status footer: playthrough + db path left, schema/counts right.
    m_footerLeft = new QLabel(this);
    m_footerRight = new QLabel(this);
    statusBar()->addWidget(m_footerLeft);
    statusBar()->addPermanentWidget(m_footerRight);

    connect(m_sidebar, &Sidebar::sectionSelected, this, &MainWindow::selectSection);
    connect(m_sidebar, &Sidebar::playthroughSelected, this, [this](int index) {
        const auto& playthroughs = m_controller.playthroughs();
        if (index >= 0 && index < int(playthroughs.size()))
            m_controller.switchPlaythrough(playthroughs[size_t(index)].id);
    });
}

void MainWindow::buildMenus()
{
    QMenu* goMenu = menuBar()->addMenu(QStringLiteral("&Go"));
    const QStringList keys = {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                              QStringLiteral("4"), QStringLiteral("5")};
    for (int i = 0; i < kSectionCount; ++i) {
        QAction* action = goMenu->addAction(sectionTitle(i));
        action->setShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(keys.at(i))));
        connect(action, &QAction::triggered, this, [this, i] { selectSection(i); });
    }
    goMenu->addSeparator();
    QAction* helper = goMenu->addAction(QStringLiteral("Reading Helper"));
    helper->setShortcut(QKeySequence::fromString(QStringLiteral("Ctrl+R")));
    connect(helper, &QAction::triggered, this, [this] { selectSection(4); });

    QAction* quickJournal = goMenu->addAction(QStringLiteral("Quick journal entry"));
    quickJournal->setShortcut(QKeySequence::fromString(QStringLiteral("Ctrl+Shift+J")));
    connect(quickJournal, &QAction::triggered, this, [this] {
        selectSection(3); // the quick-capture field focuses from Task 15
    });

    QAction* newAction = goMenu->addAction(QStringLiteral("New in current section"));
    newAction->setShortcut(QKeySequence::fromString(QStringLiteral("Ctrl+N")));
    connect(newAction, &QAction::triggered, this, [this] {
        // Screens wire their add-dialogs to this from Tasks 11–15.
        statusBar()->showMessage(QStringLiteral("New “%1” arrives with its screen.")
                                     .arg(sectionTitle(m_currentSection)),
                                 4000);
    });

    QAction* find = goMenu->addAction(QStringLiteral("Focus search"));
    find->setShortcut(QKeySequence::fromString(QStringLiteral("Ctrl+F")));
    connect(find, &QAction::triggered, this, [this] { focusSearch(); });
}

void MainWindow::selectSection(int index)
{
    if (index < 0 || index >= kSectionCount)
        return;
    m_currentSection = index;
    m_sections->setCurrentIndex(index);
    m_sidebar->setCurrentSection(index);
}

void MainWindow::focusSearch()
{
    // The search fields arrive with the Books/Memories/Skills screens.
    statusBar()->showMessage(QStringLiteral("Search arrives with its screen."), 4000);
}

void MainWindow::refreshPlaythroughs()
{
    QStringList names;
    int activeIndex = 0;
    const auto& playthroughs = m_controller.playthroughs();
    for (size_t i = 0; i < playthroughs.size(); ++i) {
        names << playthroughs[i].name;
        if (m_controller.activePlaythrough()
            && playthroughs[i].id == m_controller.activePlaythrough()->id)
            activeIndex = int(i);
    }
    m_sidebar->setPlaythroughs(names, activeIndex);
}

void MainWindow::refreshFooter()
{
    const auto counts = m_controller.counts();
    QString left = QStringLiteral("%1 · %2")
                       .arg(m_controller.activePlaythrough()
                                ? m_controller.activePlaythrough()->name
                                : QStringLiteral("—"),
                            m_controller.dbPath());
    left = QFontMetrics(font()).elidedText(left, Qt::ElideMiddle, 620);
    m_footerLeft->setText(left);
    m_footerRight->setText(
        QStringLiteral("schema v%1 · %2 principles · %3 languages · %4 books · %5 memories · "
                       "%6 skills · %7 journal entries")
            .arg(m_controller.schemaVersion())
            .arg(m_controller.principles().size())
            .arg(m_controller.languages().size())
            .arg(counts.books)
            .arg(counts.memories)
            .arg(counts.skills)
            .arg(counts.journal));
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    // Window activation reloads from the db — the macOS onAppear equivalent and
    // half of the reload choke point (writes are the other half).
    if (event->type() == QEvent::ActivationChange && isActiveWindow())
        m_controller.reloadAll();
}

} // namespace boh
