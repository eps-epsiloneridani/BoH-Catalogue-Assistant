#include "BookRowDelegate.h"

#include "Badges.h"
#include "ThemedText.h"

#include <QPainter>

#include <algorithm>

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

    // Text area: the row minus the badge block, as an explicit rect — never a
    // doubly-shifted adjusted() (that once produced right < left, and the list
    // painted blank rows: user bug report 3).
    const int textWidth = row.width() - (badgeRect.isValid() ? badgeRect.width() + 8 : 0);
    const QRect textArea(row.left(), row.top(), std::max(0, textWidth), row.height());

    painter->setFont(titleFont);
    painter->drawText(QRect(textArea.left(), textArea.top() + 2, textArea.width(),
                            option.fontMetrics.height()),
                      Qt::AlignLeft | Qt::AlignTop,
                      option.fontMetrics.elidedText(title, Qt::ElideRight, textWidth));

    QFont subFont = option.font;
    subFont.setPointSizeF(subFont.pointSizeF() * 0.9);
    const QFontMetrics subMetrics(subFont);
    painter->setFont(subFont);
    painter->setPen(option.state & QStyle::State_Selected
                        ? option.palette.color(QPalette::HighlightedText)
                        : ThemedText::secondaryTextColor(option.palette));
    painter->drawText(QRect(textArea.left(), textArea.top() + option.fontMetrics.height() + 6,
                            textArea.width(), subMetrics.height()),
                      Qt::AlignLeft | Qt::AlignTop,
                      subMetrics.elidedText(subtitle, Qt::ElideRight, textWidth));

    if (badgeRect.isValid())
        Badges::paint(painter, badgeRect, badgeText, badgeTint, Badges::isDarkMode(option.widget));
    painter->restore();
}

QSize BookRowDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const
{
    return {260, option.fontMetrics.height() * 2 + 14};
}

} // namespace boh