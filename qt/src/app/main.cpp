// BoH Librarian — Qt port entry point (plan Task 9): bootstrap the controller,
// show the main window, fail loud when the db can't open.
#include "AppController.h"
#include "Views/MainWindow.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("BoH Librarian"));

    boh::AppController controller;
    controller.bootstrap();
    if (controller.phase() == boh::AppController::Phase::Failed) {
        QMessageBox::critical(nullptr, QStringLiteral("BoH Librarian"),
                              QStringLiteral("The library failed to open:\n\n%1")
                                  .arg(controller.failureMessage()));
        return 1;
    }

    boh::MainWindow window(controller);
    window.show();
    return QApplication::exec();
}
