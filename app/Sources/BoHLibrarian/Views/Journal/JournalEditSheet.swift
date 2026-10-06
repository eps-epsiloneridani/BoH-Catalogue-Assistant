import SwiftUI
import BoHLibrarianCore

// Edit one journal entry: its text, the in-game day, and optional entity links.

struct JournalEditSheet: View {
    let entry: JournalEntry
    let store: JournalStore

    @Environment(\.dismiss) private var dismiss

    @State private var text: String
    @State private var gameDay: String
    @State private var bookID: Int64?
    @State private var memoryID: Int64?
    @State private var skillID: Int64?

    init(entry: JournalEntry, store: JournalStore) {
        self.entry = entry
        self.store = store
        _text = State(initialValue: entry.entry)
        _gameDay = State(initialValue: entry.gameDay ?? "")
        _bookID = State(initialValue: entry.bookID)
        _memoryID = State(initialValue: entry.memoryID)
        _skillID = State(initialValue: entry.skillID)
    }

    private var canSave: Bool {
        !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }

    var body: some View {
        Form {
            Section("Entry") {
                TextEditor(text: $text)
                    .frame(minHeight: 90)
                TextField("In-game day (optional)", text: $gameDay)
            }

            Section("Links (optional)") {
                Picker("Book", selection: $bookID) {
                    Text("—").tag(Int64?.none)
                    ForEach(store.bookPickerList) { book in
                        Text(book.title).tag(Int64?.some(book.id))
                    }
                }
                Picker("Memory", selection: $memoryID) {
                    Text("—").tag(Int64?.none)
                    ForEach(store.memoryPickerList) { memory in
                        Text(memory.name).tag(Int64?.some(memory.id))
                    }
                }
                Picker("Skill", selection: $skillID) {
                    Text("—").tag(Int64?.none)
                    ForEach(store.skillPickerList) { skill in
                        Text(skill.name).tag(Int64?.some(skill.id))
                    }
                }
            }

            Section {
                HStack {
                    Spacer()
                    Button("Cancel", role: .cancel) { dismiss() }
                    Button("Save") { save() }
                        .buttonStyle(.borderedProminent)
                        .disabled(!canSave)
                }
            }
        }
        .formStyle(.grouped)
        .frame(minWidth: 460, minHeight: 420)
        .onAppear { store.refreshLinkData() }
    }

    private func save() {
        var updated = entry
        updated.entry = text.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedDay = gameDay.trimmingCharacters(in: .whitespacesAndNewlines)
        updated.gameDay = trimmedDay.isEmpty ? nil : trimmedDay
        updated.bookID = bookID
        updated.memoryID = memoryID
        updated.skillID = skillID
        store.update(updated)
        dismiss()
    }
}