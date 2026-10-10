// Port of SaveImport.swift — populating a playthrough from a Book of Hours save.
// The save and the game's definition files are lenient JSON (mixed UTF-16/UTF-8,
// raw control characters, trailing commas), so nothing here uses Qt's strict
// QJsonDocument without the sanitizer pipeline. Task 7: LenientJSON, SaveScanner.
// Task 8: SaveImporter + Linux Steam path discovery.
#pragma once

#include "Models.h"
#include "Repositories/BookRepository.h"
#include "Repositories/JournalRepository.h"
#include "Repositories/LookupRepositories.h"
#include "Repositories/MemoryRepository.h"
#include "Repositories/SkillRepository.h"
#include "SQLiteDatabase.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace boh {

// MARK: - Lenient JSON

class LenientJSON {
public:
    /// Parse the game's JSON dialect: UTF-16 (LE or BE) with BOM or UTF-8;
    /// raw control characters inside strings; trailing commas before `]`/`}`;
    /// duplicate keys — last wins.
    static std::optional<QJsonObject> object(const QByteArray& data);

    /// Escape raw control characters that appear inside JSON strings.
    static QString escapingControlCharacters(const QString& text);

    /// Remove trailing commas the game's parser tolerates — OUTSIDE strings only:
    /// a string value may contain ", ]" as text, which a whole-text regex would
    /// corrupt. Same state machine as escapingControlCharacters.
    static QString strippingTrailingCommas(const QString& text);

    // Typed accessors over the parsed document.
    static std::optional<QJsonObject> dictionary(const QJsonValue& value);
    static std::vector<QJsonObject> dictionaries(const QJsonValue& value);
    static std::optional<QString> string(const QJsonValue& value);
    static std::optional<int> intValue(const QJsonValue& value);
};

// MARK: - Standard paths

class BoHPaths {
public:
    /// The saves directory. Override for tests with `BOH_SAVE_DIR`; otherwise the
    /// Linux Steam candidates (Proton prefix under every library).
    static std::optional<QString> saveDirectory();

    /// The game's element-definition directory. Override for tests with
    /// `BOH_GAME_ELEMENTS`; otherwise the Proton install under every library.
    static std::optional<QString> gameElementsDirectory();

    // Testable seams (Review Focus #3: installs outside default paths).
    /// Steam library roots we know about (default + Debian + Flatpak layouts).
    static QStringList steamRoots();
    /// Parse each root's libraryfolders.vdf for library paths; a root without a
    /// vdf contributes itself (the default install IS a library).
    static QStringList steamLibraryPaths(const QStringList& steamRoots);
    /// Per library: the Proton-prefix save directory for app id 1028310.
    static QStringList saveDirectoryCandidates(const QStringList& libraryPaths);
    /// Per library: the Windows-layout StreamingAssets elements directory.
    static QStringList gameElementsDirectoryCandidates(const QStringList& libraryPaths);
    static std::optional<QString> firstExisting(const QStringList& candidates);
};

// MARK: - Scanner

/// One save game found in the standard install path (summary only — the file
/// is only fully parsed when an import runs).
struct SaveGameSummary {
    QString path;
    QString fileName;
    std::optional<QDateTime> modifiedAt;
    std::optional<QString> gameVersion;
    int bookCount = 0;
    int masteredCount = 0;
    int skillStackCount = 0;

    /// "AUTOSAVE" / "save" — the extension-less stem.
    QString stem() const;
};

class SaveScanner {
public:
    /// Real saves are single-digit MB (AUTOSAVE ~ 7.5 MB); this cap keeps a stray
    /// huge file in the save folder from freezing the manager sheet — files over
    /// it are skipped rather than slurped.
    static constexpr int maxSaveFileBytes = 64 * 1024 * 1024;

    /// Save-game candidates in the standard directory, newest first. Only files
    /// that actually contain a `RootPopulationCommand` count (the folder also
    /// holds achievements/config, which don't). Quick string scan, no full parse.
    static std::vector<SaveGameSummary> availableSaves(int maxFileBytes = maxSaveFileBytes);

private:
    static std::optional<QString> decodedText(const QByteArray& data);
    static std::optional<QString> versionIn(const QString& text);
    static int countOccurrences(const QString& needle, const QString& text);
};

// MARK: - Import

struct ImportReport {
    int booksCreated = 0;
    int booksUpdated = 0;
    int skillsCreated = 0;
    int skillsUpdated = 0;
    int memoriesCreated = 0;
    std::vector<QString> languagesAdded;
    int uncataloguedSkipped = 0;
    /// Books seen at the auction but not acquired: unearned, never imported.
    int unearnedSkipped = 0;
    std::vector<QString> warnings;

    QString summary() const;
};

class SaveImportError : public std::runtime_error {
public:
    enum class Kind { UnreadableSave, GameDataNotFound, NoActivePlaythrough };

    SaveImportError(Kind kind, const QString& message);
    Kind kind() const { return m_kind; }

private:
    Kind m_kind;
};

class SaveImporter {
public:
    /// Import one save into a playthrough. All writes happen in the caller's
    /// transaction (the database is main-thread confined).
    static ImportReport run(const QString& savePath, SQLiteDatabase& db, qint64 playthroughID,
                            const QString& elementsDirectory);

private:
    static void collectStacks(const QJsonObject& sphere, QHash<QString, QJsonObject>& stacks,
                              QHash<QString, QString>& locations, int& uncatalogued, int& unearned,
                              const QStringList& chain);
    static QString roomLabel(const QString& id);
    static std::optional<QString> locationLabel(const QStringList& chain);
    static QHash<QString, QJsonObject> elementIndex(const QString& directory);
    static QString principleKey(int value, const QJsonObject& definition);
    static QString languageName(const QString& aspectSuffix,
                                const QHash<QString, QJsonObject>& index);
    static QString elementName(const QString& code);
    static MemoryDraft memoryDraft(const QString& element, const QJsonObject& definition,
                                   const QHash<QString, qint64>& principleIDByName);
    static BookKind bookKind(const QJsonObject& aspects);
};

} // namespace boh
