import SwiftUI
import BoHLibrarianCore

// Phase 1 shell: navigation skeleton with section placeholders and a live status
// footer fed from the real database. Screens land in Phases 2–5 (docs/ROADMAP.md).

struct RootView: View {
    @Environment(AppState.self) private var appState

    var body: some View {
        switch appState.phase {
        case .loading:
            ProgressView("Opening the library…")
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        case .failed(let message):
            FailureView(message: message)
        case .ready:
            mainContent
        }
    }

    private var mainContent: some View {
        // List selection on macOS is Optional-valued; map nil (deselect) to "keep current".
        let selection = Binding<AppSection?>(
            get: { appState.section },
            set: { if let section = $0 { appState.section = section } }
        )
        return NavigationSplitView {
            List(selection: selection) {
                Section {
                    PlaythroughMenu()
                }
                Section {
                    ForEach(AppSection.allCases) { section in
                        Label(section.title, systemImage: section.systemImage)
                            .tag(section)
                    }
                }
            }
            .listStyle(.sidebar)
            .navigationTitle("BoH Librarian")
        } detail: {
            DetailView(section: appState.section, appState: appState)
        }
    }
}

// MARK: - Sections

enum AppSection: Int, CaseIterable, Identifiable {
    case books = 1
    case memories = 2
    case skills = 3
    case journal = 4
    case readingHelper = 5

    var id: Int { rawValue }

    var title: String {
        switch self {
        case .books: "Books"
        case .memories: "Memories"
        case .skills: "Skills"
        case .journal: "Journal"
        case .readingHelper: "Reading Helper"
        }
    }

    var systemImage: String {
        switch self {
        case .books: "books.vertical"
        case .memories: "sparkles"
        case .skills: "graduationcap"
        case .journal: "note.text"
        case .readingHelper: "text.magnifyingglass"
        }
    }

    /// The roadmap phase in which this screen gets real UI.
    var arrivalPhase: Int {
        switch self {
        case .books: 2
        case .memories: 3
        case .readingHelper: 4
        case .skills, .journal: 5
        }
    }
}

// MARK: - Detail + footer

private struct DetailView: View {
    let section: AppSection
    let appState: AppState

    var body: some View {
        VStack(spacing: 0) {
            detail
            Divider()
            statusFooter
        }
    }

    @ViewBuilder private var detail: some View {
        switch section {
        case .books:
            if let store = appState.booksStore {
                BooksScreen(store: store)
            } else {
                FailureView(message: "The books store is unavailable — the database may not have opened.")
            }
        case .memories:
            if let store = appState.memoriesStore {
                MemoriesScreen(store: store)
            } else {
                FailureView(message: "The memories store is unavailable — the database may not have opened.")
            }
        case .readingHelper:
            if let store = appState.helperStore {
                ReadingHelperScreen(store: store)
            } else {
                FailureView(message: "The reading helper store is unavailable — the database may not have opened.")
            }
        case .skills:
            if let store = appState.skillsStore {
                SkillsScreen(store: store)
            } else {
                FailureView(message: "The skills store is unavailable — the database may not have opened.")
            }
        case .journal:
            if let store = appState.journalStore {
                JournalScreen(store: store)
            } else {
                FailureView(message: "The journal store is unavailable — the database may not have opened.")
            }
        default:
            SectionPlaceholder(section: section)
        }
    }

    private var statusFooter: some View {
        // Counts are per active playthrough (stores are scoped; bootstrap counts
        // stand in for sections without stores yet).
        HStack(spacing: 8) {
            Image(systemName: "person.crop.circle")
                .foregroundStyle(.secondary)
            Text(appState.activePlaythrough?.name ?? "—")
                .help("Active playthrough")
            Text("·")
            Text(appState.dbPath)
                .help(appState.dbPath)
            Spacer()
            Text("schema v\(appState.schemaVersion)")
            Text("\(appState.principles.count) principles")
            Text("\(appState.languages.count) languages")
            Text("\(appState.booksStore?.books.count ?? appState.bookCount) books")
            Text("\(appState.memoriesStore?.memories.count ?? appState.memoryCount) memories")
            Text("\(appState.skillsStore?.skills.count ?? appState.skillCount) skills")
            Text("\(appState.journalStore?.entries.count ?? appState.journalCount) journal entries")
        }
        .font(.caption)
        .foregroundStyle(.secondary)
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
    }
}

private struct SectionPlaceholder: View {
    let section: AppSection

    var body: some View {
        ContentUnavailableView {
            Label(section.title, systemImage: section.systemImage)
        } description: {
            Text("This screen arrives in Phase \(section.arrivalPhase) (see docs/ROADMAP.md). "
                 + "The database layer behind it is already built and tested.")
        }
    }
}

private struct FailureView: View {
    let message: String

    var body: some View {
        ContentUnavailableView {
            Label("Could not open the library", systemImage: "exclamationmark.triangle")
        } description: {
            Text(message)
        }
    }
}