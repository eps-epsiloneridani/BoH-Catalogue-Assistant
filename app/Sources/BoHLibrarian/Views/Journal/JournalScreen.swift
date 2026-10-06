import SwiftUI
import BoHLibrarianCore

// The Journal: reverse-chronological findings with in-game day headers, a
// quick-capture box (⌘⇧J), entity-link chips, edit and delete.

struct JournalScreen: View {
    let store: JournalStore

    @Environment(AppState.self) private var appState

    @State private var quickAddText = ""
    @State private var editingEntry: JournalEntry?
    @FocusState private var quickAddFocused: Bool

    init(store: JournalStore) {
        self.store = store
    }

    var body: some View {
        @Bindable var store = store
        return Group {
            if store.entries.isEmpty {
                emptyState
            } else {
                content
            }
        }
        .frame(minWidth: 640)
        .searchable(text: $store.searchText, placement: .toolbar,
                    prompt: "Search the journal…")
        .alert("Something went wrong", isPresented: errorBinding) {
            Button("OK") { store.lastError = nil }
        } message: {
            Text(store.lastError ?? "")
        }
        .sheet(item: $editingEntry, onDismiss: { store.reload() }) { entry in
            JournalEditSheet(entry: entry, store: store)
        }
        .onChange(of: store.requestFocus) { _, requested in
            if requested {
                quickAddFocused = true
                store.requestFocus = false
            }
        }
    }

    private var errorBinding: Binding<Bool> {
        Binding(get: { store.lastError != nil },
                set: { if !$0 { store.lastError = nil } })
    }

    // MARK: Layout

    private var content: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                quickAdd
                Divider()
                timeline
            }
            .padding(20)
            .frame(maxWidth: 820, alignment: .leading)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .onAppear { store.reload() }
    }

    // MARK: Quick capture

    private var quickAdd: some View {
        @Bindable var appState = appState
        return VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 8) {
                Image(systemName: "plus.circle")
                    .foregroundStyle(.secondary)
                TextField("Note today's finding…", text: $quickAddText)
                    .focused($quickAddFocused)
                    .onSubmit(submitQuickAdd)
            }
            HStack {
                Text("logged to \"\(gameDayLabel)\"")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Spacer()
                Button("Add Note", action: submitQuickAdd)
                    .buttonStyle(.borderedProminent)
                    .disabled(quickAddText.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
        }
    }

    private var gameDayLabel: String {
        let day = appState.currentGameDay.trimmingCharacters(in: .whitespacesAndNewlines)
        return day.isEmpty ? "no in-game day set" : day
    }

    private func submitQuickAdd() {
        let day = appState.currentGameDay.trimmingCharacters(in: .whitespacesAndNewlines)
        store.quickAdd(text: quickAddText, gameDay: day.isEmpty ? nil : day)
        quickAddText = ""
    }

    // MARK: Timeline

    private var timeline: some View {
        VStack(alignment: .leading, spacing: 4) {
            ForEach(store.rows) { row in
                if let header = row.header {
                    Text(header.uppercased())
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.secondary)
                        .padding(.top, 10)
                        .padding(.bottom, 2)
                }
                JournalRowView(entry: row.entry, store: store,
                               onEdit: { editingEntry = row.entry },
                               onDelete: { store.delete(row.entry) })
            }
            if store.rows.isEmpty {
                Text("No entries match the search.")
                    .font(.callout)
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity)
                    .padding(.top, 20)
            }
        }
    }

    private var emptyState: some View {
        ContentUnavailableView {
            Label("Nothing in the journal yet", systemImage: "note.text")
        } description: {
            Text("Jot anything worth remembering — the record-read flow and the book screens write here automatically, and anything you note yourself lands above.")
        } actions: {
            Button("Note today's finding") { quickAddFocused = true }
                .keyboardShortcut(.defaultAction)
        }
        .onChange(of: store.requestFocus) { _, requested in
            if requested {
                store.requestFocus = false
            }
        }
    }
}

// MARK: - Entry row

private struct JournalRowView: View {
    let entry: JournalEntry
    let store: JournalStore
    var onEdit: () -> Void
    var onDelete: () -> Void

    var body: some View {
        HStack(alignment: .top, spacing: 8) {
            VStack(alignment: .leading, spacing: 4) {
                Text(entry.entry)
                    .font(.callout)
                if !linkChips.isEmpty {
                    HStack(spacing: 6) {
                        ForEach(linkChips) { chip in
                            Label(chip.title, systemImage: chip.icon)
                                .font(.caption2)
                                .padding(.horizontal, 6)
                                .padding(.vertical, 2)
                                .background(Capsule().fill(.quaternary.opacity(0.5)))
                                .foregroundStyle(.secondary)
                        }
                    }
                }
                Text(entry.loggedAt)
                    .font(.caption2)
                    .foregroundStyle(.tertiary)
            }
            Spacer(minLength: 8)
            Button(action: onEdit) {
                Image(systemName: "pencil")
            }
            .buttonStyle(.borderless)
            .help("Edit entry")
            Button(role: .destructive, action: onDelete) {
                Image(systemName: "trash")
            }
            .buttonStyle(.borderless)
            .help("Delete entry")
        }
        .padding(.vertical, 4)
    }

    private struct Chip: Identifiable {
        let id: String
        let title: String
        let icon: String
    }

    private var linkChips: [Chip] {
        var chips: [Chip] = []
        if let bookID = entry.bookID, let title = store.bookTitle(bookID) {
            chips.append(Chip(id: "b\(bookID)", title: title, icon: "book"))
        }
        if let memoryID = entry.memoryID, let name = store.memoryName(memoryID) {
            chips.append(Chip(id: "m\(memoryID)", title: name, icon: "sparkles"))
        }
        if let skillID = entry.skillID, let name = store.skillName(skillID) {
            chips.append(Chip(id: "s\(skillID)", title: name, icon: "graduationcap"))
        }
        return chips
    }
}