import SwiftUI
import BoHLibrarianCore

// One memory's full record: read-only aspect display (editing lives in the
// Edit… sheet's AspectEditor — the detail pane is not an editor), sources,
// which books yield it, notes, delete. Reload-on-ID only (same pattern as
// BookDetailView).

struct MemoryDetailView: View {
    let memory: Memory
    let store: MemoriesStore
    var onEdit: () -> Void

    @State private var notes: String = ""
    @State private var notesDirty = false
    @State private var confirmingDelete = false

    init(memory: Memory, store: MemoriesStore, onEdit: @escaping () -> Void) {
        self.memory = memory
        self.store = store
        self.onEdit = onEdit
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                header
                aspectsSection
                sourcesSection
                yieldsSection
                notesSection
                dangerSection
            }
            .padding(20)
            .frame(maxWidth: 760, alignment: .leading)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .task(id: memory.id) { load() }
        // Same guarded re-seed: sheet saves refresh the pane's notes while they
        // are not mid-edit; mid-edit typing is preserved, never clobbered.
        .onChange(of: memory.notes) { _, fresh in
            if !notesDirty { notes = fresh ?? "" }
        }
    }

    // MARK: Header

    private var header: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(alignment: .firstTextBaseline) {
                Text(memory.name)
                    .font(.title2.bold())
                Spacer()
                Button("Edit…") { onEdit() }
            }
            HStack(spacing: 8) {
                Text(memory.kind.rawValue.capitalized)
                if memory.persistent {
                    Label("persistent", systemImage: "infinity")
                        .foregroundStyle(.orange)
                        .help("Survives dawn — wiped at the first dawn after Numa")
                }
            }
            .font(.subheadline)
            .foregroundStyle(.secondary)
        }
    }

    // MARK: Aspects

    private var aspectsSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Aspects")
            if memory.aspects.isEmpty {
                Text("No aspects recorded — use Edit… to add them.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } else {
                VStack(alignment: .leading, spacing: 6) {
                    ForEach(memory.aspects) { aspect in
                        PrincipleBadge(name: store.principleName(aspect.principleID),
                                       level: aspect.level,
                                       colorHex: store.principleColor(aspect.principleID))
                    }
                }
            }
        }
    }

    // MARK: Sources

    private var sourcesSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("How to obtain")
            if store.sources(for: memory.id).isEmpty {
                Text("No sources recorded — add them with Edit….")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } else {
                VStack(alignment: .leading, spacing: 6) {
                    ForEach(store.sources(for: memory.id)) { source in
                        HStack(spacing: 6) {
                            Text(source.kind)
                                .font(.callout.weight(.medium))
                            if let detail = source.detail {
                                Text(detail)
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                }
            }
        }
    }

    // MARK: Books that yield this

    private var yieldsSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Books that yield this")
            if store.yielding(for: memory.id).isEmpty {
                Text("No linked book yields this yet — link one with Edit…, or use “Record read…” on a book.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } else {
                VStack(alignment: .leading, spacing: 6) {
                    ForEach(store.yielding(for: memory.id)) { book in
                        HStack(spacing: 6) {
                            Image(systemName: "book")
                                .foregroundStyle(.secondary)
                                .accessibilityHidden(true)
                            Text(book.title)
                                .font(.callout)
                        }
                    }
                }
            }
        }
    }

    // MARK: Notes

    private var notesSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Notes")
            TextEditor(text: $notes)
                .frame(minHeight: 64)
                .scrollContentBackground(.hidden)
                .padding(8)
                .background(RoundedRectangle(cornerRadius: 8).fill(.quaternary.opacity(0.4)))
                .onChange(of: notes) { _, new in
                    notesDirty = new != (memory.notes ?? "")
                }
            if notesDirty {
                HStack {
                    Button("Save note") {
                        store.updateNotes(memory, notes: notes)
                        notesDirty = false
                    }
                    Button("Revert", role: .cancel) {
                        notes = memory.notes ?? ""
                        notesDirty = false
                    }
                    Spacer()
                }
            }
        }
    }

    // MARK: Danger

    private var dangerSection: some View {
        HStack {
            Spacer()
            Button("Delete Memory…", role: .destructive) { confirmingDelete = true }
        }
        .confirmationDialog("Delete “\(memory.name)”?", isPresented: $confirmingDelete,
                            titleVisibility: .visible) {
            Button("Delete Memory", role: .destructive) { store.delete(memory) }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("Books that yield it lose their link; journal entries keep their text.")
        }
    }

    // MARK: Helpers

    private func sectionTitle(_ title: String) -> some View {
        Text(title.uppercased())
            .font(.caption.weight(.semibold))
            .foregroundStyle(.secondary)
    }

    private func load() {
        // Notes are the pane's only editable state; sources/backlinks read live
        // from the store so edits made in the sheet show the moment it closes.
        notes = memory.notes ?? ""
        notesDirty = false
    }
}