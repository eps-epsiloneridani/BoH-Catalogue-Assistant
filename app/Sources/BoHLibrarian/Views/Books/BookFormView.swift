import SwiftUI
import BoHLibrarianCore

// Add/Edit book sheet. Deliberately quick: title (+ mystery) is enough to save.
// Everything else can be filled in later from the same sheet.

struct BookFormView: View {
    enum Mode {
        case add
        case edit(Book)
    }

    let mode: Mode
    let principles: [Principle]
    let languages: [Language]
    let onSave: (BookDraft) -> Void

    @Environment(\.dismiss) private var dismiss

    @State private var title = ""
    @State private var setName = ""
    @State private var volume = ""
    @State private var kind: BookKind = .book
    @State private var languageID: Int64?
    @State private var mysteryPrincipleID: Int64?
    @State private var difficultyKnown = true
    @State private var difficulty = 4
    @State private var readStatus: ReadStatus = .uncatalogued
    @State private var contamination: Contamination?
    @State private var location = ""
    @State private var lessons: Int?
    @State private var notes = ""

    init(mode: Mode, principles: [Principle], languages: [Language],
         onSave: @escaping (BookDraft) -> Void) {
        self.mode = mode
        self.principles = principles
        self.languages = languages
        self.onSave = onSave

        if case .edit(let book) = mode {
            _title = State(initialValue: book.title)
            _setName = State(initialValue: book.setName ?? "")
            _volume = State(initialValue: book.volume ?? "")
            _kind = State(initialValue: book.bookKind)
            _languageID = State(initialValue: book.languageID)
            _mysteryPrincipleID = State(initialValue: book.mysteryPrincipleID)
            _difficultyKnown = State(initialValue: book.difficulty != nil)
            _difficulty = State(initialValue: book.difficulty ?? 4)
            _readStatus = State(initialValue: book.readStatus)
            _contamination = State(initialValue: book.contamination)
            _location = State(initialValue: book.location ?? "")
            _lessons = State(initialValue: book.lessons)
            _notes = State(initialValue: book.notes ?? "")
        }
    }

    private var canSave: Bool {
        !title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }

    var body: some View {
        Form {
            Section("The book") {
                TextField("Title", text: $title)
                Picker("Kind", selection: $kind) {
                    ForEach(BookKind.allCases, id: \.self) { kind in
                        Text(kind.rawValue.capitalized).tag(kind)
                    }
                }
                TextField("Series / set (optional)", text: $setName)
                TextField("Volume / edition (optional)", text: $volume)
                TextField("Location — room, shelf… (optional)", text: $location)
            }

            Section("Reading") {
                Picker("Mystery principle", selection: $mysteryPrincipleID) {
                    Text("—").tag(Int64?.none)
                    ForEach(principles) { principle in
                        Text(principle.name).tag(Int64?.some(principle.id))
                    }
                }
                // Difficulty is recordable even when the principle isn't — it's the
                // number to beat, noted at catalogue time (docs/DATABASE.md).
                Toggle("Difficulty known", isOn: $difficultyKnown)
                if difficultyKnown {
                    Stepper("Difficulty: \(difficulty)", value: $difficulty, in: 1...25)
                }
                Picker("Language", selection: $languageID) {
                    Text("—").tag(Int64?.none)
                    ForEach(languages) { language in
                        Text(language.native ? "\(language.name) (native)" : language.name)
                            .tag(Int64?.some(language.id))
                    }
                }
                Picker("Read status", selection: $readStatus) {
                    ForEach(ReadStatus.allCases, id: \.self) { status in
                        Text(status.rawValue.capitalized).tag(status)
                    }
                }
                .pickerStyle(.segmented)
                Picker("Contamination", selection: $contamination) {
                    Text("Unknown / not checked").tag(Contamination?.none)
                    Text("None").tag(Contamination?.some(.clear))
                    ForEach([Contamination.curse, .theoplasmic, .infestation, .corruption], id: \.self) {
                        Text($0.displayName).tag(Contamination?.some($0))
                    }
                }
                Picker("Lessons granted", selection: $lessons) {
                    Text("—").tag(Int?.none)
                    ForEach([1, 2, 3], id: \.self) { Text("\($0)").tag(Int?.some($0)) }
                }
            }

            Section("Notes") {
                TextEditor(text: $notes)
                    .frame(minHeight: 64)
            }

            Section {
                HStack {
                    Spacer()
                    Button("Cancel", role: .cancel) { dismiss() }
                    Button(saveTitle) { save() }
                        .buttonStyle(.borderedProminent)
                        .disabled(!canSave)
                }
            }
        }
        .formStyle(.grouped)
        .frame(minWidth: 440, minHeight: 480)
    }

    private var saveTitle: String {
        if case .add = mode { "Add Book" } else { "Save" }
    }

    private func save() {
        let trimmedTitle = title.trimmingCharacters(in: .whitespacesAndNewlines)
        let draft = BookDraft(
            title: trimmedTitle,
            setName: optionalText(setName),
            volume: optionalText(volume),
            bookKind: kind,
            languageID: languageID,
            mysteryPrincipleID: mysteryPrincipleID,
            difficulty: difficultyKnown ? difficulty : nil,
            readStatus: readStatus,
            contamination: contamination,
            location: optionalText(location),
            lessons: lessons,
            notes: optionalText(notes)
        )
        onSave(draft)
        dismiss()
    }

    private func optionalText(_ text: String) -> String? {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        return trimmed.isEmpty ? nil : trimmed
    }
}