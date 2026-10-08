import SwiftUI
import BoHLibrarianCore

// Shared aspect-row editor used by the memory screens and the record-read sheet.

/// One aspect row before saving. Rows with no principle chosen are skipped on save.
struct AspectDraftRow: Identifiable, Equatable {
    var id = UUID()
    var principleID: Int64?
    var level = 2

    init(principleID: Int64? = nil, level: Int = 2) {
        self.principleID = principleID
        self.level = level
    }
}

/// Rows of principle picker + level stepper + remove button, with an add button.
/// The caller persists after each `onChanged` mutation (or on save).
struct AspectEditor: View {
    let principles: [Principle]
    @Binding var rows: [AspectDraftRow]
    var onChanged: () -> Void = {}

    var body: some View {
        ForEach($rows) { $row in
            HStack {
                Picker("Aspect", selection: $row.principleID) {
                    Text("—").tag(Int64?.none)
                    ForEach(principles) { principle in
                        Text(principle.name).tag(Int64?.some(principle.id))
                    }
                }
                Stepper("Level \(row.level)", value: $row.level, in: 1...8)
                Button {
                    rows.removeAll { $0.id == row.id }
                    onChanged()
                } label: {
                    Image(systemName: "minus.circle")
                }
                .buttonStyle(.borderless)
                .accessibilityLabel("Remove aspect")
                .help("Remove aspect")
            }
        }
        Button {
            rows.append(AspectDraftRow())
            onChanged()
        } label: {
            Label("Add aspect", systemImage: "plus.circle")
        }
    }
}