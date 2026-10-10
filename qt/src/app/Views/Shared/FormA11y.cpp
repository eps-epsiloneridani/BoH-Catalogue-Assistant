#include "FormA11y.h"

#include <QFormLayout>
#include <QLabel>

namespace boh {

void attachFormBuddies(QFormLayout* form)
{
    for (int row = 0; row < form->rowCount(); ++row) {
        QLayoutItem* labelItem = form->itemAt(row, QFormLayout::LabelRole);
        QLayoutItem* fieldItem = form->itemAt(row, QFormLayout::FieldRole);
        if (!labelItem || !fieldItem)
            continue;
        auto* label = qobject_cast<QLabel*>(labelItem->widget());
        if (!label || label->buddy())
            continue;
        if (fieldItem->widget())
            label->setBuddy(fieldItem->widget());
    }
}

} // namespace boh
