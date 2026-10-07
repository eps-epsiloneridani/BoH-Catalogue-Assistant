import SwiftUI
import BoHLibrarianCore

// Playthrough lifecycle UI: the sidebar switcher, and the New / Manage sheets.
// The db keeps every playthrough; "saving" is automatic — switching away leaves
// the data intact, and git commits version it.

// MARK: - Sidebar menu

struct PlaythroughMenu: View {
    @Environment(AppState.self) private var appState

    var body: some View {
        Menu {
            ForEach(appState.playthroughs) { playthrough in
                let isActive = playthrough.id == appState.activePlaythrough?.id
                Button {
                    if !isActive { appState.switchPlaythrough(to: playthrough.id) }
                } label: {
                    if isActive {
                        Label(playthrough.name, systemImage: "checkmark")
                    } else {
                        Text(playthrough.name)
                    }
                }
            }
            Divider()
            Button("New Playthrough…") { showingNew = true }
            Button("Manage Playthroughs…") { showingManage = true }
        } label: {
            HStack(spacing: 6) {
                Image(systemName: "person.crop.circle")
                    .accessibilityHidden(true)
                Text(appState.activePlaythrough?.name ?? "No playthrough")
                    .lineLimit(1)
                Spacer()
                Image(systemName: "chevron.up.chevron.down")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .accessibilityHidden(true)
            }
            .contentShape(.rect)
            .accessibilityLabel("Playthrough: \(appState.activePlaythrough?.name ?? "none")")
            .accessibilityHint("Save, load and create playthroughs")
        }
        .menuStyle(.button)
        .fixedSize()
        .padding(.vertical, 2)
        .buttonStyle(.plain)
        .help("Save, load and create playthroughs")
        .sheet(isPresented: $showingNew) { NewPlaythroughSheet() }
        .sheet(isPresented: $showingManage) { ManagePlaythroughsSheet() }
        .alert("Playthrough problem", isPresented: errorBinding) {
            Button("OK") { appState.playthroughError = nil }
        } message: {
            Text(appState.playthroughError ?? "")
        }
    }

    @State private var showingNew = false
    @State private var showingManage = false

    private var errorBinding: Binding<Bool> {
        Binding(get: { appState.playthroughError != nil },
                set: { if !$0 { appState.playthroughError = nil } })
    }
}

// MARK: - New playthrough sheet

struct NewPlaythroughSheet: View {
    @Environment(AppState.self) private var appState
    @Environment(\.dismiss) private var dismiss

    @State private var name = ""
    @State private var notes = ""

    var body: some View {
        Form {
            Section {
                TextField("Name (e.g. The Twice-Born, Autumn 1936)", text: $name)
                TextField("Notes — librarian, background, anything to remember",
                          text: $notes, axis: .vertical)
                    .lineLimit(1...3)
            } footer: {
                Text("Your current playthrough is saved exactly as it is — everything "
                     + "recorded so far stays put, and switching back is one click.")
            }
            Section {
                HStack {
                    Spacer()
                    Button("Cancel", role: .cancel) { dismiss() }
                    Button("Create & Switch") {
                        if appState.createPlaythrough(name: name, notes: notes) {
                            dismiss()
                        }
                    }
                    .buttonStyle(.borderedProminent)
                }
            }
        }
        .formStyle(.grouped)
        .frame(minWidth: 420, minHeight: 260)
    }
}

// MARK: - Manage sheet

// Plain layout, not a grouped Form: rows left-justify naturally (user request),
// and the rename TextField commits on blur as well as Return — a grouped Form +
// onSubmit-only rename lost every change that wasn't followed by Return (the
// repeated grouped-Form lesson from the memory editors, now applied here too).

struct ManagePlaythroughsSheet: View {
    @Environment(AppState.self) private var appState
    @Environment(\.dismiss) private var dismiss

    @State private var confirmingDelete: Playthrough?
    @State private var showingImport = false

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    HStack {
                        Button {
                            showingImport = true
                        } label: {
                            Label("Import from Save…", systemImage: "square.and.arrow.down")
                        }
                        .help("Populate a playthrough from a Book of Hours save game")
                        Spacer()
                    }

                    Text("Reads save games from ~/Library/Application Support/Weather Factory/Book of Hours. "
                         + "Importing from an arbitrary path needs a file picker — parked for a later build.")
                        .font(.caption)
                        .foregroundStyle(.secondary)

