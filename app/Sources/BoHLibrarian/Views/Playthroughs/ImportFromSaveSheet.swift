import SwiftUI
import BoHLibrarianCore

// Import from a Book of Hours save: lists save games found in the standard
// install path, previews what will be imported, and lets the user choose the
// destination playthrough. Importing from an arbitrary path needs a file
// picker — parked (docs/ROADMAP.md §Later).

struct ImportFromSaveSheet: View {
    @Environment(AppState.self) private var appState
    @Environment(\.dismiss) private var dismiss

    @State private var saves: [SaveGameSummary] = []
    @State private var scanning = true
    @State private var selected: SaveGameSummary?
    @State private var asNewPlaythrough = true
    @State private var newPlaythroughName = ""
    @State private var nameTouchedByUser = false
    @State private var importing = false
    @State private var report: ImportReport?
    @State private var showResult = false

    var body: some View {
        Form {
            savesSection
            if let selected {
                destinationSection(selected)
                actionSection(selected)
            }
        }
        .formStyle(.grouped)
        .frame(minWidth: 560, minHeight: 460)
        .task { scan() }
        .alert("Import complete", isPresented: $showResult) {
            Button("Done") { dismiss() }
        } message: {
            Text(report?.summary ?? "")
        }
    }

    // MARK: Sections

    private var savesSection: some View {
        Section {
            if scanning {
                HStack { ProgressView().controlSize(.small); Text("Looking for save games…") }
            } else if saves.isEmpty {
                ContentUnavailableView {
                    Label("No save games found", systemImage: "questionmark.folder")
                } description: {
                    Text("Looked in ~/Library/Application Support/Weather Factory/Book of Hours. "
                         + "Importing from somewhere else needs a file picker — parked for a later build.")
                }
            } else {
                ForEach(saves) { save in
                    SaveRow(summary: save, isSelected: selected?.id == save.id) {
                        select(save)
                    }
                }
            }
        } header: {
            Text("Save games")
        } footer: {
            Text("The active save is AUTOSAVE.json — the game writes it continuously. "
                 + "Manual saves appear here too.")
        }
    }

    private func destinationSection(_ selected: SaveGameSummary) -> some View {
        Section("Destination") {
            Picker("Import into", selection: $asNewPlaythrough) {
                Text("New playthrough").tag(true)
                Text("Current playthrough — \(appState.activePlaythrough?.name ?? "none")")
                    .tag(false)
            }
            .pickerStyle(.radioGroup)
            if asNewPlaythrough {
                TextField("Playthrough name", text: $newPlaythroughName,
                          prompt: Text("Imported from \(selected.stem)"))
            }
        }
    }

    private func actionSection(_ selected: SaveGameSummary) -> some View {
        Section {
            HStack {
                if importing {
                    ProgressView().controlSize(.small)
                    Text("Importing… (parsing a large save — a few moments)")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                Spacer()
                Button("Cancel", role: .cancel) { dismiss() }
                Button("Import") { runImport(selected) }
                    .buttonStyle(.borderedProminent)
                    .disabled(importing)
            }
        } header: {
            Text("Ready")
        } footer: {
            Text("Will import about \(selected.bookCount) books (\(selected.masteredCount) mastered) "
                 + "and \(selected.skillStackCount) skill records from \(selected.fileName). "
                 + "Existing entries in the destination are only ever filled in, never overwritten.")
        }
    }

    // MARK: Actions

    private func scan() {
        saves = SaveScanner.availableSaves()
        scanning = false
    }

    private func select(_ save: SaveGameSummary) {
        selected = save
        if !nameTouchedByUser {
            newPlaythroughName = "Imported from \(save.stem)"
        }
    }

    private func runImport(_ save: SaveGameSummary) {
        importing = true
        // Main-thread by design (db confinement); the import is a one-shot,
        // multi-second operation. The disabled button prevents double runs.
        if let result = appState.importSave(save, asNewPlaythrough: asNewPlaythrough,
                                            playthroughName: newPlaythroughName) {
            report = result
            showResult = true
        } else {
            dismiss()  // failure — appState.playthroughError surfaces at root level
        }
        importing = false
    }
}

private struct SaveRow: View {
    let summary: SaveGameSummary
    let isSelected: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack {
                Image(systemName: isSelected ? "checkmark.circle.fill" : "doc.text")
                    .foregroundStyle(isSelected ? .green : .secondary)
                VStack(alignment: .leading, spacing: 2) {
                    Text(summary.fileName)
                        .fontWeight(isSelected ? .medium : .regular)
                    Text(caption)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                Spacer()
            }
            .contentShape(.rect)
        }
        .buttonStyle(.plain)
    }

    private var caption: String {
        var parts: [String] = []
        if let modified = summary.modifiedAt {
            parts.append(modified.formatted(date: .abbreviated, time: .shortened))
        }
        if let version = summary.gameVersion {
            parts.append("game \(version)")
        }
        parts.append("\(summary.bookCount) books (\(summary.masteredCount) mastered)")
        parts.append("\(summary.skillStackCount) skill records")
        return parts.joined(separator: " · ")
    }
}