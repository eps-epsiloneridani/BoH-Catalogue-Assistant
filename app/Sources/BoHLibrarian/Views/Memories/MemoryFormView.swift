import SwiftUI
import BoHLibrarianCore

// Add/Edit memory sheet — same shape as the record-read quick-add, shared AspectEditor.

struct MemoryFormView: View {
    enum Mode {
        case add
        case edit(Memory)
    }

    let mode: Mode
    let principles: [Principle]
    let onSave: (MemoryDraft) -> Void

    @Environment(\.dismiss) private var dismiss

    @State private var name = ""
    @State private var kind: MemoryKind = .memory
    @State private var persistent = false
    @State private var notes = ""
    @State private var aspectRows: [AspectDraftRow] = [AspectDraftRow()]

    init(mode: Mode, principles: [Principle], onSave: @escaping (MemoryDraft) -> Void) {
        self.mode = mode
        self.principles = principles
        self.onSave = onSave

        if case .edit(let memory) = mode {
            _name = State(initialValue: memory.name)
            _kind = State(initialValue: memory.kind)
            _persistent = State(initialValue: memory.persistent)
            _notes = State(initialValue: memory.notes ?? "")
            _aspectRows = State(initialValue: memory.aspects.map {
                AspectDraftRow(principleID: $0.principleID, level: $0.level)
            })
        }
    }

    private var canSave: Bool {
        !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }

    var body: some View {
        Form {
            Section("The memory") {
                TextField("Name", text: $name)
                Picker("Kind", selection: $kind) {
                    ForEach(MemoryKind.allCases, id: \.self) { kind in
                        Text(kind.rawValue.capitalized).tag(kind)
                    }
                }
                .pickerStyle(.segmented)
                Toggle("Persistent (survives dawn)", isOn: $persistent)
            }

            Section("Aspects") {
                AspectEditor(principles: principles, rows: $aspectRows)
                Text("Rows without a principle are skipped on save.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section("Notes") {
                TextEditor(text: $notes)
                    .frame(minHeight: 64)
            }

            Section {
                HStack {
                    Spacer()
                    Button("Cancel", role: .cancel) { dismiss() }
                    Button(saveTitle) { save() }
                        .buttonStyle(.borderedProminent)
                        .disabled(!canSave)
                }
            }
        }
        .formStyle(.grouped)
        .frame(minWidth: 440, minHeight: 460)
    }

    private var saveTitle: String {
        if case .add = mode { "Add Memory" } else { "Save" }
    }

    private func save() {
        let trimmedName = name.trimmingCharacters(in: .whitespacesAndNewlines)
        let aspects = aspectRows.compactMap { row -> AspectDraft? in
            guard let principleID = row.principleID else { return nil }
            return AspectDraft(principleID: principleID, level: row.level)
        }
        let trimmedNotes = notes.trimmingCharacters(in: .whitespacesAndNewlines)
        onSave(MemoryDraft(name: trimmedName, kind: kind, persistent: persistent,
                           notes: trimmedNotes.isEmpty ? nil : trimmedNotes,
                           aspects: aspects))
        dismiss()
    }
}