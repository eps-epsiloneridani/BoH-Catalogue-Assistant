import SwiftUI
import BoHLibrarianCore

// The Reading Helper: pick the book in your hand, see exactly what it demands,
// see which recorded memories and skills can meet it, and log the read in one
// flow (shared record-read sheet). Per docs/GUI_PLAN.md §Reading Helper.

struct ReadingHelperScreen: View {
    let store: ReadingHelperStore

    @Environment(AppState.self) private var appState

    @State private var recordingBook: Book?
    @State private var prefilledMemoryID: Int64?

    init(store: ReadingHelperStore) {
        self.store = store
    }

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
        .searchable(text: $store.searchText, placement: .toolbar,
                    prompt: "Search for the book in hand…")
        .toolbar {
            ToolbarItemGroup(placement: .primaryAction) {
                statusMenu
                mysteryMenu
                sortMenu
            }
        }
        .sheet(item: $recordingBook, onDismiss: {
            store.reload()
            prefilledMemoryID = nil
        }) { book in
            if let booksStore = appState.booksStore {
                MarkAsReadSheet(book: book, store: booksStore,
                                preselectedMemoryID: prefilledMemoryID)
            }
        }
    }

    // MARK: Filter + sort menus (Books-screen vocabulary)

    private var statusMenu: some View {
        @Bindable var store = store
        return Menu {
            Picker("Show", selection: $store.statusFilter) {
                ForEach(BookStatusFilter.allCases) { filter in
                    Text(filter.rawValue).tag(filter)
                }
            }
        } label: {
            Label("Filter", systemImage: "line.3.horizontal.decrease.circle")
        }
        .help("Filter by read status — mastered, catalogued, unread…")
    }

    private var mysteryMenu: some View {
        @Bindable var store = store
        return Menu {
            Picker("Mystery principle", selection: $store.mysteryPrincipleID) {
                Text("Any mystery").tag(Int64?.none)
                ForEach(appState.principles) { principle in
                    Text(principle.name).tag(Int64?.some(principle.id))
                }
            }
        } label: {
            Label("Mystery", systemImage: "sparkles")
        }
        .help("Show only the books whose mystery is this principle")
    }

    private var sortMenu: some View {
        @Bindable var store = store
        return Menu {
            Picker("Sort by", selection: $store.sort) {
                ForEach(BookSort.allCases) { sort in
                    Text(sort.rawValue).tag(sort)
                }
            }
        } label: {
            Label("Sort", systemImage: "arrow.up.arrow.down")
        }
        .help("Change the list order")
    }

    // MARK: Layout

    private var content: some View {
        HStack(spacing: 0) {
            pickerList
                .frame(minWidth: 240, idealWidth: 300, maxWidth: 420)
            Divider()
            if let book = store.selectedBook {
                DeskPanel(book: book, store: store) { prefill in
                    prefilledMemoryID = prefill
                    recordingBook = book
                }
            } else {
                ContentUnavailableView("Select a book", systemImage: "text.magnifyingglass",
                                       description: Text("Pick the book you're about to read — the helper works out what it demands and what you've recorded that could meet it."))
            }
        }
    }

    private var pickerList: some View {
        let selection = Binding<Int64?>(
            get: { store.selectedBookID },
            set: { store.selectedBookID = $0 }
        )
        return List(selection: selection) {
            ForEach(store.displayed) { book in
                HelperBookRow(book: book, store: store)
                    .tag(book.id)
            }
        }
        .listStyle(.inset)
        .onAppear {
            store.reload()
            store.ensureSelection()
        }
    }

    private var emptyState: some View {
        ContentUnavailableView {
            Label("No books recorded yet", systemImage: "text.magnifyingglass")
        } description: {
            Text("Add books on the Books screen as you catalogue them — then come here to plan each read.")
        }
    }
}

// MARK: - Book picker row

private struct HelperBookRow: View {
    let book: Book
    let store: ReadingHelperStore

    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(book.title)
                    .font(.body)
                    .lineLimit(1)
                HStack(spacing: 4) {
                    Text(book.readStatus.rawValue.capitalized)
                    if let language = store.languageName(book.languageID) {
                        Text("· \(language)")
                    }
                    if book.contamination != nil && book.contamination != .clear {
                        Image(systemName: "exclamationmark.triangle.fill")
                            .foregroundStyle(.orange)
                            .accessibilityLabel("contaminated")
                            .help("Contaminated")
                    }
                }
                .font(.caption)
                .foregroundStyle(.secondary)
            }
            Spacer(minLength: 8)
            if let principleID = book.mysteryPrincipleID, let difficulty = book.difficulty {
                PrincipleBadge(name: store.principleName(principleID),
                               level: difficulty,
                               colorHex: store.principleColor(principleID))
            } else if let difficulty = book.difficulty {
                Text("\(difficulty)")
                    .font(.caption.weight(.bold))
                    .foregroundStyle(.secondary)
                    .help("Difficulty recorded; principle not")
            } else {
                Image(systemName: book.readStatus == .mastered
                      ? "checkmark.circle.fill" : "questionmark.circle")
                    .foregroundStyle(book.readStatus == .mastered ? .green : .secondary)
                    .accessibilityLabel(book.readStatus.rawValue)
            }
        }
        .padding(.vertical, 2)
    }
}

