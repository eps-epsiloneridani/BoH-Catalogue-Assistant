// BoH Librarian — Qt port entry point. Plan Task 1: an empty main window.
// Sidebar navigation, stores, and db bootstrap arrive in Tasks 9–10.
#include <QApplication>
#include <QMainWindow>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("BoH Librarian"));

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("BoH Librarian"));
    window.resize(1100, 720);
    window.show();
    return QApplication::exec();
}
