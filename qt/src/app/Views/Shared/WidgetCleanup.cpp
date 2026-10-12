#include "WidgetCleanup.h"

#include <QLayout>
#include <QLayoutItem>
#include <QWidget>

namespace boh {

namespace {
void clearItem(QLayoutItem* item);
}

void clearLayoutWidgets(QLayout* layout)
{
    if (!layout)
        return;
    while (layout->count() > 0) { // guard: probing an empty QFormLayout warns
        QLayoutItem* item = layout->takeAt(0);
        if (!item)
            break;
        clearItem(item);
        delete item;
    }
}

namespace {
void clearItem(QLayoutItem* item)
{
    if (!item)
        return;
    if (auto* widget = item->widget()) {
        widget->setParent(nullptr); // detach + hide now; destruction deferred
        widget->deleteLater();
        return;
    }
    if (auto* childLayout = item->layout()) {
        clearLayoutWidgets(childLayout); // recurse: nested layouts' widgets too
        return;
    }
    // (spacer items: nothing to clean)
}
} // namespace

} // namespace boh