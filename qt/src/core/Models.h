// Port of Models.swift — value types and value-set enums (docs/DATABASE.md).
// Swift Identifiable computed `id`s (Aspect.id = principleID etc.) are dropped:
// they existed for SwiftUI list identity, which Qt views don't need.
#pragma once

#include <QString>
#include <QtGlobal>

#include <optional>
#include <vector>

namespace boh {

// MARK: - Value sets

enum class MemoryKind { Memory, Weather, Numen };
QString memoryKindToString(MemoryKind kind);                  // "memory" | "weather" | "numen"
std::optional<MemoryKind> memoryKindFromString(const QString& raw);

enum class BookKind { Book, Scroll, Film, Record };
QString bookKindToString(BookKind kind);                      // "book" | "scroll" | "film" | "record"
std::optional<BookKind> bookKindFromString(const QString& raw);

enum class ReadStatus { Uncatalogued, Catalogued, Mastered };
QString readStatusToString(ReadStatus status);
std::optional<ReadStatus> readStatusFromString(const QString& raw);

// Lowercase raw values are the game's own contamination categories.
enum class Contamination { None, Curse, Theoplasm, Infestation, Corruption, Winkwell, Witchworms };
QString contaminationToString(Contamination contamination);   // db/game value
QString contaminationLabel(Contamination contamination);      // Title-case, for pickers
std::optional<Contamination> contaminationFromString(const QString& raw);

enum class MemorySourceKind {
    ReReadBook, FirstRead, Weather, Talk, Consider, Consume, Craft, Gather, Numa, Other,
};
QString memorySourceKindToString(MemorySourceKind kind);      // "re-read book", "first read", …
std::optional<MemorySourceKind> memorySourceKindFromString(const QString& raw);

// MARK: - Lookups

struct Principle {
    qint64 id = 0;
    QString name;
    int sortOrder = 0;
    std::optional<QString> color;
    std::optional<QString> notes;

    bool operator==(const Principle&) const = default;
};

struct Language {
    qint64 id = 0;
    QString name;
    bool native = false;
    std::optional<QString> notes;

    bool operator==(const Language&) const = default;
};

// MARK: - Memories

struct Aspect {
    qint64 principleID = 0;
    QString principleName;
    int level = 0;

    bool operator==(const Aspect&) const = default;
};

/// An aspect pending creation, before it has a principle name attached.
struct AspectDraft {
    qint64 principleID = 0;
    int level = 0;

    bool operator==(const AspectDraft&) const = default;
};

struct Memory {
    qint64 id = 0;
    QString name;
    MemoryKind kind = MemoryKind::Memory;
    bool persistent = false;
    std::optional<QString> notes;
    std::vector<Aspect> aspects;

    bool operator==(const Memory&) const = default;
};

struct MemoryDraft {
    QString name;
    MemoryKind kind = MemoryKind::Memory;
    bool persistent = false;
    std::optional<QString> notes;
    std::vector<AspectDraft> aspects;
};

struct MemorySource {
    QString kind;
    std::optional<QString> detail;

    bool operator==(const MemorySource&) const = default;
};

/// A memory satisfying a Reading-Helper lookup (canonical query 1, docs/DATABASE.md).
struct MemoryCandidate {
    qint64 id = 0;
    QString name;
    MemoryKind kind = MemoryKind::Memory;
    bool persistent = false;
    int level = 0;

    bool operator==(const MemoryCandidate&) const = default;
};

/// A book that yields a given memory (backlink shown on memory screens).
struct BookRef {
    qint64 id = 0;
    QString title;

    bool operator==(const BookRef&) const = default;
};

// MARK: - Books

struct Book {
    qint64 id = 0;
    QString title;
    std::optional<QString> setName;
    std::optional<QString> volume;
    BookKind bookKind = BookKind::Book;
    std::optional<qint64> languageID;
    std::optional<qint64> mysteryPrincipleID;
    /// Level of the reading requirement — the game community's "Difficulty"
    /// (wiki book tables: "Mastery Difficulty"). Recordable as soon as a book is
    /// catalogued, even when it can't be mastered straightaway.
    std::optional<int> difficulty;
    ReadStatus readStatus = ReadStatus::Uncatalogued;
    std::optional<Contamination> contamination;
    std::optional<QString> location;
    int timesRead = 0;
    std::optional<QString> firstReadAt;
    std::optional<QString> lastReadAt;
    std::optional<int> lessons;
    std::optional<qint64> yieldedMemoryID;
    std::optional<QString> notes;

    bool operator==(const Book&) const = default;
};

struct BookDraft {
    QString title;
    std::optional<QString> setName;
    std::optional<QString> volume;
    BookKind bookKind = BookKind::Book;
    std::optional<qint64> languageID;
    std::optional<qint64> mysteryPrincipleID;
    std::optional<int> difficulty;
    ReadStatus readStatus = ReadStatus::Uncatalogued;
    std::optional<Contamination> contamination;
    std::optional<QString> location;
    std::optional<int> lessons;
    std::optional<qint64> yieldedMemoryID;
    std::optional<QString> notes;
};

/// Which skill's lessons a book teaches (junction `BookLessons`).
struct BookLessonsEntry {
    qint64 skillID = 0;
    int amount = 1;

    bool operator==(const BookLessonsEntry&) const = default;
};

// MARK: - Skills

struct Skill {
    qint64 id = 0;
    QString name;
    bool isLanguage = false;
    std::optional<qint64> primaryPrincipleID;
    std::optional<qint64> secondaryPrincipleID;
    std::optional<int> level;
    std::optional<QString> wisdom;
    std::optional<QString> element;
    std::optional<QString> notes;

    bool operator==(const Skill&) const = default;
};

struct SkillDraft {
    QString name;
    bool isLanguage = false;
    std::optional<qint64> primaryPrincipleID;
    std::optional<qint64> secondaryPrincipleID;
    std::optional<int> level;
    std::optional<QString> wisdom;
    std::optional<QString> element;
    std::optional<QString> notes;
};

/// A skill's computed contribution toward a principle (canonical query 2:
/// a level-L skill contributes L+1 primary / L secondary).
struct SkillContribution {
    Skill skill;
    int contributes = 0;

    bool operator==(const SkillContribution&) const = default;
};

// MARK: - Journal

/// One saved game. BoH is run-based: findings don't carry over between
/// Librarians, so every book/memory/skill/journal row is scoped to one of these.
struct Playthrough {
    qint64 id = 0;
    QString name;
    QString createdAt;
    std::optional<QString> notes;

    bool operator==(const Playthrough&) const = default;
};

struct JournalEntry {
    qint64 id = 0;
    QString loggedAt;
    std::optional<QString> gameDay;
    QString entry;
    std::optional<qint64> bookID;
    std::optional<qint64> memoryID;
    std::optional<qint64> skillID;

    bool operator==(const JournalEntry&) const = default;
};

struct JournalDraft {
    std::optional<QString> gameDay;
    QString entry;
    std::optional<qint64> bookID;
    std::optional<qint64> memoryID;
    std::optional<qint64> skillID;
};

} // namespace boh
