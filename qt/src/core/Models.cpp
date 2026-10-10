#include "Models.h"

#include <QHash>

namespace boh {

namespace {
/// Table-driven enum mapping — the value sets live in docs/DATABASE.md; these
/// functions are the single C++ home of their db spellings.
struct NameTable {
    QHash<QString, int> byName;
    QHash<int, QString> byValue;

    void add(int value, const char* raw)
    {
        byValue.insert(value, QString::fromUtf8(raw));
        byName.insert(QString::fromUtf8(raw), value);
    }

    template <typename E>
    QString toString(E value) const
    {
        return byValue.value(int(value));
    }

    template <typename E>
    std::optional<E> fromString(const QString& raw) const
    {
        const auto it = byName.constFind(raw);
        if (it == byName.constEnd())
            return std::nullopt;
        return E(it.value());
    }
};

const NameTable& memoryKindNames()
{
    static const NameTable table = [] {
        NameTable t;
        t.add(int(MemoryKind::Memory), "memory");
        t.add(int(MemoryKind::Weather), "weather");
        t.add(int(MemoryKind::Numen), "numen");
        return t;
    }();
    return table;
}

const NameTable& bookKindNames()
{
    static const NameTable table = [] {
        NameTable t;
        t.add(int(BookKind::Book), "book");
        t.add(int(BookKind::Scroll), "scroll");
        t.add(int(BookKind::Film), "film");
        t.add(int(BookKind::Record), "record");
        return t;
    }();
    return table;
}

const NameTable& readStatusNames()
{
    static const NameTable table = [] {
        NameTable t;
        t.add(int(ReadStatus::Uncatalogued), "uncatalogued");
        t.add(int(ReadStatus::Catalogued), "catalogued");
        t.add(int(ReadStatus::Mastered), "mastered");
        return t;
    }();
    return table;
}

const NameTable& contaminationNames()
{
    static const NameTable table = [] {
        NameTable t;
        t.add(int(Contamination::None), "none");
        t.add(int(Contamination::Curse), "curse");
        t.add(int(Contamination::Theoplasm), "theoplasmic");
        t.add(int(Contamination::Infestation), "infestation");
        t.add(int(Contamination::Corruption), "corruption");
        t.add(int(Contamination::Winkwell), "winkwell");
        t.add(int(Contamination::Witchworms), "witchworms");
        return t;
    }();
    return table;
}

const NameTable& memorySourceKindNames()
{
    static const NameTable table = [] {
        NameTable t;
        t.add(int(MemorySourceKind::ReReadBook), "re-read book");
        t.add(int(MemorySourceKind::FirstRead), "first read");
        t.add(int(MemorySourceKind::Weather), "weather");
        t.add(int(MemorySourceKind::Talk), "talk");
        t.add(int(MemorySourceKind::Consider), "consider");
        t.add(int(MemorySourceKind::Consume), "consume");
        t.add(int(MemorySourceKind::Craft), "craft");
        t.add(int(MemorySourceKind::Gather), "gather");
        t.add(int(MemorySourceKind::Numa), "numa");
        t.add(int(MemorySourceKind::Other), "other");
        return t;
    }();
    return table;
}
} // namespace

QString memoryKindToString(MemoryKind kind) { return memoryKindNames().toString(kind); }
std::optional<MemoryKind> memoryKindFromString(const QString& raw) { return memoryKindNames().fromString<MemoryKind>(raw); }

QString bookKindToString(BookKind kind) { return bookKindNames().toString(kind); }
std::optional<BookKind> bookKindFromString(const QString& raw) { return bookKindNames().fromString<BookKind>(raw); }

QString readStatusToString(ReadStatus status) { return readStatusNames().toString(status); }
std::optional<ReadStatus> readStatusFromString(const QString& raw) { return readStatusNames().fromString<ReadStatus>(raw); }

QString contaminationToString(Contamination contamination) { return contaminationNames().toString(contamination); }
std::optional<Contamination> contaminationFromString(const QString& raw) { return contaminationNames().fromString<Contamination>(raw); }

QString contaminationLabel(Contamination contamination)
{
    switch (contamination) {
    case Contamination::None: return QStringLiteral("None");
    case Contamination::Curse: return QStringLiteral("Curse");
    case Contamination::Theoplasm: return QStringLiteral("Theoplasm");
    case Contamination::Infestation: return QStringLiteral("Infestation");
    case Contamination::Corruption: return QStringLiteral("Corruption");
    case Contamination::Winkwell: return QStringLiteral("Winkwell");
    case Contamination::Witchworms: return QStringLiteral("Witchworms");
    }
    return QString();
}

QString memorySourceKindToString(MemorySourceKind kind) { return memorySourceKindNames().toString(kind); }
std::optional<MemorySourceKind> memorySourceKindFromString(const QString& raw) { return memorySourceKindNames().fromString<MemorySourceKind>(raw); }

} // namespace boh
