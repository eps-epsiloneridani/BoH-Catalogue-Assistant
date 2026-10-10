#include "BookRowDelegate.h"

#include "Badges.h"

#include <QPainter>

namespace boh {

BookRowDelegate::BookRowDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void BookRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const
{
    painter->save();
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.color(QPalette::Highlight));
        painter->setPen(option.palette.color(QPalette::HighlightedText));
    } else {
        painter->setPen(option.palette.color(QPalette::WindowText));
    }

    const QString title = index.data(kTitleRole).toString();
    const QString subtitle = index.data(kSubtitleRole).toString();
    const QString badgeText = index.data(kBadgeTextRole).toString();
    const QString badgeTint = index.data(kBadgeTintRole).toString();

    const QRect row = option.rect.adjusted(8, 4, -8, -4);
    QFont titleFont = option.font;
    titleFont.setBold(true);

    // Badge on the right, vertically centered.
    QRect badgeRect;
    if (!badgeText.isEmpty()) {
        const QSize size = Badges::badgeSize(badgeText, option.fontMetrics);
        badgeRect = QRect(row.right() - size.width(), row.top() + (row.height() - size.height()) / 2,
                          size.width(), size.height());
    }

    const int textWidth = row.width() - (badgeRect.isValid() ? badgeRect.width() + 8 : 0);
    painter->setFont(titleFont);
    painter->drawText(row.adjusted(0, 2, -textWidth - row.width(), 0), Qt::AlignLeft | Qt::AlignTop,
                      option.fontMetrics.elidedText(title, Qt::ElideRight, textWidth));

    QFont subFont = option.font;
    subFont.setPointSizeF(subFont.pointSizeF() * 0.9);
    painter->setFont(subFont);
    painter->setPen(option.state & QStyle::State_Selected
                        ? option.palette.color(QPalette::HighlightedText)
                        : option.palette.color(QPalette::Mid));
    painter->drawText(row.adjusted(0, option.fontMetrics.height() + 6, -textWidth - row.width(), 0),
                      Qt::AlignLeft | Qt::AlignTop,
                      option.fontMetrics.elidedText(subtitle, Qt::ElideRight, textWidth));

    if (badgeRect.isValid())
        Badges::paint(painter, badgeRect, badgeText, badgeTint, Badges::isDarkMode(option.widget));
    painter->restore();
}

QSize BookRowDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const
{
    return {260, option.fontMetrics.height() * 2 + 14};
}

} // namespace boh
