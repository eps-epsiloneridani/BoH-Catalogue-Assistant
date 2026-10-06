import SwiftUI
import BoHLibrarianCore

// Add/Edit memory sheet — same shape as the record-read quick-add, shared
// AspectEditor. In edit mode it is ALSO the home of the source ("how to obtain")
// and yielding-book editors; the detail pane is display-only (user request).

struct MemoryFormView: View {
    enum Mode {
        case add
        case edit(Memory)
    }

    /// One editable "how to obtain" row; local row identity keeps the ForEach
    /// stable while (kind, detail) pairs change.
    struct SourceRow: Identifiable {
        let id = UUID()
        var kind: String
        var detail: String?
    }

    let mode: Mode
    let principles: [Principle]
    /// All books in the playthrough — the link picker's options (edit mode).
    var books: [BookRef] = []
    let onSave: (MemoryDraft, [MemorySource], [Int64]) -> Void

    @Environment(\.dismiss) private var dismiss

    @State private var name = ""
    @State private var kind: MemoryKind = .memory
    @State private var persistent = false
    @State private var notes = ""
    @State private var aspectRows: [AspectDraftRow] = [AspectDraftRow()]
    @State private var sourceRows: [SourceRow] = []
    @State private var linkedBookIDs: [Int64] = []

    init(mode: Mode, principles: [Principle],
         books: [BookRef] = [],
         sources: [MemorySource] = [],
         linked: [BookRef] = [],
         onSave: @escaping (MemoryDraft, [MemorySource], [Int64]) -> Void) {
        self.mode = mode
        self.principles = principles
        self.books = books
        self.onSave = onSave

        if case .edit(let memory) = mode {
            _name = State(initialValue: memory.name)
            _kind = State(initialValue: memory.kind)
            _persistent = State(initialValue: memory.persistent)
            _notes = State(initialValue: memory.notes ?? "")
            _aspectRows = State(initialValue: memory.aspects.map {
                AspectDraftRow(principleID: $0.principleID, level: $0.level)
            })
            _sourceRows = State(initialValue: sources.map {
                SourceRow(kind: $0.kind, detail: $0.detail)
            })
            _linkedBookIDs = State(initialValue: linked.map(\.id))
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

            if case .edit = mode {
                Section("How to obtain") {
                    ForEach($sourceRows) { $source in
                        HStack {
                            Picker("Source kind", selection: $source.kind) {
                                ForEach(MemorySourceKind.allCases, id: \.self) { kind in
                                    Text(kind.rawValue).tag(kind.rawValue)
                                }
                            }
                            TextField("Detail (optional)",
                                      text: Binding(get: { source.detail ?? "" },
                                                    set: { source.detail = $0.isEmpty ? nil : $0 }))
                            Button {
                                sourceRows.removeAll { $0.id == source.id }
                            } label: {
                                Image(systemName: "minus.circle")
                            }
                            .buttonStyle(.borderless)
                            .accessibilityLabel("Remove source")
                            .help("Remove source")
                        }
                    }
                    Button("Add source") {
                        sourceRows.append(SourceRow(kind: MemorySourceKind.consider.rawValue))
                    }
                }

                Section("Books that yield this") {
                    ForEach(linkedBookIDs.sorted { title(for: $0) < title(for: $1) }, id: \.self) { id in
                        HStack {
                            Image(systemName: "book")
                                .foregroundStyle(.secondary)
                                .accessibilityHidden(true)
                            Text(title(for: id))
                            Spacer()
                            Button {
                                linkedBookIDs.removeAll { $0 == id }
                            } label: {
                                Image(systemName: "minus.circle")
                            }
                            .buttonStyle(.borderless)
                            .accessibilityLabel("Unlink \(title(for: id))")
                            .help("Unlink this book")
                        }
                    }
                    Menu {
                        ForEach(linkableBooks) { book in
                            Button(book.title) { linkedBookIDs.append(book.id) }
                        }
                    } label: {
                        Label("Link a book…", systemImage: "link")
                    }
                    .disabled(linkableBooks.isEmpty)
                }
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

    private var linkableBooks: [BookRef] {
        books.filter { book in !linkedBookIDs.contains(book.id) }
    }

    private func title(for id: Int64) -> String {
        books.first { $0.id == id }?.title ?? "book \(id)"
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
                           aspects: aspects),
               sourceRows.map { MemorySource(kind: $0.kind, detail: $0.detail) },
               linkedBookIDs)
        dismiss()
    }
}