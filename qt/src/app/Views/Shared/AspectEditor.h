// Plan Task 12/13: rows of (principle, level) with add/remove — the quick-add
// memory and the memory/skill edit forms share it. Remove buttons carry
// accessible names (D12).
#pragma once

#include "Models.h"

#include <QWidget>

#include <vector>

class QComboBox;
class QSpinBox;
class QVBoxLayout;

namespace boh {

class AspectEditor final : public QWidget {
    Q_OBJECT

public:
    explicit AspectEditor(const std::vector<Principle>& principles, QWidget* parent = nullptr);

    std::vector<AspectDraft> aspects() const;

private:
    void addRow();
    void removeRow(QWidget* row);

    std::vector<Principle> m_principles;
    QVBoxLayout* m_rows = nullptr;
};

} // namespace boh
