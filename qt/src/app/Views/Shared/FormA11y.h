// D12 helper: QFormLayout labels only reach assistive tech when they're the
// field's buddy — attach them in one call after building a form.
#pragma once

class QFormLayout;

namespace boh {
void attachFormBuddies(QFormLayout* form);
}
