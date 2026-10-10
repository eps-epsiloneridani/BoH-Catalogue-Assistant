#include "PlaythroughsDialog.h"

#include "AppController.h"
#include "SQLiteDatabase.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace boh {

namespace {
/// Restores an env var on scope exit (the folder-picker scan override).
class ScopedEnv {
public:
    ScopedEnv(const char* key, const QString& value)
        : m_key(key), m_old(qEnvironmentVariable(key))
    {
        qputenv(key, value.toUtf8());
    }
    ~ScopedEnv()
    {
        if (m_old.isEmpty())
            qunsetenv(m_key);
        else
            qputenv(m_key, m_old.toUtf8());
    }
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;

private:
    const char* m_key;
    QString m_old;
};
} // namespace

// MARK: - NewPlaythroughDialog

NewPlaythroughDialog::NewPlaythroughDialog(int existingCount, QWidget* parent)
    : QDialog(parent)
    , m_existingCount(existingCount)
{
    setWindowTitle(QStringLiteral("New Playthrough"));
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(QStringLiteral("Playthrough %1").arg(existingCount + 1));
    form->addRow(QStringLiteral("Name"), m_name);
    m_notes = new QLineEdit(this);
    m_notes->setPlaceholderText(QStringLiteral("optional"));
    form->addRow(QStringLiteral("Notes"), m_notes);
    layout->addLayout(form);
    auto* buttons = new QHBoxLayout();
    auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* create = new QPushButton(QStringLiteral("Create & Switch"), this);
    create->setDefault(true);
    connect(create, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(create);
    layout->addLayout(buttons);
}

QString NewPlaythroughDialog::playthroughName() const
{
    return m_name->text().trimmed();
}

QString NewPlaythroughDialog::notes() const
{
    return m_notes->text().trimmed();
}

// MARK: - ManagePlaythroughsDialog

ManagePlaythroughsDialog::ManagePlaythroughsDialog(AppController* controller, QWidget* parent)
    : QDialog(parent)
    , m_controller(controller)
{
    setWindowTitle(QStringLiteral("Manage Playthroughs"));
    setMinimumWidth(560);
    auto* layout = new QVBoxLayout(this);

    m_rowsLayout = new QVBoxLayout();
    m_rowsLayout->setSpacing(6);
    layout->addLayout(m_rowsLayout);
    rebuildRows();

    layout->addWidget(new QLabel(
        QStringLiteral("Deleting removes every book, memory, skill and journal entry recorded in "
                       "it — permanently. The active playthrough can't be deleted. Changes "
                       "(rename, load, delete, import) apply immediately — the buttons just "
                       "close."),
        this));
    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
    connect(cancel, &QPushButton::clicked, this, [this] {
        commitAllRenames();
        reject();
    });
    auto* ok = new QPushButton(QStringLiteral("OK"), this);
    connect(ok, &QPushButton::clicked, this, [this] {
        commitAllRenames();
        accept();
    });
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    layout->addLayout(buttons);
}

void ManagePlaythroughsDialog::rebuildRows()
{
    QLayoutItem* child;
    while ((child = m_rowsLayout->takeAt(0)) != nullptr) {
        if (auto* widget = child->widget())
            widget->deleteLater();
        delete child;
    }
    m_rows.clear();

    const auto playthroughs = m_controller->playthroughs();
    const auto active = m_controller->activePlaythrough();
    for (const Playthrough& playthrough : playthroughs) {
        auto* row = new QWidget(this);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        auto* name = new QLineEdit(playthrough.name, row);
        name->setAccessibleName(QStringLiteral("Playthrough name"));
        name->setFixedWidth(220);
        rowLayout->addWidget(name);
        rowLayout->addWidget(new QLabel(playthrough.createdAt, row), 1);

        auto* load = new QPushButton(QStringLiteral("Load"), row);
        load->setEnabled(!active || playthrough.id != active->id);
        connect(load, &QPushButton::clicked, this, [this, id = playthrough.id] {
            commitAllRenames();
            m_controller->switchPlaythrough(id);
            rebuildRows();
        });
        rowLayout->addWidget(load);

        auto* remove = new QPushButton(QStringLiteral("Delete"), row);
        const bool lastRun = playthroughs.size() <= 1;
        const bool isActive = active && playthrough.id == active->id;
        remove->setEnabled(!lastRun && !isActive);
        if (isActive)
            remove->setToolTip(QStringLiteral("Can't delete the active playthrough"));
        else if (lastRun)
            remove->setToolTip(QStringLiteral("Can't delete the only playthrough"));
        connect(remove, &QPushButton::clicked, this, [this, playthrough] {
            if (QMessageBox::question(
                    this, QStringLiteral("Delete Playthrough"),
                    QStringLiteral("Delete “%1” and ALL its findings? This cannot be undone.")
                        .arg(playthrough.name))
                != QMessageBox::Yes)
                return;
            if (!m_controller->deletePlaythrough(playthrough)) {
                if (!m_controller->lastError().isEmpty())
                    QMessageBox::warning(this, QStringLiteral("Manage Playthroughs"),
                                         m_controller->lastError());
                m_controller->clearError();
            }
            rebuildRows();
        });
        rowLayout->addWidget(remove);

        m_rowsLayout->addWidget(row);
        m_rows.push_back(RowWidgets{playthrough.id, name});
    }
    m_rowsLayout->addStretch(1);
}

void ManagePlaythroughsDialog::commitAllRenames()
{
    // Three commit paths (Return handled per-field): focus-out and dialog close
    // both land here. Blank restores the stored name.
    const auto playthroughs = m_controller->playthroughs();
    for (const RowWidgets& row : m_rows) {
        const QString text = row.name->text().trimmed();
        for (const Playthrough& playthrough : playthroughs) {
            if (playthrough.id == row.id && text != playthrough.name) {
                if (!text.isEmpty())
                    m_controller->renamePlaythrough(playthrough, text);
                break;
            }
        }
    }
}

// MARK: - ImportFromSaveDialog

ImportFromSaveDialog::ImportFromSaveDialog(AppController* controller, QWidget* parent)
    : QDialog(parent)
    , m_controller(controller)
{
    setWindowTitle(QStringLiteral("Import from Save"));
    setMinimumWidth(560);
    auto* layout = new QVBoxLayout(this);

    m_saves = new QListWidget(this);
    m_saves->setAccessibleName(QStringLiteral("Save games"));
    layout->addWidget(m_saves);
    m_status = new QLabel(this);
    layout->addWidget(m_status);
    connect(m_saves, &QListWidget::currentRowChanged, this, [this](int index) {
        m_importButton->setEnabled(index >= 0 && index < int(m_summaries.size()));
    });

    layout->addWidget(new QLabel(
        QStringLiteral("The active save is AUTOSAVE.json — the game writes it continuously. "
                       "Saves elsewhere (another Steam library, a backup) load via "
                       "“Choose folder…”."),
        this));

    auto* destinationForm = new QFormLayout();
    m_destination = new QComboBox(this);
    m_destination->addItem(QStringLiteral("New playthrough"), 1);
    m_destination->addItem(QStringLiteral("Current playthrough — ")
                               + (m_controller->activePlaythrough()
                                      ? m_controller->activePlaythrough()->name
                                      : QStringLiteral("none")),
                           0);
    destinationForm->addRow(QStringLiteral("Import into"), m_destination);
    m_newName = new QLineEdit(this);
    destinationForm->addRow(QStringLiteral("New playthrough name"), m_newName);
    layout->addLayout(destinationForm);

    m_report = new QLabel(this);
    m_report->setWordWrap(true);
    layout->addWidget(m_report);

    auto* buttons = new QHBoxLayout();
    auto* chooseFolder = new QPushButton(QStringLiteral("Choose folder…"), this);
    connect(chooseFolder, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Folder containing save games"),
            QDir::homePath() + QStringLiteral("/.config/unity3d"));
        if (dir.isEmpty())
            return;
        const ScopedEnv env("BOH_SAVE_DIR", dir);
        scan();
        m_folderNote = QStringLiteral("Looking in %1.").arg(dir);
        m_status->setText(m_folderNote);
    });
    buttons->addWidget(chooseFolder);
    buttons->addStretch(1);
    auto* done = new QPushButton(QStringLiteral("Done"), this);
    connect(done, &QPushButton::clicked, this, &QDialog::reject);
    m_importButton = new QPushButton(QStringLiteral("Import"), this);
    m_importButton->setEnabled(false);
    m_importButton->setDefault(true);
    connect(m_importButton, &QPushButton::clicked, this, [this] { runImport(); });
    buttons->addWidget(m_importButton);
    layout->addLayout(buttons);

    scan();
}

