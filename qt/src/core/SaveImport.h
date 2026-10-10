// Port of SaveImport.swift — populating a playthrough from a Book of Hours save.
// The save and the game's definition files are lenient JSON (mixed UTF-16/UTF-8,
// raw control characters, trailing commas), so nothing here uses Qt's strict
// QJsonDocument without the sanitizer pipeline. Task 7: LenientJSON, BoHPaths
// (env stubs; Linux candidates land in Task 8), SaveScanner. Task 8: importer.
#pragma once

#include "Models.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QString>

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
    /// The saves directory. Override for tests with `BOH_SAVE_DIR`.
    /// (Linux Steam candidates arrive with the importer in Task 8.)
    static std::optional<QString> saveDirectory();

    /// The game's element-definition directory. Override for tests with
    /// `BOH_GAME_ELEMENTS`. (Linux Steam candidates arrive in Task 8.)
    static std::optional<QString> gameElementsDirectory();
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

} // namespace boh
