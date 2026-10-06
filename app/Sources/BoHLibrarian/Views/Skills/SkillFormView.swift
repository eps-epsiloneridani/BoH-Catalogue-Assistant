import SwiftUI
import BoHLibrarianCore

// Add/Edit skill sheet. A skill is recorded when learned: level 1 by default,
// primary/secondary principles fixed from then on.

struct SkillFormView: View {
    enum Mode {
        case add
        case edit(Skill)
    }

    let mode: Mode
    let principles: [Principle]
    let onSave: (SkillDraft) -> Void

    @Environment(\.dismiss) private var dismiss

    @State private var name = ""
    @State private var isLanguage = false
    @State private var primaryPrincipleID: Int64?
    @State private var secondaryPrincipleID: Int64?
    @State private var levelKnown = true
    @State private var level = 1
    @State private var wisdom = ""
    @State private var element = ""
    @State private var notes = ""

    init(mode: Mode, principles: [Principle], onSave: @escaping (SkillDraft) -> Void) {
        self.mode = mode
        self.principles = principles
        self.onSave = onSave

        if case .edit(let skill) = mode {
            _name = State(initialValue: skill.name)
            _isLanguage = State(initialValue: skill.isLanguage)
            _primaryPrincipleID = State(initialValue: skill.primaryPrincipleID)
            _secondaryPrincipleID = State(initialValue: skill.secondaryPrincipleID)
            _levelKnown = State(initialValue: skill.level != nil)
            _level = State(initialValue: skill.level ?? 1)
            _wisdom = State(initialValue: skill.wisdom ?? "")
            _element = State(initialValue: skill.element ?? "")
            _notes = State(initialValue: skill.notes ?? "")
        }
    }

    private var canSave: Bool {
        !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }

    var body: some View {
        Form {
            Section("The skill") {
                TextField("Name", text: $name)
                Toggle("Language skill", isOn: $isLanguage)
                    .help("Exotic languages are skills: visitor-taught, slotted when reading, committed to the Tree like any other")
            }

            Section("Principles") {
                Picker("Primary (contributes level+1)", selection: $primaryPrincipleID) {
                    Text("—").tag(Int64?.none)
                    ForEach(principles) { principle in
                        Text(principle.name).tag(Int64?.some(principle.id))
                    }
                }
                Picker("Secondary (contributes level)", selection: $secondaryPrincipleID) {
                    Text("—").tag(Int64?.none)
                    ForEach(principles) { principle in
                        Text(principle.name).tag(Int64?.some(principle.id))
                    }
                }
            }

            Section("Progress") {
                Toggle("Level known", isOn: $levelKnown)
                if levelKnown {
                    Stepper("Level: \(level)", value: $level, in: 1...9)
                }
                TextField("Wisdom (once committed to the Tree)", text: $wisdom)
                TextField("Element of the Soul gained", text: $element)
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
        .frame(minWidth: 440, minHeight: 520)
    }

    private var saveTitle: String {
        if case .add = mode { "Add Skill" } else { "Save" }
    }

    private func save() {
        let trimmedName = name.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedWisdom = wisdom.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedElement = element.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedNotes = notes.trimmingCharacters(in: .whitespacesAndNewlines)
        onSave(SkillDraft(
            name: trimmedName,
            isLanguage: isLanguage,
            primaryPrincipleID: primaryPrincipleID,
            secondaryPrincipleID: secondaryPrincipleID,
            level: levelKnown ? level : nil,
            wisdom: trimmedWisdom.isEmpty ? nil : trimmedWisdom,
            element: trimmedElement.isEmpty ? nil : trimmedElement,
            notes: trimmedNotes.isEmpty ? nil : trimmedNotes
        ))
        dismiss()
    }
}