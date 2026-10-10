// Plan Task 16: the playthroughs manager + import sheet.
// Manager: left-justified rows; renames commit on Return, focus-out AND close
// (three commit paths — the macOS click-OK bug); blank restores the stored
// name; the active/last run can't be deleted.
// Import sheet: standard-path scan, day-one folder picker (Linux has no single
// standard layout), destination = new or current playthrough, summary report.
#pragma once

#include "Models.h"
#include "SaveImport.h"

#include <QDialog>
#include <QWidget>

#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QVBoxLayout;

namespace boh {

class AppController;

/// "New Playthrough…": name + notes → create & switch.
class NewPlaythroughDialog final : public QDialog {
    Q_OBJECT

public:
    explicit NewPlaythroughDialog(int existingCount, QWidget* parent = nullptr);

    QString playthroughName() const;
    QString notes() const;

private:
    QLineEdit* m_name = nullptr;
    QLineEdit* m_notes = nullptr;
    int m_existingCount = 0;
};

/// "Manage Playthroughs…": rename / load / delete with confirmations.
class ManagePlaythroughsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ManagePlaythroughsDialog(AppController* controller, QWidget* parent = nullptr);

private:
    void rebuildRows();
    void commitAllRenames();

    struct RowWidgets {
        qint64 id = 0;
        class QLineEdit* name = nullptr;
    };
    AppController* m_controller;
    QVBoxLayout* m_rowsLayout = nullptr;
    std::vector<RowWidgets> m_rows;
};

/// "Import from Save…".
class ImportFromSaveDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ImportFromSaveDialog(AppController* controller, QWidget* parent = nullptr);

private:
    void scan();
    void runImport();

    AppController* m_controller;
    QListWidget* m_saves = nullptr;
    std::vector<SaveGameSummary> m_summaries;
    QLabel* m_status = nullptr;
    QLabel* m_report = nullptr;
    QComboBox* m_destination = nullptr;
    QLineEdit* m_newName = nullptr;
    QPushButton* m_importButton = nullptr;
    QString m_folderNote;
};

} // namespace boh
