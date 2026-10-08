import SwiftUI
import BoHLibrarianCore

// The Skills screen: every skill and language learned this playthrough, with the
// level stepper and the contribution math front and centre. Per docs/GUI_PLAN.md.

struct SkillsScreen: View {
    let store: SkillsStore

    @Environment(AppState.self) private var appState

    @State private var addingSkill = false
    @State private var editingSkill: Skill?

    init(store: SkillsStore) {
        self.store = store
    }

    var body: some View {
        @Bindable var store = store
        return Group {
            if store.skills.isEmpty {
                emptyState
            } else {
                content
            }
        }
        .frame(minWidth: 640)
        .onAppear { store.reload() }
        .searchable(text: $store.options.searchText, placement: .toolbar,
                    prompt: "Search name, wisdom, notes…")
        .toolbar {
            ToolbarItemGroup(placement: .primaryAction) {
                Button {
                    addingSkill = true
                } label: {
                    Label("Add Skill", systemImage: "plus")
                }
                .keyboardShortcut("n")
                .help("Record a newly learned skill or language (⌘N)")

                kindMenu
                principleMenu
                sortMenu
            }
        }
        .alert("Something went wrong", isPresented: errorBinding) {
            Button("OK") { store.lastError = nil }
        } message: {
            Text(store.lastError ?? "")
        }
        .sheet(isPresented: $addingSkill) {
            SkillFormView(mode: .add, principles: appState.principles) { draft in
                store.add(draft)
            }
        }
        .sheet(item: $editingSkill) { skill in
            SkillFormView(mode: .edit(skill), principles: appState.principles) { draft in
                store.update(skill, with: draft)
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
            skillList
                .frame(minWidth: 260, idealWidth: 320, maxWidth: 480)
            Divider()
            if let skill = store.selectedSkill {
                SkillDetailView(skill: skill, store: store,
                                onEdit: { editingSkill = skill })
            } else {
                ContentUnavailableView("Select a skill", systemImage: "graduationcap",
                                       description: Text("Pick a skill to level it up, see what it contributes, and where it sits on the Tree of Wisdoms."))
            }
        }
    }

    private var skillList: some View {
        let selection = Binding<Int64?>(
            get: { store.selectedSkillID },
            set: { store.selectedSkillID = $0 }
        )
        return List(selection: selection) {
            ForEach(store.displayed) { skill in
                SkillRow(skill: skill, store: store)
                    .tag(skill.id)
            }
        }
        .listStyle(.inset)
    }

    private var emptyState: some View {
        ContentUnavailableView {
            Label("No skills recorded yet", systemImage: "graduationcap")
        } description: {
            Text("Record each skill as you learn it from Lessons — level 1 shows 2 of its primary principle and 1 of its secondary.")
        } actions: {
            Button("Add Skill") { addingSkill = true }
                .keyboardShortcut(.defaultAction)
        }
    }

    // MARK: Toolbar menus

    private var kindMenu: some View {
        @Bindable var store = store
        return Menu {
            Picker("Show", selection: $store.options.kindFilter) {
                ForEach(SkillKindFilter.allCases) { filter in
                    Text(filter.rawValue).tag(filter)
                }
            }
        } label: {
            Label("Kind", systemImage: "line.3.horizontal.decrease.circle")
        }
        .help("All skills, languages only, or non-language skills only")
    }

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
        .help("Skills with this principle as primary or secondary")
    }

    private var sortMenu: some View {
        @Bindable var store = store
        return Menu {
            Picker("Sort by", selection: $store.options.sort) {
                ForEach(SkillSort.allCases) { sort in
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

private struct SkillRow: View {
    let skill: Skill
    let store: SkillsStore

    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                HStack(spacing: 4) {
                    if skill.isLanguage {
                        Image(systemName: "globe")
                            .font(.caption)
                            .foregroundStyle(.blue)
                            .accessibilityLabel("language skill")
                            .help("Language skill")
                    }
                    Text(skill.name)
                        .font(.body)
                        .lineLimit(1)
                }
                if let wisdom = skill.wisdom {
                    Text(wisdom)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                }
            }
            Spacer(minLength: 8)
            VStack(alignment: .trailing, spacing: 3) {
                // The card's own numbers: level L shows L+1 primary, L secondary.
                HStack(spacing: 4) {
                    if let primary = skill.primaryPrincipleID,
                       let level = skill.level,
                       let contributions = contributions {
                        PrincipleBadge(name: store.principleName(primary),
                                       level: contributions.primary,
                                       colorHex: store.principleColor(primary))
                        if let secondary = skill.secondaryPrincipleID {
                            PrincipleBadge(name: store.principleName(secondary),
                                           level: contributions.secondary,
                                           colorHex: store.principleColor(secondary))
                        }
                    }
                }
                if let level = skill.level {
                    Text("level \(level)")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                } else {
                    Text("unleveled")
                        .font(.caption2)
                        .foregroundStyle(.tertiary)
                }
            }
        }
        .padding(.vertical, 2)
    }

    private var contributions: (primary: Int, secondary: Int)? {
        skill.level.map { SkillMath.contributions(level: $0) }
    }
}