// MARK: - Desk panel

private struct DeskPanel: View {
    let book: Book
    let store: ReadingHelperStore
    var onRecord: (Int64?) -> Void   // argument: memory id to prefill, or nil

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                header
                if book.readStatus == .mastered {
                    masteredPanel
                } else {
                    requirementPanel
                    candidatesSection
                    skillsSection
                }
            }
            .padding(20)
            .frame(maxWidth: 760, alignment: .leading)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    // MARK: Header

    private var header: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(alignment: .firstTextBaseline) {
                Text(book.title)
                    .font(.title2.bold())
                Spacer()
                Button {
                    onRecord(nil)
                } label: {
                    Label("Record read…", systemImage: "square.and.pencil")
                }
                .buttonStyle(.borderedProminent)
                .help(book.readStatus == .mastered
                      ? "Log a re-read (60s, any soul) and its memory"
                      : "Log the read, its lessons and the memory gained")
            }
            Text(headerCaption)
                .font(.subheadline)
                .foregroundStyle(.secondary)
        }
    }

    private var headerCaption: String {
        var parts: [String] = [book.readStatus.rawValue.capitalized,
                               book.bookKind.rawValue.capitalized]
        if let set = book.setName { parts.append(set) }
        if let location = book.location { parts.append(location) }
        return parts.joined(separator: " · ")
    }

    // MARK: Mastered books: the memory grinder

    private var masteredPanel: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Re-reading")
            Text("Mastered — re-reads take 60 seconds with any soul, no mystery check"
                 + (book.languageID != nil ? " beyond the language" : "") + ".")
                .font(.callout)
            LabeledContent("Always yields") {
                if let name = store.yieldedMemoryName(for: book) {
                    Text(name).fontWeight(.medium)
                } else {
                    Text("not recorded")
                        .foregroundStyle(.tertiary)
                }
            }
            if store.yieldedMemoryName(for: book) == nil {
                Text("Every read gives this book's memory — use “Record read…” to note which one.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    // MARK: Requirement

    private var requirementPanel: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("The requirement")
            HStack(alignment: .firstTextBaseline, spacing: 10) {
                if let principleID = book.mysteryPrincipleID, let difficulty = book.difficulty {
                    PrincipleBadge(name: store.principleName(principleID),
                                   level: difficulty,
                                   colorHex: store.principleColor(principleID))
                }
                Text(ReadingMath.requirementLine(
                    principleName: store.principleName(book.mysteryPrincipleID),
                    difficulty: book.difficulty))
                    .font(.body)
            }
            if let reach = reachLine {
                Text(reach)
                    .font(.callout)
                    .foregroundStyle(.secondary)
            }

            if let languageID = book.languageID {
                LabeledContent("Language") {
                    HStack(spacing: 6) {
                        Text(store.languageName(languageID) ?? "?")
                        if let known = store.isLanguageKnown(languageID) {
                            Image(systemName: known ? "checkmark.circle.fill" : "exclamationmark.circle")
                                .foregroundStyle(known ? .green : .orange)
                                .accessibilityLabel(known ? "known" : "not learned yet")
                                .help(known ? "You know this language"
                                            : "Not learned yet — a visitor can teach it, for an Iron Spintria")
                        }
                    }
                }
            }
            if let contamination = book.contamination, contamination != .clear {
                Label("\(contamination.displayName) present — read with care, and mind which soul you spend.",
                      systemImage: "exclamationmark.triangle.fill")
                    .font(.callout)
                    .foregroundStyle(.orange)
            }
            if book.bookKind == .film || book.bookKind == .record {
                Label(book.bookKind == .film
                      ? "A reel of film — needs the projector, not a reading desk."
                      : "A phonograph record — needs a phonograph, not a reading desk.",
                      systemImage: "waveform")
                    .font(.callout)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var reachLine: String? {
        let candidates = store.memoryCandidates(for: book)
        let contributions = store.skillContributions(for: book)
        let bestMemory = candidates.satisfying.first?.level ?? candidates.nearMisses.first?.level
        return ReadingMath.reachLine(difficulty: book.difficulty,
                                     memory: bestMemory,
                                     skill: contributions.first?.contributes)
    }

    // MARK: Memory candidates

    private var candidatesSection: some View {
        let candidates = store.memoryCandidates(for: book)
        return VStack(alignment: .leading, spacing: 10) {
            sectionTitle(candidates.satisfying.isEmpty
                         ? "Closest recorded memories"
                         : "Memories that satisfy it")
            if book.mysteryPrincipleID == nil || book.difficulty == nil {
                Text("Candidates appear once the book's principle and difficulty are recorded.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } else if candidates.satisfying.isEmpty && candidates.nearMisses.isEmpty {
                Text("No recorded memory carries this principle yet — record one when you gain it, or lean on skills, weather and inks.")
                    .font(.callout)
                    .foregroundStyle(.secondary)
            } else {
                ForEach(candidates.satisfying) { candidate in
                    CandidateRow(candidate: candidate, book: book, store: store,
                                 caption: "satisfies", prominent: true) {
                        onRecord(candidate.id)
                    }
                }
                ForEach(candidates.nearMisses) { candidate in
                    CandidateRow(candidate: candidate, book: book, store: store,
                                 caption: "not enough alone", prominent: false) {
                        onRecord(candidate.id)
                    }
                }
                Text("Tap a memory to record the read with it. Weather counts if you've recorded today's.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    // MARK: Skills

    private var skillsSection: some View {
        let contributions = store.skillContributions(for: book)
        return VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Skills that help")
            if contributions.isEmpty {
                Text("No recorded skill carries this principle yet. A level-L skill contributes L+1 to its primary principle, L to its secondary.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } else {
                ForEach(contributions) { contribution in
                    SkillRow(contribution: contribution, book: book, store: store)
                }
            }
        }
    }

    private func sectionTitle(_ title: String) -> some View {
        Text(title.uppercased())
            .font(.caption.weight(.semibold))
            .foregroundStyle(.secondary)
    }
}

// MARK: - Candidate row

private struct CandidateRow: View {
    let candidate: MemoryCandidate
    let book: Book
    let store: ReadingHelperStore
    let caption: String
    let prominent: Bool
    var onUse: () -> Void

    var body: some View {
        Button(action: onUse) {
            HStack {
                if let principleID = book.mysteryPrincipleID {
                    PrincipleBadge(name: store.principleName(principleID),
                                   level: candidate.level,
                                   colorHex: store.principleColor(principleID))
                }
                Text(candidate.name)
                    .fontWeight(prominent ? .medium : .regular)
                kindIcons
                Spacer()
                Text(caption)
                    .font(.caption)
                    .foregroundStyle(prominent ? Color.green : Color.secondary)
            }
            .contentShape(.rect)
        }
        .buttonStyle(.plain)
        .help("Record this read using \(candidate.name)")
        .padding(.vertical, 1)
    }

    private var kindIcons: some View {
        HStack(spacing: 4) {
            switch candidate.kind {
            case .weather:
                Image(systemName: "cloud.fill")
                    .font(.caption2)
                    .accessibilityLabel("Weather memory")
                    .help("Weather")
            case .numen:
                Image(systemName: "star.circle.fill")
                    .font(.caption2)
                    .foregroundStyle(.purple)
                    .accessibilityLabel("Numen memory")
                    .help("Numen")
            case .memory:
                EmptyView()
            }
            if candidate.persistent {
                Image(systemName: "infinity")
                    .font(.caption2)
                    .foregroundStyle(.orange)
                    .accessibilityLabel("Persistent — survives dawn")
                    .help("Persistent — survives dawn (not Numa)")
            }
        }
        .foregroundStyle(.secondary)
    }
}

// MARK: - Skill row

private struct SkillRow: View {
    let contribution: SkillContribution
    let book: Book
    let store: ReadingHelperStore

    var body: some View {
        HStack {
            Text(contribution.skill.name)
            Text(contribution.skill.role(book: book))
                .font(.caption)
                .foregroundStyle(.secondary)
            Spacer()
            Text("+\(contribution.contributes)")
                .font(.callout.weight(.bold))
                .monospacedDigit()
            Text("level \(contribution.skill.level ?? 0)")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
        .padding(.vertical, 1)
    }
}

private extension Skill {
    func role(book: Book) -> String {
        guard let principleID = book.mysteryPrincipleID else { return "" }
        return primaryPrincipleID == principleID ? "primary" : "secondary"
    }
}