import SwiftUI
import BoHLibrarianCore

// The Memories screen: list of memory cards (search, principle/level filter, sort)
// with the selected memory's full editor on the right. Per docs/GUI_PLAN.md §Memories.

struct MemoriesScreen: View {
    let store: MemoriesStore

    @Environment(AppState.self) private var appState

    @State private var addingMemory = false
    @State private var editingMemory: Memory?

    init(store: MemoriesStore) {
        self.store = store
    }

    var body: some View {
        @Bindable var store = store
        return Group {
            if store.memories.isEmpty {
                emptyState
            } else {
                content
            }
        }
        .frame(minWidth: 640)
        .searchable(text: $store.options.searchText, placement: .toolbar,
                    prompt: "Search name, aspects, notes…")
        .toolbar {
            ToolbarItemGroup(placement: .primaryAction) {
                Button {
                    addingMemory = true
                } label: {
                    Label("Add Memory", systemImage: "plus")
                }
                .keyboardShortcut("n")
                .help("Record a newly obtained memory (⌘N)")

                principleMenu
                levelMenu
                sortMenu
            }
        }
        .alert("Something went wrong", isPresented: errorBinding) {
            Button("OK") { store.lastError = nil }
        } message: {
            Text(store.lastError ?? "")
        }
        .sheet(isPresented: $addingMemory) {
            MemoryFormView(mode: .add, principles: appState.principles) { draft, _, _ in
                store.add(draft)
            }
        }
        .sheet(item: $editingMemory) { memory in
            MemoryFormView(mode: .edit(memory), principles: appState.principles,
                           books: store.booksForLinking,
                           sources: store.sources(for: memory.id),
                           linked: store.allYielding(memory.id)) { draft, sources, links in
                store.update(memory, with: draft)
                store.setSources(memory.id, sources)
                store.setYieldingBooks(memory.id, links)
            }
        }
    }

    private var errorBinding: Binding<Bool> {
        Binding(get: { store.lastError != nil },
                set: { if !$0 { store.lastError = nil } })
    }

    // MARK: Layout

    private var content: some View {
        HStack(spacing: 0) {
            memoryList
                .frame(minWidth: 260, idealWidth: 320, maxWidth: 480)
            Divider()
            if let memory = store.selectedMemory {
                MemoryDetailView(memory: memory, store: store,
                                 onEdit: { editingMemory = memory })
            } else {
                ContentUnavailableView("Select a memory", systemImage: "sparkles",
                                       description: Text("Pick a memory to see its aspects, sources and the books that yield it."))
            }
        }
    }

    private var memoryList: some View {
        let selection = Binding<Int64?>(
            get: { store.selectedMemoryID },
            set: { store.selectedMemoryID = $0 }
        )
        return List(selection: selection) {
            ForEach(store.displayed) { memory in
                MemoryRow(memory: memory, store: store)
                    .tag(memory.id)
            }
        }
        .listStyle(.inset)
        .onAppear { store.reload() }
    }

    private var emptyState: some View {
        ContentUnavailableView {
            Label("No memories recorded yet", systemImage: "sparkles")
        } description: {
            Text("Record a memory once you've gained it — the record-read flow on a book adds one automatically.")
        } actions: {
            Button("Add Memory") { addingMemory = true }
                .keyboardShortcut(.defaultAction)
        }
    }

    // MARK: Toolbar menus

    private var principleMenu: some View {
        @Bindable var store = store
        return Menu {
            Picker("Principle", selection: $store.options.principleID) {
                Text("All").tag(Int64?.none)
                ForEach(appState.principles) { principle in
                    Text(principle.name).tag(Int64?.some(principle.id))
                }
            }
        } label: {
            Label("Principle", systemImage: "circle.hexagongrid")
        }
        .help("Show only memories with an aspect in one principle")
    }

    private var levelMenu: some View {
        @Bindable var store = store
        return Menu {
            Picker("Level", selection: $store.options.minLevel) {
                Text("Any").tag(Int?.none)
                ForEach([2, 3, 4, 5, 6], id: \.self) { level in
                    Text("≥ \(level)").tag(Int?.some(level))
                }
            }
        } label: {
            Label("Level", systemImage: "chart.bar.fill")
        }
        .help("Minimum aspect level (in the chosen principle, or the memory's highest)")
    }

    private var sortMenu: some View {
        @Bindable var store = store
        return Menu {
            Picker("Sort by", selection: $store.options.sort) {
                ForEach(MemorySort.allCases) { sort in
                    Text(sort.rawValue).tag(sort)
                }
            }
        } label: {
            Label("Sort", systemImage: "arrow.up.arrow.down")
        }
        .help("Change the list order")
    }
}

// MARK: - Row

private struct MemoryRow: View {
    let memory: Memory
    let store: MemoriesStore

    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(memory.name)
                    .font(.body)
                    .lineLimit(1)
                HStack(spacing: 4) {
                    if memory.kind != .memory {
                        kindIcon
                    }
                    if let notes = memory.notes {
                        Text(notes)
                            .lineLimit(1)
                    }
                }
                .font(.caption)
                .foregroundStyle(.secondary)
            }
            Spacer(minLength: 8)
            HStack(spacing: 4) {
                ForEach(memory.aspects) { aspect in
                    PrincipleBadge(name: store.principleName(aspect.principleID),
                                   level: aspect.level,
                                   colorHex: store.principleColor(aspect.principleID))
                }
                if memory.persistent {
                    Image(systemName: "infinity")
                        .font(.caption2)
                        .foregroundStyle(.orange)
                        .accessibilityLabel("Persistent — survives dawn")
                        .help("Persistent — survives dawn (not Numa)")
                }
            }
        }
        .padding(.vertical, 2)
    }

    private var kindIcon: some View {
        Group {
            switch memory.kind {
            case .numen:
                Image(systemName: "star.circle.fill").foregroundStyle(.purple)
                    .accessibilityLabel("Numen memory")
                    .help("Numen")
            case .weather:
                Image(systemName: "cloud.fill")
                    .accessibilityLabel("Weather memory")
                    .help("Weather")
            case .memory:
                EmptyView()
            }
        }
        .font(.caption)
    }
}