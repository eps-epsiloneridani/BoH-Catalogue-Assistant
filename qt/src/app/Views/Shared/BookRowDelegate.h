// The Books list row delegate: title + subtitle (status · language · set) +
// mystery badge on the right, announced as ONE element (D12: composite rows
// carry a crafted accessible name via Qt::AccessibleTextRole).
#pragma once

#include <QStyledItemDelegate>

namespace boh {

class BookRowDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit BookRowDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    // Data roles used from QListWidget items.
    static constexpr int kBookIDRole = Qt::UserRole + 1;
    static constexpr int kTitleRole = Qt::UserRole + 2;
    static constexpr int kSubtitleRole = Qt::UserRole + 3;
    static constexpr int kBadgeTextRole = Qt::UserRole + 4;
    static constexpr int kBadgeTintRole = Qt::UserRole + 5;
    static constexpr int kStatusRole = Qt::UserRole + 6;
};

} // namespace boh
