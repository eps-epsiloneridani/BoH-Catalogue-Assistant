import SwiftUI
import BoHLibrarianCore

// "Record read" sheet — the heart of the book loop. One form writes read status,
// counters, the yielded memory (creating it inline if it's new to the records),
// lessons and a Journal entry, all in one transaction.

struct MarkAsReadSheet: View {
    let book: Book
    let store: BooksStore

    @Environment(AppState.self) private var appState
    @Environment(\.dismiss) private var dismiss

    /// Which memory the book yielded this read.
    private enum Gained: Hashable {
        case none
        case existing(Int64)
        case newMemory
    }

    /// Mastering read defaults ON (user request): the common gap-fill case — a
    /// book mastered from the form, recording the memory it yielded — is a
    /// mastering read in spirit; flip off deliberately for a pure re-read.
    @State private var mastering = true
    @State private var usedMemoryID: Int64?
    @State private var gainedChoice: Gained = .none
    @State private var gainedExistingID: Int64?
    /// Everything recorded — the "gained (existing)" pickers must see imported
    /// yields too, since selecting one is exactly what earns it.
    @State private var memories: [Memory] = []
    /// Earned only — what the read can be satisfied *with*.
    @State private var earnedMemories: [Memory] = []

    // Quick-add memory fields
    @State private var newName = ""
    @State private var newKind: MemoryKind = .memory
    @State private var newPersistent = false
    @State private var newAspects: [AspectDraftRow] = [AspectDraftRow()]

    @State private var lessons: Int?
    @State private var note = ""

    init(book: Book, store: BooksStore, preselectedMemoryID: Int64? = nil) {
        self.book = book
        self.store = store
        // Reading Helper flows can open this sheet with the chosen memory in hand.
        _usedMemoryID = State(initialValue: preselectedMemoryID)
    }

    private var canRecord: Bool {
        guard gainedChoice == .newMemory else { return true }
        return !newName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }

    var body: some View {
        Form {
            Section {
                Toggle("Mastering read (first complete read)", isOn: $mastering)
                    .disabled(book.readStatus == .mastered && !mastering)
                if book.readStatus == .mastered {
                    Text("Already mastered — recording a re-read for the memory.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            } header: {
                Text("“\(book.title)”")
            } footer: {
                if mastering {
                    Text("Marks the book as mastered and records lessons, if noted.")
                } else {
                    Text("Re-reads always give the book's memory, regardless of mystery.")
                }
            }

            Section("Reading") {
                Picker("Memory used", selection: $usedMemoryID) {
                    Text("—").tag(Int64?.none)
                    ForEach(earnedMemories) { memory in
                        Text(memoryLabel(memory)).tag(Int64?.some(memory.id))
                    }
                }
            }

            Section("What did you gain?") {
                Picker("Memory gained", selection: $gainedChoice) {
                    Text("None / not noted").tag(Gained.none)
                    ForEach(memories) { memory in
                        Text(memoryLabel(memory)).tag(Gained.existing(memory.id))
                    }
                    Text("New memory…").tag(Gained.newMemory)
                }
                if case .existing = gainedChoice {
                    Picker("Choose memory", selection: $gainedExistingID) {
                        Text("—").tag(Int64?.none)
                        ForEach(memories) { memory in
                            Text(memoryLabel(memory)).tag(Int64?.some(memory.id))
                        }
                    }
                } else if gainedChoice == .newMemory {
                    quickAddFields
                }
            }

            if mastering {
                Section("This read") {
                    Picker("Lessons granted", selection: $lessons) {
                        Text("—").tag(Int?.none)
                        ForEach([1, 2, 3], id: \.self) { Text("\($0)").tag(Int?.some($0)) }
                    }
                }
            }

            Section("Journal") {
                @Bindable var appState = appState
                TextField("In-game day (e.g. Year 1, Autumn, day 3)",
                          text: $appState.currentGameDay)
                TextField("Note (optional)", text: $note, axis: .vertical)
                    .lineLimit(1...3)
            }

            Section {
                HStack {
                    Spacer()
                    Button("Cancel", role: .cancel) { dismiss() }
                    Button("Record read") { record() }
                        .buttonStyle(.borderedProminent)
                        .disabled(!canRecord)
                }
            }
        }
        .formStyle(.grouped)
        .frame(minWidth: 480, minHeight: 520)
        .onAppear {
            memories = store.allMemories
            earnedMemories = store.earnedMemories
        }
    }

    // MARK: Quick-add memory

    private var quickAddFields: some View {
        Group {
            TextField("Memory name", text: $newName)
            Picker("Kind", selection: $newKind) {
                ForEach(MemoryKind.allCases, id: \.self) { kind in
                    Text(kind.rawValue.capitalized).tag(kind)
                }
            }
            .pickerStyle(.segmented)
            Toggle("Persistent (survives dawn)", isOn: $newPersistent)
            AspectEditor(principles: appState.principles, rows: $newAspects)
        }
    }

    // MARK: Recording

    private func record() {
        var gainedMemory: Memory?
        switch gainedChoice {
        case .none:
            gainedMemory = nil
        case .existing(let id):
            gainedMemory = memories.first { $0.id == id }
                ?? gainedExistingID.flatMap { id in memories.first { $0.id == id } }
        case .newMemory:
            let drafts = newAspects.compactMap { row -> AspectDraft? in
                guard let principleID = row.principleID else { return nil }
                return AspectDraft(principleID: principleID, level: row.level)
            }
            gainedMemory = store.createMemory(MemoryDraft(
                name: newName.trimmingCharacters(in: .whitespacesAndNewlines),
                kind: newKind, persistent: newPersistent, aspects: drafts
            ))
            if gainedMemory == nil { return }  // store.lastError set; alert shows at screen level
        }

        let day = appState.currentGameDay.trimmingCharacters(in: .whitespacesAndNewlines)
        store.recordRead(book: book,
                         mastering: mastering,
                         usedMemoryID: usedMemoryID,
                         gainedMemory: gainedMemory,
                         lessons: mastering ? lessons : nil,
                         gameDay: day.isEmpty ? nil : day,
                         note: note)
        dismiss()
    }

    private func memoryLabel(_ memory: Memory) -> String {
        var label = memory.name
        if !memory.aspects.isEmpty {
            let aspects = memory.aspects
                .map { "\($0.principleName) \($0.level)" }
                .joined(separator: ", ")
            label += "  (\(aspects))"
        }
        if memory.persistent {
            label += "  ∙ persistent"
        }
        return label
    }
}