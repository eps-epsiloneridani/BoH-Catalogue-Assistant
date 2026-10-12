#include "RowListDelegate.h"

#include "Badges.h"
#include "ThemedText.h"

#include <QPainter>

#include <algorithm>
#include <vector>

namespace boh {

void RowListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const
{
    painter->save();
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.color(QPalette::Highlight));
        painter->setPen(option.palette.color(QPalette::HighlightedText));
    } else {
        painter->setPen(option.palette.color(QPalette::WindowText));
    }
    const QRect row = option.rect.adjusted(8, 4, -8, -4);
    const bool dark = Badges::isDarkMode(option.widget);

    // Layout FIRST (badges reserve their width), then paint.
    const QStringList badges =
        index.data(kBadgesRole).toString().split(QLatin1Char('|'), Qt::SkipEmptyParts);
    const QStringList tints = index.data(kTintsRole).toString().split(QLatin1Char('|'));
    int badgeX = row.right();
    int badgesWidth = 0;
    std::vector<QRect> badgeRects;
    for (int i = badges.size() - 1; i >= 0; --i) {
        const QSize size = Badges::badgeSize(badges[i], option.fontMetrics);
        badgeX -= size.width() + 4;
        if (badgeX < row.left())
            break;
        badgesWidth += size.width() + 4;
        badgeRects.push_back(QRect(badgeX + 4, row.top() + (row.height() - size.height()) / 2,
                                   size.width(), size.height()));
    }
    const int textWidth = row.width() - badgesWidth;
    const QRect textArea(row.left(), row.top(), std::max(0, textWidth), row.height());

    QFont titleFont = option.font;
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->drawText(QRect(textArea.left(), textArea.top() + 2, textArea.width(),
                            option.fontMetrics.height()),
                      Qt::AlignLeft | Qt::AlignTop,
                      option.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                                     Qt::ElideRight, textWidth));

    const QString subtitle = index.data(kSubtitleRole).toString();
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

    std::reverse(badgeRects.begin(), badgeRects.end());
    for (int i = 0; i < badgeRects.size(); ++i)
        Badges::paint(painter, badgeRects[i], badges[i],
                      i < tints.size() ? tints[i] : QString(), dark);
    painter->restore();
}

QSize RowListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const
{
    return {240, option.fontMetrics.height() * 2 + 14};
}

} // namespace boh
