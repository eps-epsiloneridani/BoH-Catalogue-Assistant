// Plan Task 12: the record-a-read dialog — the heart of the book loop. One
// form writes read status, counters, the yielded memory (creating it inline if
// new), lessons and a Journal entry, all in one transaction.
//
// Spoiler posture: "Memory used" is earned-only; "Memory gained" lists earned
// memories by name plus this book's unearned (imported) yield as a PLACEHOLDER
// — the name reveals only after the read earns it. Mastering defaults ON.
#pragma once

#include "Models.h"
#include "Stores/BooksStore.h"

#include <QDialog>

#include <optional>
#include <vector>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

namespace boh {

class AppController;
class AspectEditor;

class RecordReadDialog final : public QDialog {
    Q_OBJECT

public:
    RecordReadDialog(const Book& book, BooksStore* store, AppController* controller,
                     QWidget* parent = nullptr);

private:
    void record();
    void rebuildGainedChoices();
    QString memoryLabel(const Memory& memory) const;
    bool canRecord() const;

    const Book m_book;
    BooksStore* m_store;
    AppController* m_controller;

    std::vector<Memory> m_memories;       // full table — resolution for picked rows
    std::vector<Memory> m_earnedMemories; // "Memory used" + named "gained" rows
    std::optional<qint64> m_hiddenYield;  // this book's unearned yield, placeholder only

    QCheckBox* m_mastering = nullptr;
    QComboBox* m_usedMemory = nullptr;
    QComboBox* m_gainedChoice = nullptr;
    QComboBox* m_gainedExisting = nullptr;
    QLineEdit* m_newName = nullptr;
    QComboBox* m_newKind = nullptr;
    QCheckBox* m_newPersistent = nullptr;
    AspectEditor* m_newAspects = nullptr;
    QSpinBox* m_lessons = nullptr;
    QLineEdit* m_gameDay = nullptr;
    QLineEdit* m_note = nullptr;

    // Choice encoding: 0 = none; positive = memory id; -1 = new memory.
    static constexpr qint64 kGainedNew = -1;
};

} // namespace boh
