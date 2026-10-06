import SwiftUI
import BoHLibrarianCore

// Everything recorded about one book, with inline status change, record-read,
// quick journal notes and notes editing. Data-layer reads happen on appear /
// when the book's relevant fields change (see AuxKey).

struct BookDetailView: View {
    let book: Book
    let store: BooksStore
    var onEdit: () -> Void
    var onMarkRead: () -> Void

    @Environment(AppState.self) private var appState

    init(book: Book, store: BooksStore,
         onEdit: @escaping () -> Void, onMarkRead: @escaping () -> Void) {
        self.book = book
        self.store = store
        self.onEdit = onEdit
        self.onMarkRead = onMarkRead
    }

    @State private var yieldedMemoryName: String?
    @State private var journalEntries: [JournalEntry] = []
    @State private var lessonSkillNames: [String] = []
    @State private var knownLanguage: Bool?
    @State private var notes: String = ""
    @State private var notesDirty = false
    @State private var quickNote = ""
    @State private var confirmingDelete = false

    /// Reload auxiliary data whenever the parts of the book that feed it change.
    private struct AuxKey: Hashable {
        let id: Int64
        let yieldedMemoryID: Int64?
        let lessons: Int?
    }

    private var auxKey: AuxKey {
        AuxKey(id: book.id, yieldedMemoryID: book.yieldedMemoryID, lessons: book.lessons)
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                header
                if let contamination = book.contamination, contamination != .clear {
                    contaminationBanner(contamination)
                }
                factsSection
                readingSection
                yieldSection
                journalSection
                notesSection
                dangerSection
            }
            .padding(20)
            .frame(maxWidth: 760, alignment: .leading)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .task(id: auxKey) { loadAuxiliary() }
    }

    // MARK: Header

    private var header: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(alignment: .firstTextBaseline) {
                Text(book.title)
                    .font(.title2.bold())
                Spacer()
                Button("Edit…") { onEdit() }
                Button("Record read…") { onMarkRead() }
                    .buttonStyle(.borderedProminent)
            }
            Text(subtitle)
                .font(.subheadline)
                .foregroundStyle(.secondary)
        }
    }

    private var subtitle: String {
        var parts: [String] = []
        if let set = book.setName { parts.append(set) }
        if let volume = book.volume { parts.append(volume) }
        parts.append(book.bookKind.rawValue.capitalized)
        if let location = book.location { parts.append("· \(location)") }
        return parts.joined(separator: " · ")
    }

    private func contaminationBanner(_ contamination: Contamination) -> some View {
        Label("\(contamination.displayName) present — read with care, and use the right soul.",
              systemImage: "exclamationmark.triangle.fill")
            .font(.callout)
            .foregroundStyle(.orange)
            .padding(10)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 8).fill(.orange.opacity(0.12)))
    }

    // MARK: Facts

    private var factsSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("The book")
            LabeledContent("Difficulty") {
                if let principleID = book.mysteryPrincipleID, let difficulty = book.difficulty {
                    PrincipleBadge(name: store.principleName(principleID), level: difficulty,
                                   colorHex: store.principleColor(principleID))
                } else if let difficulty = book.difficulty {
                    Text("\(difficulty)")
                        .fontWeight(.medium)
                        .help("Principle not recorded yet")
                } else {
                    Text("not recorded").foregroundStyle(.tertiary)
                }
            }
            LabeledContent("Language") {
                HStack(spacing: 6) {
                    if let name = store.languageName(book.languageID) {
                        Text(name)
                        if let known = knownLanguage {
                            Image(systemName: known ? "checkmark.circle.fill" : "exclamationmark.circle")
                                .foregroundStyle(known ? .green : .orange)
                                .help(known ? "You know this language" : "Not learned yet — a visitor can teach it")
                        }
                    } else {
                        Text("none / not recorded").foregroundStyle(.tertiary)
                    }
                }
            }
            if !lessonSkillNames.isEmpty {
                LabeledContent("Lessons teach") {
                    Text(lessonSkillNames.joined(separator: ", "))
                }
            }
        }
    }

    // MARK: Reading

    private var readingSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Reading")
            Picker("Status", selection: statusBinding) {
                ForEach(ReadStatus.allCases, id: \.self) { status in
                    Text(status.rawValue.capitalized).tag(status)
                }
            }
            .pickerStyle(.segmented)
            .labelsHidden()

            LabeledContent("Times read") {
                Text(book.timesRead == 0 ? "never" : "\(book.timesRead)")
            }
            if let first = book.firstReadAt {
                LabeledContent("First read") { Text(first) }
            }
            if let last = book.lastReadAt {
                LabeledContent("Last read") { Text(last) }
            }
            LabeledContent("Lessons granted") {
                if let lessons = book.lessons {
                    Text("\(lessons)")
                } else {
                    Text("—").foregroundStyle(.tertiary)
                }
            }
        }
    }

    private var statusBinding: Binding<ReadStatus> {
        Binding(get: { book.readStatus },
                set: { store.setReadStatus(book, $0) })
    }

    // MARK: Yield

    private var yieldSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Yields")
            LabeledContent("Memory") {
                if let name = yieldedMemoryName {
                    Text(name).fontWeight(.medium)
                } else {
                    Text("not recorded")
                        .foregroundStyle(.tertiary)
                }
            }
            if yieldedMemoryName == nil {
                Text("Every read of this book gives the same memory — use “Record read…” to note it.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    // MARK: Journal

    private var journalSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Journal")
            TextField("Add a note about this book…", text: $quickNote)
                .onSubmit {
                    let text = quickNote.trimmingCharacters(in: .whitespacesAndNewlines)
                    guard !text.isEmpty else { return }
                    store.addJournalNote(book: book, text: text,
                                          gameDay: gameDayForNote)
                    quickNote = ""
                    loadAuxiliary()
                }
            ForEach(journalEntries) { entry in
                VStack(alignment: .leading, spacing: 2) {
                    Text(entry.entry).font(.callout)
                    Text(entryCaption(entry)).font(.caption).foregroundStyle(.secondary)
                }
                .padding(.vertical, 2)
            }
        }
    }

    private var gameDayForNote: String? {
        let day = appState.currentGameDay.trimmingCharacters(in: .whitespacesAndNewlines)
        return day.isEmpty ? nil : day
    }

    private func entryCaption(_ entry: JournalEntry) -> String {
        [entry.gameDay, entry.loggedAt].compactMap { $0 }.joined(separator: " · ")
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
                    notesDirty = new != (book.notes ?? "")
                }
            if notesDirty {
                HStack {
                    Button("Save note") {
                        store.updateNotes(book, notes: notes)
                        notesDirty = false
                    }
                    Button("Revert", role: .cancel) {
                        notes = book.notes ?? ""
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
            Button("Delete Book…", role: .destructive) { confirmingDelete = true }
        }
        .confirmationDialog("Delete “\(book.title)”?", isPresented: $confirmingDelete,
                            titleVisibility: .visible) {
            Button("Delete Book", role: .destructive) { store.delete(book) }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("Journal entries keep their text but lose the link to this book.")
        }
    }

    // MARK: Helpers

    private func sectionTitle(_ title: String) -> some View {
        Text(title.uppercased())
            .font(.caption.weight(.semibold))
            .foregroundStyle(.secondary)
    }

    private func loadAuxiliary() {
        yieldedMemoryName = book.yieldedMemoryID.flatMap { store.memoryName($0) }
        journalEntries = store.journalEntries(for: book)
        lessonSkillNames = store.lessonSkillNames(for: book)
        knownLanguage = store.isLanguageKnown(book.languageID)
        notes = book.notes ?? ""
        notesDirty = false
    }
}