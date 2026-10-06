import Foundation
import Observation
import BoHLibrarianCore

/// UI state for the Skills screen (mirrors the other stores).
@Observable
final class SkillsStore {

    private let repo: SkillRepository
    private var principlesByID: [Int64: Principle] = [:]

    private(set) var skills: [Skill] = []
    var lastError: String?

    var options = SkillQueryOptions()
    var selectedSkillID: Int64?

    init(db: SQLiteDatabase, playthroughID: Int64) {
        self.repo = SkillRepository(db: db, playthroughID: playthroughID)
        let principles = (try? PrincipleRepository(db: db).all()) ?? []
        principlesByID = Dictionary(uniqueKeysWithValues: principles.map { ($0.id, $0) })
        reload()
    }

    // MARK: Derived

    var displayed: [Skill] {
        SkillFiltering.apply(skills, options: options,
                             principleNames: principlesByID.mapValues(\.name))
    }

    var selectedSkill: Skill? {
        displayed.first { $0.id == selectedSkillID }
    }

    // MARK: Lookups

    func principleName(_ id: Int64?) -> String? {
        id.flatMap { principlesByID[$0]?.name }
    }

    func principleColor(_ id: Int64?) -> String? {
        id.flatMap { principlesByID[$0]?.color }
    }

    // MARK: Data

    func reload() {
        do {
            skills = try repo.all()
        } catch {
            lastError = "\(error)"
        }
    }

    func perform(_ label: String, _ operation: () throws -> Void) {
        do {
            try operation()
            reload()
        } catch {
            lastError = "\(label) failed: \(error)"
        }
    }

    // MARK: CRUD

    func add(_ draft: SkillDraft) {
        perform("Adding skill") { selectedSkillID = try repo.insert(draft).id }
    }

    func update(_ skill: Skill) {
        perform("Saving skill") { try repo.update(skill) }
    }

    /// Apply the edit form's draft onto an existing skill, keeping the id.
    func update(_ original: Skill, with draft: SkillDraft) {
        var skill = original
        skill.name = draft.name
        skill.isLanguage = draft.isLanguage
        skill.primaryPrincipleID = draft.primaryPrincipleID
        skill.secondaryPrincipleID = draft.secondaryPrincipleID
        skill.level = draft.level
        skill.wisdom = draft.wisdom
        skill.element = draft.element
        skill.notes = draft.notes
        update(skill)
    }

    /// Immediate persistence for the detail view's level stepper.
    func setLevel(_ skill: Skill, to level: Int) {
        var edited = skill
        edited.level = level
        update(edited)
    }

    func updateWisdomAndElement(_ skill: Skill, wisdom: String?, element: String?) {
        var edited = skill
        edited.wisdom = wisdom
        edited.element = element
        update(edited)
    }

    func updateNotes(_ skill: Skill, notes: String) {
        var edited = skill
        edited.notes = notes.isEmpty ? nil : notes
        update(edited)
    }

    func delete(_ skill: Skill) {
        perform("Deleting skill") {
            try repo.delete(skill.id)
            if selectedSkillID == skill.id { selectedSkillID = nil }
        }
    }
}