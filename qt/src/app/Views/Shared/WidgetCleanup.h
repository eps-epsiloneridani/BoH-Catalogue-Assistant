// Remediation R5: the detail panes' teardown took layout items and deleted
// them — but deleting a QLayout does NOT delete its widgets, and nested-layout
// items never even reached the deleteLater path: every form field and button
// row leaked and stayed drawn (the "details pane keeps overwriting" bug).
// clearLayoutWidgets walks the tree recursively: every widget is detached
// (setParent(nullptr) — instantly hidden, safe from a widget's own signal
// handler) and deleteLater'd.
#pragma once

class QLayout;

namespace boh {

void clearLayoutWidgets(QLayout* layout);

} // namespace boh