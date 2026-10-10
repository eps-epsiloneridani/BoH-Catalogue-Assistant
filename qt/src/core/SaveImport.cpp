#include "SaveImport.h"

#include <QDir>
#include <QFile>
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

// MARK: - Standard paths

std::optional<QString> BoHPaths::saveDirectory()
{
    const QString override = qEnvironmentVariable("BOH_SAVE_DIR");
    if (!override.isEmpty())
        return override;
    // Linux Steam candidates arrive with the importer (plan Task 8).
    return std::nullopt;
}

std::optional<QString> BoHPaths::gameElementsDirectory()
{
    const QString override = qEnvironmentVariable("BOH_GAME_ELEMENTS");
    if (!override.isEmpty())
        return override;
    // Linux Steam candidates arrive with the importer (plan Task 8).
    return std::nullopt;
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

} // namespace boh
