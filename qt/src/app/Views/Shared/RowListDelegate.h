// Shared row painter for list panes: bold title (DisplayRole), AA subtitle,
// tinted aspect badges on the right (reserved width — titles never under
// badges). Used by the Memories and Skills lists.
#pragma once

#include <QStyledItemDelegate>

namespace boh {

class RowListDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    static constexpr int kSubtitleRole = Qt::UserRole + 1;
    static constexpr int kBadgesRole = Qt::UserRole + 2;
    static constexpr int kTintsRole = Qt::UserRole + 3;
};

} // namespace boh