void ImportFromSaveDialog::scan()
{
    m_summaries = SaveScanner::availableSaves();
    m_saves->clear();
    for (const SaveGameSummary& summary : m_summaries) {
        QStringList parts;
        parts << summary.fileName;
        if (summary.modifiedAt)
            parts << summary.modifiedAt->toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        if (summary.gameVersion)
            parts << *summary.gameVersion;
        parts << QStringLiteral("%1 books (%2 mastered)")
                       .arg(summary.bookCount)
                       .arg(summary.masteredCount);
        m_saves->addItem(parts.join(QStringLiteral(" · ")));
    }
    if (!m_summaries.empty())
        m_saves->setCurrentRow(0);
    else
        m_importButton->setEnabled(false);
}

void ImportFromSaveDialog::runImport()
{
    const int row = m_saves->currentRow();
    if (row < 0 || row >= int(m_summaries.size()))
        return;
    const SaveGameSummary summary = m_summaries[size_t(row)];
    const bool asNew = m_destination->currentData().toInt() == 1;
    qint64 destinationID = m_controller->activePlaythrough()
                               ? m_controller->activePlaythrough()->id
                               : 0;
    if (asNew) {
        const QString name = m_newName->text().trimmed();
        const QString finalName = name.isEmpty() ? QStringLiteral("Imported from %1").arg(summary.stem())
                                                 : name;
        if (!m_controller->createPlaythrough(finalName, QString())) {
            QMessageBox::warning(this, QStringLiteral("Import"),
                                 m_controller->lastError());
            m_controller->clearError();
            return;
        }
        destinationID = m_controller->activePlaythrough()->id;
    }

    m_importButton->setEnabled(false);
    m_report->setText(QStringLiteral("Importing… (parsing a large save — a few moments)"));
    QApplication::processEvents();

    const auto elements = BoHPaths::gameElementsDirectory();
    if (!elements) {
        m_report->setText(QStringLiteral("Book of Hours game data not found at the standard "
                                         "Steam path"));
        m_importButton->setEnabled(true);
        return;
    }
    try {
        m_controller->db()->transaction([&] {
            const ImportReport report =
                SaveImporter::run(summary.path, *m_controller->db(), destinationID, *elements);
            m_report->setText(report.summary());
        });
        m_controller->reloadAll();
    } catch (const std::exception& e) {
        m_report->setText(QStringLiteral("Import failed: %1").arg(e.what()));
        m_importButton->setEnabled(true);
        return;
    }
    m_importButton->setEnabled(true);
}

} // namespace boh
