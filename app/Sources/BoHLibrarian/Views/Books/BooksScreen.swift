import SwiftUI
import BoHLibrarianCore

// The Books screen: master list on the left (search, filter, sort, add),
// selected book's everything on the right. Per docs/GUI_PLAN.md §Books.

struct BooksScreen: View {
    let store: BooksStore

    @Environment(AppState.self) private var appState

    @State private var addingBook = false
    @State private var editingBook: Book?
    @State private var markingRead: Book?

    var body: some View {
        @Bindable var store = store
        return Group {
            if store.books.isEmpty {
                emptyState
            } else {
                content
            }
        }
        .frame(minWidth: 640)
        .searchable(text: $store.options.searchText, placement: .toolbar,
                    prompt: "Search title, set, notes…")
        .toolbar {
            ToolbarItemGroup(placement: .primaryAction) {
                Button {
                    addingBook = true
                } label: {
                    Label("Add Book", systemImage: "plus")
                }
                .keyboardShortcut("n")
                .help("Record a newly found book (⌘N)")

                filterMenu
                sortMenu
            }
        }
        .alert("Something went wrong", isPresented: errorBinding) {
            Button("OK") { store.lastError = nil }
        } message: {
            Text(store.lastError ?? "")
        }
        .sheet(isPresented: $addingBook) {
            BookFormView(mode: .add, principles: appState.principles,
                         languages: appState.languages) { draft in
                store.add(draft)
            }
        }
        .sheet(item: $editingBook) { book in
            BookFormView(mode: .edit(book), principles: appState.principles,
                         languages: appState.languages) { draft in
                store.update(book, with: draft)
            }
        }
        .sheet(item: $markingRead) { book in
            MarkAsReadSheet(book: book, store: store)
        }
    }

    private var errorBinding: Binding<Bool> {
        Binding(get: { store.lastError != nil },
                set: { if !$0 { store.lastError = nil } })
    }

    // MARK: Layout

    private var content: some View {
        HStack(spacing: 0) {
            bookList
                .frame(minWidth: 260, idealWidth: 320, maxWidth: 480)
            Divider()
            if let book = store.selectedBook {
                BookDetailView(book: book, store: store,
                               onEdit: { editingBook = book },
                               onMarkRead: { markingRead = book })
            } else {
                ContentUnavailableView("Select a book", systemImage: "book",
                                       description: Text("Pick a book from the list to see and edit everything recorded about it."))
            }
        }
    }

    private var bookList: some View {
        let selection = Binding<Int64?>(
            get: { store.selectedBookID },
            set: { store.selectedBookID = $0 }
        )
        return List(selection: selection) {
            ForEach(store.displayed) { book in
                BookRow(book: book, store: store)
                    .tag(book.id)
            }
        }
        .listStyle(.inset)
        .onAppear { store.reload() }
    }

    private var emptyState: some View {
        ContentUnavailableView {
            Label("No books recorded yet", systemImage: "books.vertical")
        } description: {
            Text("Catalogue the first book you find in Hush House — a title and its mystery is enough to start.")
        } actions: {
            Button("Add Book") { addingBook = true }
                .keyboardShortcut(.defaultAction)
        }
    }

    // MARK: Toolbar menus

    private var filterMenu: some View {
        @Bindable var store = store
        return Menu {
            Picker("Show", selection: $store.options.statusFilter) {
                ForEach(BookStatusFilter.allCases) { filter in
                    Text(filter.rawValue).tag(filter)
                }
            }
        } label: {
            Label("Filter", systemImage: "line.3.horizontal.decrease.circle")
        }
        .help("Filter by read status or contamination")
    }

    private var sortMenu: some View {
        @Bindable var store = store
        return Menu {
            Picker("Sort by", selection: $store.options.sort) {
                ForEach(BookSort.allCases) { sort in
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

private struct BookRow: View {
    let book: Book
    let store: BooksStore

    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(book.title)
                    .font(.body)
                    .lineLimit(1)
                HStack(spacing: 4) {
                    if book.bookKind != .book {
                        Text(book.bookKind.rawValue.capitalized)
                    }
                    if let set = book.setName {
                        Text(set)
                    }
                    if let volume = book.volume {
                        Text(volume)
                    }
                    if let location = book.location {
                        Text(location)
                    }
                }
                .font(.caption)
                .foregroundStyle(.secondary)
                .lineLimit(1)
            }
            Spacer(minLength: 8)
            VStack(alignment: .trailing, spacing: 3) {
                if let principleID = book.mysteryPrincipleID, let level = book.mysteryLevel {
                    PrincipleBadge(name: store.principleName(principleID),
                                   level: level,
                                   colorHex: store.principleColor(principleID))
                }
                HStack(spacing: 4) {
                    if let language = store.languageName(book.languageID) {
                        Text(language)
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                    }
                    statusIcon
                }
            }
        }
        .padding(.vertical, 2)
    }

    private var statusIcon: some View {
        Group {
            switch book.readStatus {
            case .uncatalogued:
                Image(systemName: "questionmark.circle")
            case .catalogued:
                Image(systemName: "book")
            case .mastered:
                Image(systemName: "checkmark.circle.fill")
                    .foregroundStyle(.green)
            }
        }
        .help(book.readStatus.rawValue.capitalized)
        .foregroundStyle(.secondary)
    }
}