#include "SaveImport.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStringConverter>
#include <QStringEncoder>

namespace boh {

// MARK: - Lenient JSON

std::optional<QJsonObject> LenientJSON::object(const QByteArray& data)
{
    QString text;
    if (data.size() >= 2 && (unsigned char) data[0] == 0xFF && (unsigned char) data[1] == 0xFE) {
        // UTF-16LE with BOM (the game's own files).
        auto decoder = QStringDecoder(QStringConverter::Utf16LE);
        text = decoder.decode(data.mid(2));
    } else if (data.size() >= 2 && (unsigned char) data[0] == 0xFE
               && (unsigned char) data[1] == 0xFF) {
        // UTF-16BE with BOM (the Swift pipeline only handled LE; BE is a freebie).
        auto decoder = QStringDecoder(QStringConverter::Utf16BE);
        text = decoder.decode(data.mid(2));
    } else {
        text = QString::fromUtf8(data);
    }
    QString cleaned = text;
    if (cleaned.startsWith(QChar(0xFEFF)))
        cleaned.remove(0, 1);
    cleaned = escapingControlCharacters(cleaned);
    cleaned = strippingTrailingCommas(cleaned);
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(cleaned.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    return document.object();
}

QString LenientJSON::escapingControlCharacters(const QString& text)
{
    bool hasControl = false;
    for (const QChar ch : text) {
        if (ch.unicode() < 0x20) {
            hasControl = true;
            break;
        }
    }
    if (!hasControl)
        return text; // fast path: nothing to escape

    QString out;
    out.reserve(text.size());
    bool inString = false;
    bool escaped = false;
    for (const QChar ch : text) {
        const char16_t value = ch.unicode();
        if (inString) {
            if (escaped) {
                escaped = false;
                out.append(ch);
            } else if (value == u'\\') {
                escaped = true;
                out.append(ch);
            } else if (value == u'"') {
                inString = false;
                out.append(ch);
            } else if (value < 0x20) {
                out += QStringLiteral("\\u%1").arg(value, 4, 16, QChar(u'0'));
            } else {
                out.append(ch);
            }
        } else {
            if (value == u'"')
                inString = true;
            out.append(ch);
        }
    }
    return out;
}

QString LenientJSON::strippingTrailingCommas(const QString& text)
{
    QString out;
    out.reserve(text.size());
    QString pending; // commas/whitespace since the last significant char
    bool inString = false;
    bool escaped = false;
    for (const QChar ch : text) {
        const char16_t value = ch.unicode();
        if (inString) {
            if (escaped) {
                escaped = false;
                out.append(ch);
                continue;
            }
            if (value == u'\\') {
                escaped = true;
                out.append(ch);
                continue;
            }
            if (value == u'"') {
                inString = false;
                out.append(ch);
                continue;
            }
            out.append(ch);
            continue;
        }
        switch (value) {
        case u'"':
            out += pending;
            pending.clear();
            inString = true;
            out.append(ch);
            break;
        case u' ':
        case u'\t':
        case u'\n':
        case u'\r':
        case u',':
            pending.append(ch);
            break;
        case u']':
        case u'}': {
            QString kept;
            for (const QChar pendingChar : pending) {
                if (pendingChar.unicode() != u',')
                    kept.append(pendingChar);
            }
            out += kept;
            pending.clear();
            out.append(ch);
            break;
        }
        default:
            out += pending;
            pending.clear();
            out.append(ch);
            break;
        }
    }
    out += pending;
    return out;
}

std::optional<QJsonObject> LenientJSON::dictionary(const QJsonValue& value)
{
    return value.isObject() ? std::optional<QJsonObject>(value.toObject()) : std::nullopt;
}

std::vector<QJsonObject> LenientJSON::dictionaries(const QJsonValue& value)
{
    std::vector<QJsonObject> out;
    if (!value.isArray())
        return out;
    const QJsonArray array = value.toArray();
    out.reserve(array.size());
    for (const QJsonValue& entry : array) {
        if (entry.isObject())
            out.push_back(entry.toObject());
    }
    return out;
}

std::optional<QString> LenientJSON::string(const QJsonValue& value)
{
    return value.isString() ? std::optional<QString>(value.toString()) : std::nullopt;
}

std::optional<int> LenientJSON::intValue(const QJsonValue& value)
{
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (number >= -2147483648.0 && number <= 2147483647.0 && number == qint64(number))
            return int(number);
    }
    return std::nullopt;
}

// MARK: - Import


SaveImportError::SaveImportError(Kind kind, const QString& message)
    : std::runtime_error(message.toUtf8().constData())
    , m_kind(kind)
{
}

namespace {

constexpr const char* kBookOfHoursAppID = "1028310"; // recorded in docs/SAVE_IMPORT.md

const QSet<QString>& nativeLanguageNames()
{
    static const QSet<QString> names = {QStringLiteral("greek"), QStringLiteral("latin"),
                                        QStringLiteral("sanskrit"), QStringLiteral("aramaic"),
                                        QStringLiteral("phrygian")};
    return names;
}

const QHash<QString, QString>& elementCodes()
{
    static const QHash<QString, QString> codes = {
        {QStringLiteral("hea"), QStringLiteral("Health")}, {QStringLiteral("cho"), QStringLiteral("Chor")},
        {QStringLiteral("sha"), QStringLiteral("Shapt")},  {QStringLiteral("met"), QStringLiteral("Mettle")},
        {QStringLiteral("ere"), QStringLiteral("Ereb")},   {QStringLiteral("tri"), QStringLiteral("Trist")},
        {QStringLiteral("fet"), QStringLiteral("Fet")},    {QStringLiteral("pho"), QStringLiteral("Phost")},
        {QStringLiteral("wis"), QStringLiteral("Wist")},
    };
    return codes;
}

std::optional<int> stackMutation(const QString& key, const QJsonObject& stack)
{
    return LenientJSON::intValue(stack.value(key));
}

} // namespace

// MARK: - Standard paths

QStringList BoHPaths::steamRoots()
{
    const QString home = QDir::homePath();
    return {
        home + QStringLiteral("/.steam/steam"),
        home + QStringLiteral("/.local/share/Steam"),
        home + QStringLiteral("/.steam/debian-installation"),
        home + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam"), // Flatpak Steam
    };
}

QStringList BoHPaths::steamLibraryPaths(const QStringList& steamRoots)
{
    QStringList libraries;
    // libraryfolders.vdf lists every installed library ("path"  "…"); a root
    // without one contributes itself (the default install IS a library).
    static const QRegularExpression pathPattern(QStringLiteral("\"path\"\\s*\"([^\"]+)\""));
    for (const QString& root : steamRoots) {
        QFile vdf(root + QStringLiteral("/steamapps/libraryfolders.vdf"));
        bool parsed = false;
        if (vdf.open(QIODevice::ReadOnly)) {
            const QString text = QString::fromUtf8(vdf.readAll());
            auto it = pathPattern.globalMatch(text);
            while (it.hasNext()) {
                const QString library = QDir::cleanPath(it.next().captured(1));
                if (!libraries.contains(library))
                    libraries << library;
                parsed = true;
            }
        }
        if (!parsed && QDir(root).exists() && !libraries.contains(QDir::cleanPath(root)))
            libraries << QDir::cleanPath(root);
    }
    return libraries;
}

QStringList BoHPaths::saveDirectoryCandidates(const QStringList& libraryPaths)
{
    QStringList candidates;
    for (const QString& library : libraryPaths) {
        candidates << library + QStringLiteral("/steamapps/compatdata/") + kBookOfHoursAppID
                         + QStringLiteral("/pfx/drive_c/users/steamuser/AppData/LocalLow/"
                                          "Weather Factory/Book of Hours");
    }
    return candidates;
}

QStringList BoHPaths::gameElementsDirectoryCandidates(const QStringList& libraryPaths)
{
    QStringList candidates;
    for (const QString& library : libraryPaths) {
        // Proton runs the Windows build: <Game>_Data, not Contents/Resources.
        candidates << library
                          + QStringLiteral("/steamapps/common/Book of Hours/Book of Hours_Data/"
                                           "StreamingAssets/bhcontent/core/elements");
    }
    return candidates;
}

std::optional<QString> BoHPaths::firstExisting(const QStringList& candidates)
{
    for (const QString& candidate : candidates) {
        if (QDir(candidate).exists())
            return candidate;
    }
    return std::nullopt;
}

std::optional<QString> BoHPaths::saveDirectory()
{
    const QString override = qEnvironmentVariable("BOH_SAVE_DIR");
    if (!override.isEmpty())
        return override;
    return firstExisting(saveDirectoryCandidates(steamLibraryPaths(steamRoots())));
}

std::optional<QString> BoHPaths::gameElementsDirectory()
{
    const QString override = qEnvironmentVariable("BOH_GAME_ELEMENTS");
    if (!override.isEmpty())
        return override;
    return firstExisting(gameElementsDirectoryCandidates(steamLibraryPaths(steamRoots())));
}

// MARK: - Scanner

QString SaveGameSummary::stem() const
{
    const qsizetype dot = fileName.lastIndexOf(QLatin1Char('.'));
    return dot <= 0 ? fileName : fileName.left(dot);
}

std::optional<QString> SaveScanner::decodedText(const QByteArray& data)
{
    if (data.size() >= 2 && (unsigned char) data[0] == 0xFF && (unsigned char) data[1] == 0xFE) {
        QString text = QStringDecoder(QStringConverter::Utf16LE).decode(data.mid(2));
        if (text.startsWith(QChar(0xFEFF)))
            text.remove(0, 1);
        return text;
    }
    if (data.size() >= 2 && (unsigned char) data[0] == 0xFE && (unsigned char) data[1] == 0xFF)
        return QStringDecoder(QStringConverter::Utf16BE).decode(data.mid(2));
    return QString::fromUtf8(data);
}

std::optional<QString> SaveScanner::versionIn(const QString& text)
{
    // The save serialises compactly ("Version":"x"); hand-written fixtures
    // may include spaces. Allow either.
    static const QRegularExpression pattern(QStringLiteral("\\\"Version\\\"\\s*:\\s*\\\"([^\\\"]+)\\\""));
    const auto match = pattern.match(text);
    if (!match.hasMatch() || match.lastCapturedIndex() < 1)
        return std::nullopt;
    return match.captured(1);
}

int SaveScanner::countOccurrences(const QString& needle, const QString& text)
{
    int count = 0;
    qsizetype from = 0;
    while (true) {
        const qsizetype found = text.indexOf(needle, from);
        if (found < 0)
            break;
        ++count;
        from = found + needle.size();
    }
    return count;
}

std::vector<SaveGameSummary> SaveScanner::availableSaves(int maxFileBytes)
{
    const auto directoryPath = BoHPaths::saveDirectory();
    if (!directoryPath)
        return {};
    const QDir directory(*directoryPath);
    if (!directory.exists())
        return {};

    std::vector<SaveGameSummary> summaries;
    const QStringList files = directory.entryList({QStringLiteral("*.json")}, QDir::Files,
                                                  QDir::Name);
    for (const QString& fileName : files) {
        if (fileName.section(QLatin1Char('.'), -1).compare(QStringLiteral("json"),
                                                           Qt::CaseInsensitive) != 0)
            continue;
        QFileInfo info(directory.filePath(fileName));
        if (info.size() > maxFileBytes)
            continue;
        QFile file(info.filePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray data = file.readAll();
        const auto text = decodedText(data);
        if (!text || !text->contains(QStringLiteral("\"RootPopulationCommand\"")))
            continue;
        SaveGameSummary summary;
        summary.path = info.filePath();
        summary.fileName = fileName;
        summary.modifiedAt = info.lastModified();
        summary.gameVersion = versionIn(*text);
        summary.bookCount = countOccurrences(QStringLiteral("\"EntityId\":\"t."), *text);
        summary.masteredCount = countOccurrences(QStringLiteral("\"mastery."), *text);
        summary.skillStackCount = countOccurrences(QStringLiteral("\"EntityId\":\"s."), *text);
        summaries.push_back(summary);
    }
    std::sort(summaries.begin(), summaries.end(),
              [](const SaveGameSummary& a, const SaveGameSummary& b) {
                  return (a.modifiedAt ? *a.modifiedAt : QDateTime())
                         > (b.modifiedAt ? *b.modifiedAt : QDateTime());
              });
    return summaries;
}


// MARK: - Importer

QString ImportReport::summary() const
{
    QStringList parts;
    parts << QStringLiteral("%1 books (%2 new, %3 updated)")
                    .arg(booksCreated + booksUpdated)
                    .arg(booksCreated)
                    .arg(booksUpdated);
    parts << QStringLiteral("%1 skills (%2 new, %3 updated)")
                    .arg(skillsCreated + skillsUpdated)
                    .arg(skillsCreated)
                    .arg(skillsUpdated);
    parts << QStringLiteral("%1 memories created").arg(memoriesCreated);
    if (!languagesAdded.empty()) {
        QString joined;
        for (const QString& language : languagesAdded)
            joined += (joined.isEmpty() ? QString() : QStringLiteral(", ")) + language;
        parts << QStringLiteral("languages added: ") + joined;
    }
    if (uncataloguedSkipped > 0)
        parts << QStringLiteral("%1 uncatalogued texts skipped (identity unknown)")
                       .arg(uncataloguedSkipped);
    if (unearnedSkipped > 0)
        parts << QStringLiteral("%1 unacquired lots skipped (seen at the auction, not owned)")
                       .arg(unearnedSkipped);
    if (!warnings.empty())
        parts << QStringLiteral("%1 warning(s)").arg(int(warnings.size()));
    return parts.join(QStringLiteral(" · "));
}

ImportReport SaveImporter::run(const QString& savePath, SQLiteDatabase& db, qint64 playthroughID,
                               const QString& elementsDirectory)
{
    QFile saveFile(savePath);
    if (!saveFile.open(QIODevice::ReadOnly))
        throw SaveImportError(SaveImportError::Kind::UnreadableSave, savePath);
    const auto saveRoot =
        LenientJSON::object(saveFile.readAll());
    const auto root = saveRoot ? LenientJSON::dictionary(saveRoot->value(QStringLiteral("RootPopulationCommand")))
                               : std::nullopt;
    if (!root)
        throw SaveImportError(SaveImportError::Kind::UnreadableSave,
                              QStringLiteral("couldn't read save at %1").arg(savePath));
    const QHash<QString, QJsonObject> index = elementIndex(elementsDirectory);

    // Player state: element stacks, deduped by EntityId (the save keeps copies
    // of committed/uncommitted skill cards and moved-around books), plus where
    // each one sits — the sphere chain "Room > slot" (Library > ShelfSpaceD.3).
    QHash<QString, QJsonObject> stacks;
    QHash<QString, QString> locations;
    int uncatalogued = 0;
    int unearned = 0;
    for (const QJsonObject& sphere : LenientJSON::dictionaries(root->value(QStringLiteral("Spheres")))) {
        collectStacks(sphere, stacks, locations, uncatalogued, unearned, {});
    }

    // Our lookups.
    QHash<QString, qint64> principleIDByName;
    for (const Principle& principle : PrincipleRepository(db).all())
        principleIDByName.insert(principle.name.toLower(), principle.id);
    LanguageRepository languageRepo(db);
    std::vector<Language> languages = languageRepo.all();
    QHash<QString, qint64> languageIDByName;
    for (const Language& language : languages)
        languageIDByName.insert(language.name.toLower(), language.id);

    BookRepository bookRepo(db, playthroughID);
    MemoryRepository memoryRepo(db, playthroughID);
    SkillRepository skillRepo(db, playthroughID);
    JournalRepository journalRepo(db, playthroughID);

    ImportReport report;
    report.unearnedSkipped = unearned;
    QHash<QString, Book> existingBooksByTitle;
    for (const Book& book : bookRepo.all())
        existingBooksByTitle.insert(book.title, book);
    QHash<QString, Memory> existingMemoriesByName;
    for (const Memory& memory : memoryRepo.all())
        existingMemoriesByName.insert(memory.name, memory);
    QHash<QString, Skill> existingSkillsByName;
    for (const Skill& skill : skillRepo.all())
        existingSkillsByName.insert(skill.name, skill);

    // Pass 1: skills (books reference them via lessons).
    QHash<QString, qint64> skillIDByElementID;
    for (auto it = stacks.constBegin(); it != stacks.constEnd(); ++it) {
        const QString& entityID = it.key();
        if (!entityID.startsWith(QStringLiteral("s.")))
            continue;
        const QJsonObject& stack = it.value();
        if (!index.contains(entityID)) {
            report.warnings.push_back(
                QStringLiteral("skill %1 has no game definition — skipped").arg(entityID));
            continue;
        }
        const QJsonObject def = index.value(entityID);
        const int level = 1 + stackMutation(QStringLiteral("skill"), stack).value_or(0);
        const auto name = LenientJSON::string(def.value(QStringLiteral("Label")));
        if (!name) {
            report.warnings.push_back(
                QStringLiteral("skill %1 has no Label — skipped").arg(entityID));
            continue;
        }
        const auto defAspects = LenientJSON::dictionary(def.value(QStringLiteral("aspects")));
        const bool isLanguage =
            defAspects && defAspects->contains(QStringLiteral("skill.language"));
        QString wisdom;
        for (auto keyIt = stack.constBegin(); keyIt != stack.constEnd(); ++keyIt) {
            if (keyIt.key().startsWith(QStringLiteral("w.")) && keyIt.value() == -1) {
                wisdom = keyIt.key().mid(2);
                break;
            }
        }
        QString elementCode;
        for (auto keyIt = stack.constBegin(); keyIt != stack.constEnd(); ++keyIt) {
            if (keyIt.key().startsWith(QStringLiteral("a.x"))) {
                elementCode = keyIt.key().mid(3);
                break;
            }
        }

        if (existingSkillsByName.contains(*name)) {
            Skill existing = existingSkillsByName.value(*name);
            if (!existing.level)
                existing.level = level;
            if (!existing.isLanguage && isLanguage)
                existing.isLanguage = true;
            if (!existing.wisdom && !wisdom.isEmpty())
                existing.wisdom = wisdom;
            if (!existing.element && !elementCode.isEmpty())
                existing.element = elementName(elementCode);
            skillRepo.update(existing);
            report.skillsUpdated += 1;
            skillIDByElementID.insert(entityID, existing.id);
        } else {
            SkillDraft draft;
            draft.name = *name;
            draft.isLanguage = isLanguage;
            const auto primary = principleKey(2, def);
            const auto secondary = principleKey(1, def);
            draft.primaryPrincipleID =
                primary.isEmpty() ? std::nullopt
                                  : std::optional<qint64>(principleIDByName.value(primary.toLower()));
            draft.secondaryPrincipleID =
                secondary.isEmpty()
                    ? std::nullopt
                    : std::optional<qint64>(principleIDByName.value(secondary.toLower()));
            draft.level = level;
            if (!wisdom.isEmpty())
                draft.wisdom = wisdom;
            if (!elementCode.isEmpty())
                draft.element = elementName(elementCode);
            const Skill created = skillRepo.insert(draft);
            report.skillsCreated += 1;
            existingSkillsByName.insert(*name, created);
            skillIDByElementID.insert(entityID, created.id);
        }
    }

    // Pass 2: books.
    for (auto it = stacks.constBegin(); it != stacks.constEnd(); ++it) {
        const QString& entityID = it.key();
        if (!entityID.startsWith(QStringLiteral("t.")))
            continue;
        const QJsonObject& stack = it.value();
        if (!index.contains(entityID)) {
            report.warnings.push_back(
                QStringLiteral("book %1 has no game definition — skipped").arg(entityID));
            continue;
        }
        const QJsonObject def = index.value(entityID);
        const auto title = LenientJSON::string(def.value(QStringLiteral("Label")));
        if (!title) {
            report.warnings.push_back(
                QStringLiteral("book %1 has no game definition — skipped").arg(entityID));
            continue;
        }
        const QJsonObject aspects =
            LenientJSON::dictionary(def.value(QStringLiteral("aspects"))).value_or(QJsonObject());

        // Requirement.
        std::optional<qint64> principleID;
        std::optional<int> difficulty;
        for (auto keyIt = aspects.constBegin(); keyIt != aspects.constEnd(); ++keyIt) {
            if (keyIt.key().startsWith(QStringLiteral("mystery."))) {
                principleID = principleIDByName.value(keyIt.key().mid(QStringLiteral("mystery.").length()).toLower());
                difficulty = LenientJSON::intValue(keyIt.value());
                break;
            }
        }
        // Fallback: the mastered mutation's value equals the difficulty.
        if (!difficulty) {
            for (auto keyIt = stack.constBegin(); keyIt != stack.constEnd(); ++keyIt) {
                if (keyIt.key().startsWith(QStringLiteral("mastery."))) {
                    difficulty = LenientJSON::intValue(keyIt.value());
                    break;
                }
            }
        }

        // Language.
        std::optional<qint64> languageID;
        for (auto keyIt = aspects.constBegin(); keyIt != aspects.constEnd(); ++keyIt) {
            if (!keyIt.key().startsWith(QStringLiteral("w.")))
                continue;
            const QString name =
                languageName(keyIt.key().mid(2), index);
            if (languageIDByName.contains(name.toLower())) {
                languageID = languageIDByName.value(name.toLower());
            } else {
                const Language added = languageRepo.insert(name, false);
                languages.push_back(added);
                languageIDByName.insert(name.toLower(), added.id);
                report.languagesAdded.push_back(name);
                languageID = added.id;
            }
            break;
        }

        // State.
        bool mastered = false;
        std::optional<Contamination> contamination;
        for (auto keyIt = stack.constBegin(); keyIt != stack.constEnd(); ++keyIt) {
            if (keyIt.key().startsWith(QStringLiteral("mastery.")))
                mastered = true;
            if (keyIt.key().startsWith(QStringLiteral("contamination."))) {
                const QString raw = keyIt.key().mid(QStringLiteral("contamination.").length());
                if (const auto known = contaminationFromString(raw))
                    contamination = known;
                else
                    report.warnings.push_back(
                        QStringLiteral("%1: unknown contamination '%2' — recording none")
                            .arg(*title, raw));
            }
        }

        // Lessons.
        std::vector<BookLessonsEntry> lessonEntries;
        const QJsonObject xtriggers =
            LenientJSON::dictionary(def.value(QStringLiteral("xtriggers"))).value_or(QJsonObject());
        for (auto triggerIt = xtriggers.constBegin(); triggerIt != xtriggers.constEnd();
             ++triggerIt) {
            if (!triggerIt.key().startsWith(QStringLiteral("mastering.")))
                continue;
            for (const QJsonObject& effect : LenientJSON::dictionaries(triggerIt.value())) {
                const auto targetID = LenientJSON::string(effect.value(QStringLiteral("id")));
                if (!targetID)
                    continue;
                static const QRegularExpression stripPrefix(QStringLiteral("^(x\\.|s\\.)"));
                const QString skillElement =
                    QStringLiteral("s.") + QString(*targetID).remove(stripPrefix);
                if (skillIDByElementID.contains(skillElement)) {
                    lessonEntries.push_back(BookLessonsEntry{
                        skillIDByElementID.value(skillElement),
                        LenientJSON::intValue(effect.value(QStringLiteral("level"))).value_or(1)});
                } else if (index.contains(skillElement)
                           && index.value(skillElement).contains(QStringLiteral("Label"))
                           && existingSkillsByName.contains(
                               *LenientJSON::string(index.value(skillElement).value(QStringLiteral("Label"))))) {
                    const Skill& skill = existingSkillsByName.value(
                        *LenientJSON::string(index.value(skillElement).value(QStringLiteral("Label"))));
                    lessonEntries.push_back(BookLessonsEntry{
                        skill.id,
                        LenientJSON::intValue(effect.value(QStringLiteral("level"))).value_or(1)});
                } else {
                    report.warnings.push_back(QStringLiteral("%1: lesson for unknown skill %2 skipped")
                                                  .arg(*title, skillElement));
                }
            }
        }

        // Yielded memory (the memory every read of this book gives).
        std::optional<qint64> yieldedMemoryID;
        for (auto triggerIt = xtriggers.constBegin(); triggerIt != xtriggers.constEnd();
             ++triggerIt) {
            if (!triggerIt.key().startsWith(QStringLiteral("reading.")))
                continue;
            for (const QJsonObject& effect : LenientJSON::dictionaries(triggerIt.value())) {
                const auto memoryElement = LenientJSON::string(effect.value(QStringLiteral("id")));
                if (!memoryElement || !index.contains(*memoryElement))
                    continue;
                const QJsonObject memDef = index.value(*memoryElement);
                const auto memoryName = LenientJSON::string(memDef.value(QStringLiteral("Label")));
                if (!memoryName)
                    continue;
                if (existingMemoriesByName.contains(*memoryName)) {
                    yieldedMemoryID = existingMemoriesByName.value(*memoryName).id;
                } else {
                    const MemoryDraft draft = memoryDraft(*memoryElement, memDef, principleIDByName);
                    const Memory created = memoryRepo.insert(draft);
                    report.memoriesCreated += 1;
                    existingMemoriesByName.insert(*memoryName, created);
                    yieldedMemoryID = created.id;
                }
                break;
            }
            if (yieldedMemoryID)
                break;
        }

        int lessonCount = 0;
        for (const BookLessonsEntry& entry : lessonEntries)
            lessonCount += entry.amount;
        BookDraft draft;
        draft.title = *title;
        draft.bookKind = bookKind(aspects);
        draft.languageID = languageID;
        draft.mysteryPrincipleID = principleID;
        draft.difficulty = difficulty;
        draft.readStatus = mastered ? ReadStatus::Mastered : ReadStatus::Catalogued;
        draft.contamination = contamination;
        if (locations.contains(entityID))
            draft.location = locations.value(entityID);
        if (lessonCount > 0)
            draft.lessons = lessonCount;
        draft.yieldedMemoryID = yieldedMemoryID;

        if (existingBooksByTitle.contains(*title)) {
            // Fill empty fields only — never stomp user-recorded data.
            Book existing = existingBooksByTitle.value(*title);
            if (!existing.mysteryPrincipleID)
                existing.mysteryPrincipleID = draft.mysteryPrincipleID;
            if (!existing.difficulty)
                existing.difficulty = draft.difficulty;
            if (!existing.languageID)
                existing.languageID = draft.languageID;
            if (!existing.contamination)
                existing.contamination = draft.contamination;
            if (!existing.location)
                existing.location = draft.location;
            if (!existing.lessons)
                existing.lessons = draft.lessons;
            if (!existing.yieldedMemoryID)
                existing.yieldedMemoryID = draft.yieldedMemoryID;
            if (mastered && existing.readStatus != ReadStatus::Mastered)
                existing.readStatus = ReadStatus::Mastered;
            bookRepo.update(existing);
            if (!lessonEntries.empty())
                bookRepo.setLessons(existing.id, lessonEntries);
            existingBooksByTitle.insert(*title, existing);
            report.booksUpdated += 1;
        } else {
            const Book created = bookRepo.insert(draft);
            if (!lessonEntries.empty())
                bookRepo.setLessons(created.id, lessonEntries);
            existingBooksByTitle.insert(*title, created);
            report.booksCreated += 1;
        }
    }

    report.uncataloguedSkipped = uncatalogued;

    journalRepo.insert(JournalDraft{
        {}, QStringLiteral("Imported from %1: %2.")
                .arg(QFileInfo(savePath).fileName(), report.summary()),
        {}, {}, {}});
    return report;
}

void SaveImporter::collectStacks(const QJsonObject& sphere, QHash<QString, QJsonObject>& stacks,
                                 QHash<QString, QString>& locations, int& uncatalogued,
                                 int& unearned, const QStringList& chain)
{
    // GoverningSphereSpec.Id names the room and (nested) shelf/slot spheres.
    const QString specID =
        LenientJSON::dictionary(sphere.value(QStringLiteral("GoverningSphereSpec")))
            .value_or(QJsonObject())
            .value(QStringLiteral("Id"))
            .toString();
    QStringList thisChain = chain;
    thisChain << specID;
    for (const QJsonObject& token :
         LenientJSON::dictionaries(sphere.value(QStringLiteral("Tokens")))) {
        const QJsonObject payload =
            LenientJSON::dictionary(token.value(QStringLiteral("Payload"))).value_or(QJsonObject());
        const QString payloadType =
            LenientJSON::string(payload.value(QStringLiteral("$type"))).value_or(QString());
        if (payloadType.startsWith(QStringLiteral("ElementStackCreationCommand"))) {
            const auto entityID = LenientJSON::string(payload.value(QStringLiteral("EntityId")));
            if (entityID) {
                // Defunct tokens are stale copies left behind by moved items — skip.
                if (payload.value(QStringLiteral("Defunct")).toBool() == true) {
                    // skipped
                } else {
                    // Books sitting in an Oriflamme's-auction purchases sphere:
                    // seen but not acquired — unearned, never imported (spoiler
                    // posture). The room is the FIRST non-empty chain entry.
                    QString room;
                    for (const QString& entry : thisChain) {
                        if (!entry.isEmpty()) {
                            room = entry;
                            break;
                        }
                    }
                    const bool isAuctionLot =
                        entityID->startsWith(QStringLiteral("t.")) && room.startsWith(QStringLiteral("purchases."));
                    if (isAuctionLot) {
                        unearned += 1;
                    } else {
                        const QJsonObject mutations = LenientJSON::dictionary(
                                                          payload.value(QStringLiteral("Mutations")))
                                                          .value_or(QJsonObject());
                        if (entityID->startsWith(QStringLiteral("uncatbook."))) {
                            uncatalogued += 1;
                        } else if (stacks.contains(*entityID)) {
                            // Merge duplicate copies (committed/uncommitted skills,
                            // moved books).
                            QJsonObject existing = stacks.value(*entityID);
                            for (auto keyIt = mutations.constBegin(); keyIt != mutations.constEnd();
                                 ++keyIt) {
                                if (!existing.contains(keyIt.key()))
                                    existing.insert(keyIt.key(), keyIt.value());
                            }
                            stacks.insert(*entityID, existing);
                        } else {
                            QJsonObject cleaned = mutations;
                            cleaned.remove(QStringLiteral("$type"));
                            stacks.insert(*entityID, cleaned);
                        }
                        if (!locations.contains(*entityID))
                            locations.insert(*entityID, locationLabel(thisChain).value_or(QString()));
                    }
                }
            }
        }
        for (const QJsonObject& dominion :
             LenientJSON::dictionaries(payload.value(QStringLiteral("Dominions")))) {
            for (const QJsonObject& nested :
                 LenientJSON::dictionaries(dominion.value(QStringLiteral("Spheres")))) {
                collectStacks(nested, stacks, locations, uncatalogued, unearned, thisChain);
            }
        }
    }
}

QString SaveImporter::roomLabel(const QString& id)
{
    /// Room-level names the save uses for in-transit items: purchases.* =
    /// Oriflamme's auction wins awaiting shelving; portage<N> = items being
    /// carried between rooms (runtime spheres); fixedverbs = mid-task output.
    if (id.startsWith(QStringLiteral("purchases.")))
        return QStringLiteral("Oriflamme's auction (awaiting shelving)");
    if (id.startsWith(QStringLiteral("portage")))
        return QStringLiteral("in portage (player inventory)");
    if (id == QStringLiteral("fixedverbs"))
        return QStringLiteral("in a task's output sphere");
    return id;
}

std::optional<QString> SaveImporter::locationLabel(const QStringList& chain)
{
    /// "Library — shelf D.3" / "Library — scroll slot 2" / "purchases.europe —
    /// desk Mid": first non-empty spec id = the room, last = the slot; game
    /// slot ids humanized.
    QStringList parts;
    for (const QString& entry : chain) {
        if (!entry.isEmpty())
            parts << entry;
    }
    if (parts.isEmpty())
        return std::nullopt;
    const QString room = roomLabel(parts.first());
    if (parts.size() <= 1)
        return room;
    const QString slot = parts.last();
    if (slot.startsWith(QStringLiteral("ShelfSpaceSphere")))
        return QStringLiteral("%1 — shelf %2").arg(room, slot.mid(int(QString("ShelfSpaceSphere").size())));
    if (slot.startsWith(QStringLiteral("ScrollSlot")))
        return QStringLiteral("%1 — scroll slot %2").arg(room, slot.mid(int(QString("ScrollSlot").size())));
    if (slot.startsWith(QStringLiteral("ShelfSpaceDesk")))
        return QStringLiteral("%1 — desk %2").arg(room, slot.mid(int(QString("ShelfSpaceDesk").size())));
    return QStringLiteral("%1 — %2").arg(room, slot);
}

QHash<QString, QJsonObject> SaveImporter::elementIndex(const QString& directory)
{
    QHash<QString, QJsonObject> index;
    const QDir dir(directory);
    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString& fileName : files) {
        QFile file(dir.filePath(fileName));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const auto root = LenientJSON::object(file.readAll());
        if (!root)
            continue;
        for (const QJsonObject& element :
             LenientJSON::dictionaries(root->value(QStringLiteral("elements")))) {
            const auto id = LenientJSON::string(element.value(QStringLiteral("ID")));
            const auto lowerID = id ? id : LenientJSON::string(element.value(QStringLiteral("id")));
            if (lowerID)
                index.insert(*lowerID, element);
        }
    }
    return index;
}

QString SaveImporter::principleKey(int value, const QJsonObject& definition)
{
    /// The aspect key whose value is `value` — a level-1 skill's primary carries
    /// 2, its secondary 1 (docs/GAME_MECHANICS.md §Skills).
    const QJsonObject aspects =
        LenientJSON::dictionary(definition.value(QStringLiteral("aspects"))).value_or(QJsonObject());
    QStringList candidates;
    for (auto keyIt = aspects.constBegin(); keyIt != aspects.constEnd(); ++keyIt) {
        const auto aspectValue = LenientJSON::intValue(keyIt.value());
        if (!aspectValue || *aspectValue != value)
            continue;
        const QString key = keyIt.key();
        // Skip markers: skill, skill.language, wisdom options (w.*), evolves
        // (e.*), boosts.
        if (key != key.toLower())
            continue;
        if (key.startsWith(QStringLiteral("w.")) || key.startsWith(QStringLiteral("e."))
            || key.startsWith(QStringLiteral("boost.")))
            continue;
        if (key == QStringLiteral("skill") || key == QStringLiteral("skill.language"))
            continue;
        candidates << key;
    }
    std::sort(candidates.begin(), candidates.end());
    return candidates.isEmpty() ? QString() : candidates.first();
}

QString SaveImporter::languageName(const QString& aspectSuffix,
                                   const QHash<QString, QJsonObject>& index)
{
    if (nativeLanguageNames().contains(aspectSuffix)) {
        return aspectSuffix.at(0).toUpper() + aspectSuffix.mid(1);
    }
    const auto it = index.constFind(QStringLiteral("s.") + aspectSuffix);
    if (it != index.constEnd()) {
        if (const auto label = LenientJSON::string(it->value(QStringLiteral("Label"))))
            return *label;
    }
    return aspectSuffix;
}

QString SaveImporter::elementName(const QString& code)
{
    return elementCodes().value(code, QString());
}

MemoryDraft SaveImporter::memoryDraft(const QString& element, const QJsonObject& definition,
                                      const QHash<QString, qint64>& principleIDByName)
{
    MemoryDraft draft;
    draft.name = LenientJSON::string(definition.value(QStringLiteral("Label"))).value_or(element);
    const QString inherits =
        LenientJSON::string(definition.value(QStringLiteral("inherits"))).value_or(QString());
    MemoryKind kind = MemoryKind::Memory;
    if (inherits.contains(QStringLiteral("numen")))
        kind = MemoryKind::Numen;
    else if (inherits.contains(QStringLiteral("weather")))
        kind = MemoryKind::Weather;
    draft.kind = kind;
    draft.persistent = inherits.contains(QStringLiteral("persistent")) || kind == MemoryKind::Numen;
    const QJsonObject aspects =
        LenientJSON::dictionary(definition.value(QStringLiteral("aspects"))).value_or(QJsonObject());
    for (auto keyIt = aspects.constBegin(); keyIt != aspects.constEnd(); ++keyIt) {
        if (keyIt.key().startsWith(QStringLiteral("boost.")))
            continue;
        const auto level = LenientJSON::intValue(keyIt.value());
        if (!level || *level <= 0)
            continue;
        if (!principleIDByName.contains(keyIt.key().toLower()))
            continue;
        draft.aspects.push_back(AspectDraft{principleIDByName.value(keyIt.key().toLower()), *level});
    }
    return draft;
}

BookKind SaveImporter::bookKind(const QJsonObject& aspects)
{
    /// Kind markers in tomes.json (verified against 281 installed tomes):
    /// `codex` is the plain bound-book format — NOT a phonograph record;
    /// records carry `record.phonograph` (migration 007 repaired the old mapping).
    if (aspects.contains(QStringLiteral("tablet")))
        return BookKind::Book; // tablets stay "book" (kind set is book/scroll/film/record)
    if (aspects.contains(QStringLiteral("film")))
        return BookKind::Film;
    if (aspects.contains(QStringLiteral("record.phonograph")))
        return BookKind::Record;
    if (aspects.contains(QStringLiteral("scroll")))
        return BookKind::Scroll;
    return BookKind::Book;
}

} // namespace boh
