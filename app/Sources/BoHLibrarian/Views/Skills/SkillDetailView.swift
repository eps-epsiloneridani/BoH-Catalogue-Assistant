import SwiftUI
import BoHLibrarianCore

// One skill's editor: level stepper (persisted immediately), wisdom/element
// (Tree of Wisdoms commitment), notes. Local-first editing like BookDetailView.

struct SkillDetailView: View {
    let skill: Skill
    let store: SkillsStore
    var onEdit: () -> Void

    @State private var wisdom: String = ""
    @State private var element: String = ""
    @State private var commitmentsDirty = false
    @State private var notes: String = ""
    @State private var notesDirty = false
    @State private var confirmingDelete = false

    init(skill: Skill, store: SkillsStore, onEdit: @escaping () -> Void) {
        self.skill = skill
        self.store = store
        self.onEdit = onEdit
    }

    private struct AuxKey: Hashable {
        let id: Int64
        let wisdom: String?
        let element: String?
        let notes: String?
    }

    private var auxKey: AuxKey {
        AuxKey(id: skill.id, wisdom: skill.wisdom, element: skill.element, notes: skill.notes)
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                header
                aspectsSection
                levelSection
                wisdomSection
                notesSection
                dangerSection
            }
            .padding(20)
            .frame(maxWidth: 760, alignment: .leading)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .task(id: auxKey) { load() }
    }

    // MARK: Header

    private var header: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(alignment: .firstTextBaseline) {
                Text(skill.name)
                    .font(.title2.bold())
                Spacer()
                Button("Edit…") { onEdit() }
            }
            HStack(spacing: 8) {
                if skill.isLanguage {
                    Label("Language", systemImage: "globe")
                        .foregroundStyle(.blue)
                        .help("Slots into reading exotic books and the Tree of Wisdoms")
                }
                if let wisdom = skill.wisdom {
                    Label(wisdom, systemImage: "tree")
                }
                if let element = skill.element {
                    Label(element, systemImage: "person.crop.circle")
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
            HStack(spacing: 8) {
                if let primary = skill.primaryPrincipleID {
                    PrincipleBadge(name: store.principleName(primary),
                                   level: contributions?.primary ?? 0,
                                   colorHex: store.principleColor(primary))
                }
                if let secondary = skill.secondaryPrincipleID {
                    PrincipleBadge(name: store.principleName(secondary),
                                   level: contributions?.secondary ?? 0,
                                   colorHex: store.principleColor(secondary))
                }
                Spacer()
            }
            Text("A level-L skill contributes L+1 to its primary principle and L to its secondary. Level 9 maxes at 10 and 9.")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    private var contributions: (primary: Int, secondary: Int)? {
        skill.level.map { SkillMath.contributions(level: $0) }
    }

    // MARK: Level

    private var levelSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Level")
            HStack {
                if let level = skill.level {
                    Stepper("Level \(level)", value: levelBinding, in: 1...9)
                } else {
                    Button("Learned — set level 1") { store.setLevel(skill, to: 1) }
                        .help("A skill recorded without a level")
                }
            }
        }
    }

    /// Stepper binding that persists each change immediately.
    private var levelBinding: Binding<Int> {
        Binding(
            get: { skill.level ?? 1 },
            set: { store.setLevel(skill, to: $0) }
        )
    }

    // MARK: Tree of Wisdoms commitment

    private var wisdomSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Tree of Wisdoms commitment")
            TextField("Wisdom (e.g. Birdsong)", text: $wisdom)
                .onSubmit { saveCommitments() }
            TextField("Element of the Soul gained (e.g. Trist)", text: $element)
                .onSubmit { saveCommitments() }
            if commitmentsDirty {
                HStack {
                    Button("Save") { saveCommitments() }
                    Button("Revert", role: .cancel) {
                        wisdom = skill.wisdom ?? ""
                        element = skill.element ?? ""
                        commitmentsDirty = false
                    }
                    Spacer()
                }
            }
        }
    }

    private func saveCommitments() {
        let trimmedWisdom = wisdom.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedElement = element.trimmingCharacters(in: .whitespacesAndNewlines)
        store.updateWisdomAndElement(skill,
                                    wisdom: trimmedWisdom.isEmpty ? nil : trimmedWisdom,
                                    element: trimmedElement.isEmpty ? nil : trimmedElement)
        commitmentsDirty = false
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
                    notesDirty = new != (skill.notes ?? "")
                }
            if notesDirty {
                HStack {
                    Button("Save note") {
                        store.updateNotes(skill, notes: notes)
                        notesDirty = false
                    }
                    Button("Revert", role: .cancel) {
                        notes = skill.notes ?? ""
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
            Button("Delete Skill…", role: .destructive) { confirmingDelete = true }
        }
        .confirmationDialog("Delete “\(skill.name)”?", isPresented: $confirmingDelete,
                            titleVisibility: .visible) {
            Button("Delete Skill", role: .destructive) { store.delete(skill) }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("Books that listed its lessons keep their count but lose the named link; journal entries keep their text.")
        }
    }

    // MARK: Helpers

    private func sectionTitle(_ title: String) -> some View {
        Text(title.uppercased())
            .font(.caption.weight(.semibold))
            .foregroundStyle(.secondary)
    }

    private func load() {
        wisdom = skill.wisdom ?? ""
        element = skill.element ?? ""
        commitmentsDirty = false
        notes = skill.notes ?? ""
        notesDirty = false
    }
}