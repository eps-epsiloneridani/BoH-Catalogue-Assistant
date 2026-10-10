// Plan Task 9: the sidebar — playthrough switcher on top, the five sections
// below (Books · Memories · Skills · Journal · Reading Helper).
#pragma once

#include <QListWidget>
#include <QWidget>

class QComboBox;

namespace boh {

class Sidebar final : public QWidget {
    Q_OBJECT

public:
    explicit Sidebar(QWidget* parent = nullptr);

    void setPlaythroughs(const QStringList& names, int activeIndex);
    void setCurrentSection(int index);

signals:
    void playthroughSelected(int index);
    void sectionSelected(int index);

private:
    QComboBox* m_playthroughs = nullptr;
    QListWidget* m_sections = nullptr;
};

} // namespace boh
