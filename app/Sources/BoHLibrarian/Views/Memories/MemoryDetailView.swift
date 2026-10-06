import SwiftUI
import BoHLibrarianCore

// One memory's full editor: aspects (inline, saved as you change them),
// sources, which books yield it, notes, delete. Aspects/sources edit local state
// first and persist per mutation; reload-on-ID only (same pattern as BookDetailView).

struct MemoryDetailView: View {
    let memory: Memory
    let store: MemoriesStore
    var onEdit: () -> Void

    @Environment(AppState.self) private var appState

    @State private var aspectRows: [AspectDraftRow] = []
    @State private var sources: [MemorySource] = []
    @State private var backlinks: [BookRef] = []
    @State private var notes: String = ""
    @State private var notesDirty = false
    @State private var newSourceKind: MemorySourceKind = .consider
    @State private var newSourceDetail = ""
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
            AspectEditor(principles: appState.principles, rows: $aspectRows) {
                persistAspects()
            }
            if aspectRows.contains(where: { $0.principleID == nil }) {
                Text("Rows without a principle are skipped.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private func persistAspects() {
        let drafts = aspectRows.compactMap { row -> AspectDraft? in
            guard let principleID = row.principleID else { return nil }
            return AspectDraft(principleID: principleID, level: row.level)
        }
        store.setAspects(memory.id, drafts)
    }

    // MARK: Sources

    private var sourcesSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("How to obtain")
            ForEach(sources) { source in
                HStack {
                    Text(source.kind)
                        .font(.callout.weight(.medium))
                    if let detail = source.detail {
                        Text(detail)
                            .foregroundStyle(.secondary)
                    }
                    Spacer()
                    Button {
                        store.removeSource(memoryID: memory.id, source: source)
                        sources = store.sources(for: memory.id)
                    } label: {
                        Image(systemName: "minus.circle")
                            .accessibilityLabel("Remove source")
                    }
                    .buttonStyle(.borderless)
                    .help("Remove source")
                }
            }
            HStack {
                Picker("Kind", selection: $newSourceKind) {
                    ForEach(MemorySourceKind.allCases, id: \.self) { kind in
                        Text(kind.rawValue).tag(kind)
                    }
                }
                .labelsHidden()
                TextField("Detail (e.g. Talk with the Rector (17%))", text: $newSourceDetail)
                    .onSubmit(addSource)
                Button(action: addSource) {
                    Image(systemName: "plus.circle")
                        .accessibilityLabel("Add source")
                }
                .buttonStyle(.borderless)
                .help("Add source")
            }
            if sources.isEmpty {
                Text("No sources recorded — note where this one came from.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private func addSource() {
        let detail = newSourceDetail.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !detail.isEmpty else { return }
        store.addSource(memoryID: memory.id, kind: newSourceKind.rawValue,
                        detail: detail.isEmpty ? nil : detail)
        newSourceDetail = ""
        sources = store.sources(for: memory.id)
    }

    // MARK: Books that yield this

    private var yieldsSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Books that yield this")
            ForEach(backlinks) { book in
                HStack {
                    Image(systemName: "book")
                        .foregroundStyle(.secondary)
                        .accessibilityHidden(true)
                    Text(book.title)
                        .font(.callout)
                    Spacer()
                    Button {
                        store.unlinkBook(book.id)
                        backlinks = store.booksYielding(memory.id)
                    } label: {
                        Image(systemName: "minus.circle")
                            .accessibilityLabel("Unlink this book")
                    }
                    .buttonStyle(.borderless)
                    .help("Unlink — this book doesn't yield this memory")
                }
            }
            Menu {
                ForEach(linkableBooks) { book in
                    Button(book.title) {
                        store.linkBook(book.id, yields: memory.id)
                        backlinks = store.booksYielding(memory.id)
                    }
                }
            } label: {
                Label("Link a book…", systemImage: "link")
            }
            .disabled(linkableBooks.isEmpty)
            if backlinks.isEmpty {
                Text("Every read of the linked books gives this memory — use “Record read…” on a book to create the link automatically.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var linkableBooks: [BookRef] {
        store.booksForLinking.filter { book in
            !backlinks.contains { $0.id == book.id }
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
        aspectRows = memory.aspects.map {
            AspectDraftRow(principleID: $0.principleID, level: $0.level)
        }
        sources = store.sources(for: memory.id)
        backlinks = store.booksYielding(memory.id)
        notes = memory.notes ?? ""
        notesDirty = false
    }
}