                    Divider()

                    ForEach(appState.playthroughs) { playthrough in
                        PlaythroughRow(
                            playthrough: playthrough,
                            isActive: playthrough.id == appState.activePlaythrough?.id,
                            canDelete: appState.playthroughs.count > 1,
                            onRename: { appState.renamePlaythrough(playthrough, to: $0) },
                            onActivate: { appState.switchPlaythrough(to: playthrough.id) },
                            onDelete: { confirmingDelete = playthrough }
                        )
                        Divider()
                    }

                    Text("Deleting a playthrough removes every book, memory, skill and journal "
                         + "entry recorded in it — permanently. The active playthrough can’t be deleted. "
                         + "Changes (rename, load, delete, import) apply immediately — the buttons just close.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                .padding(20)
            }

            Divider()
            HStack {
                Spacer()
                Button("Cancel", role: .cancel) { dismiss() }
                Button("OK") { dismiss() }
                    .buttonStyle(.borderedProminent)
            }
            .padding(.horizontal, 20)
            .padding(.vertical, 12)
        }
        .frame(minWidth: 520, minHeight: 360)
        .sheet(isPresented: $showingImport) {
            ImportFromSaveSheet()
        }
        .confirmationDialog(
            "Delete “\(confirmingDelete?.name ?? "")”?",
            isPresented: Binding(get: { confirmingDelete != nil },
                                 set: { if !$0 { confirmingDelete = nil } }),
            titleVisibility: .visible
        ) {
            Button("Delete Playthrough & All Its Findings", role: .destructive) {
                if let playthrough = confirmingDelete {
                    appState.deletePlaythrough(playthrough)
                }
                confirmingDelete = nil
            }
            Button("Cancel", role: .cancel) { confirmingDelete = nil }
        } message: {
            Text("This cannot be undone (short of restoring a committed Boh.db from git).")
        }
    }
}

private struct PlaythroughRow: View {
    let playthrough: Playthrough
    let isActive: Bool
    let canDelete: Bool
    var onRename: (String) -> Void
    var onActivate: () -> Void
    var onDelete: () -> Void

    @FocusState private var nameFocused: Bool
    @State private var name: String

    init(playthrough: Playthrough, isActive: Bool, canDelete: Bool,
         onRename: @escaping (String) -> Void, onActivate: @escaping () -> Void,
         onDelete: @escaping () -> Void) {
        self.playthrough = playthrough
        self.isActive = isActive
        self.canDelete = canDelete
        self.onRename = onRename
        self.onActivate = onActivate
        self.onDelete = onDelete
        _name = State(initialValue: playthrough.name)
    }

    /// Commit on blur AND Return — an edit that never returned still lands when
    /// focus moves (to OK, to a Load button, to another row).
    private func commitRename() {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty, trimmed != playthrough.name else {
            name = playthrough.name   // blanked or unchanged: restore the real name
            return
        }
        onRename(trimmed)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 8) {
                Image(systemName: isActive ? "checkmark.circle.fill" : "circle")
                    .foregroundStyle(isActive ? .green : .secondary)
                    .accessibilityLabel(isActive ? "loaded" : "not loaded")
                    .help(isActive ? "Loaded" : "Not loaded")
                TextField("Name", text: $name)
                    .focused($nameFocused)
                    .onSubmit { commitRename() }
                Spacer()
                Text(playthrough.createdAt)
                    .font(.caption)
                    .foregroundStyle(.tertiary)
                if !isActive {
                    Button("Load") { onActivate() }
                }
                Button(role: .destructive) { onDelete() } label: {
                    Image(systemName: "trash")
                }
                .buttonStyle(.borderless)
                .accessibilityLabel("Delete this playthrough")
                .disabled(isActive || !canDelete)
                .help(isActive ? "Switch away before deleting"
                              : canDelete ? "Delete this playthrough and its findings"
                                          : "Can't delete the only playthrough")
            }
            if let notes = playthrough.notes, !notes.isEmpty {
                Text(notes)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
            }
        }
        .onChange(of: nameFocused) { _, focused in
            if !focused { commitRename() }
        }
        // Clicking OK (or Closing the sheet) can leave focus ON the field on
        // this macOS build - blur never fires - so also commit when the row
        // leaves the screen; unchanged names are no-ops.
        .onDisappear { commitRename() }
    }
}