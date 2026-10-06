import Foundation

// Populating a playthrough from a Book of Hours save game.
// Feasibility, formats and gotchas are documented in docs/SAVE_IMPORT.md; this
// implements that design. The save and the game's definition files are lenient
// JSON (mixed UTF-16/UTF-8, raw control characters, trailing commas), so nothing
// here uses Swift's strict Codable paths.

// MARK: - Lenient JSON

enum LenientJSON {

    /// Parse the game's JSON dialect: UTF-16 with BOM or UTF-8; raw control
    /// characters inside strings; trailing commas before `]`/`}`.
    static func object(from data: Data) -> Any? {
        let text: String
        if data.count >= 2, data[data.startIndex] == 0xFF, data[data.index(after: data.startIndex)] == 0xFE {
            text = String(data: data, encoding: .utf16) ?? ""
        } else {
            text = String(data: data, encoding: .utf8) ?? ""
        }
        var cleaned = text
        if cleaned.hasPrefix("\u{FEFF}") { cleaned.removeFirst() }
        cleaned = escapingControlCharacters(in: cleaned)
        cleaned = strippingTrailingCommas(in: cleaned)
        guard let payload = cleaned.data(using: .utf8) else { return nil }
        return try? JSONSerialization.jsonObject(with: payload, options: [.fragmentsAllowed])
    }

    /// Escape raw control characters that appear inside JSON strings.
    static func escapingControlCharacters(in text: String) -> String {
        guard text.unicodeScalars.contains(where: { $0.value < 0x20 }) else {
            return text  // fast path: nothing to escape
        }
        var out = String()
        out.reserveCapacity(text.count)
        var inString = false
        var escaped = false
        for scalar in text.unicodeScalars {
            if inString {
                if escaped {
                    escaped = false
                    out.unicodeScalars.append(scalar)
                } else if scalar == "\\" {
                    escaped = true
                    out.unicodeScalars.append(scalar)
                } else if scalar == "\"" {
                    inString = false
                    out.unicodeScalars.append(scalar)
                } else if scalar.value < 0x20 {
                    out += String(format: "\\u%04x", scalar.value)
                } else {
                    out.unicodeScalars.append(scalar)
                }
            } else {
                if scalar == "\"" { inString = true }
                out.unicodeScalars.append(scalar)
            }
        }
        return out
    }

    /// Remove trailing commas the game's parser tolerates.
    static func strippingTrailingCommas(in text: String) -> String {
        let pattern = ",(\\s*[\\]}])"
        guard let regex = try? NSRegularExpression(pattern: pattern) else { return text }
        let range = NSRange(text.startIndex..., in: text)
        return regex.stringByReplacingMatches(in: text, options: [], range: range,
                                              withTemplate: "$1")
    }

    // Typed accessors over JSONSerialization output.
    static func dictionary(_ any: Any?) -> [String: Any]? {
        any as? [String: Any]
    }

    static func dictionaries(_ any: Any?) -> [[String: Any]] {
        (any as? [[String: Any]]) ?? []
    }

    static func string(_ any: Any?) -> String? {
        any as? String
    }

    static func intValue(_ any: Any?) -> Int? {
        if let value = any as? Int { return value }
        if let value = any as? NSNumber { return value.intValue }
        if let value = any as? String { return Int(value) }
        return nil
    }
}

// MARK: - Standard paths

public enum BoHPaths {

    /// `~/Library/Application Support/Weather Factory/Book of Hours`.
    /// Override for tests with `BOH_SAVE_DIR`.
    public static func saveDirectory() -> URL? {
        if let override = ProcessInfo.processInfo.environment["BOH_SAVE_DIR"] {
            return URL(fileURLWithPath: override)
        }
        guard let support = FileManager.default.urls(for: .applicationSupportDirectory,
                                                     in: .userDomainMask).first else {
            return nil
        }
        return support.appendingPathComponent("Weather Factory/Book of Hours", isDirectory: true)
    }

    /// `…/Steam/steamapps/common/Book of Hours/OSX.app/…/bhcontent/core/elements`.
    /// Override for tests with `BOH_GAME_ELEMENTS`.
    public static func gameElementsDirectory() -> URL? {
        if let override = ProcessInfo.processInfo.environment["BOH_GAME_ELEMENTS"] {
            return URL(fileURLWithPath: override)
        }
        guard let support = FileManager.default.urls(for: .applicationSupportDirectory,
                                                     in: .userDomainMask).first else {
            return nil
        }
        return support
            .appendingPathComponent("Steam/steamapps/common/Book of Hours/OSX.app"
                                    + "/Contents/Resources/Data/StreamingAssets"
                                    + "/bhcontent/core/elements", isDirectory: true)
    }
}

// MARK: - Save scanning

/// One save game found in the standard install path (summary only — the file is
/// only fully parsed when an import runs).
public struct SaveGameSummary: Identifiable, Hashable {
    public let url: URL
    public let fileName: String
    public let modifiedAt: Date?
    public let gameVersion: String?
    public let bookCount: Int
    public let masteredCount: Int
    public let skillStackCount: Int

    public var id: String { url.path }

    /// "AUTOSAVE" / "save" — the extension-less stem.
    public var stem: String {
        (fileName as NSString).deletingPathExtension
    }
}

public enum SaveScanner {

    /// Save-game candidates in the standard directory, newest first. Only files
    /// that actually contain a `RootPopulationCommand` count (the folder also
    /// holds achievements/config, which don't). Quick string scan, no full parse.
    public static func availableSaves() -> [SaveGameSummary] {
        guard let directory = BoHPaths.saveDirectory(),
              let files = try? FileManager.default.contentsOfDirectory(
                  at: directory, includingPropertiesForKeys: [.contentModificationDateKey])
        else { return [] }

        var summaries: [SaveGameSummary] = []
        for file in files where file.pathExtension.lowercased() == "json" {
            guard let data = try? Data(contentsOf: file),
                  let text = decodedText(from: data),
                  text.contains("\"RootPopulationCommand\"") else { continue }
            let attributes = try? file.resourceValues(forKeys: [.contentModificationDateKey])
            summaries.append(SaveGameSummary(
                url: file,
                fileName: file.lastPathComponent,
                modifiedAt: attributes?.contentModificationDate,
                gameVersion: version(in: text),
                bookCount: countOccurrences(of: "\"EntityId\":\"t.", in: text),
                masteredCount: countOccurrences(of: "\"mastery.", in: text),
                skillStackCount: countOccurrences(of: "\"EntityId\":\"s.", in: text)
            ))
        }
        return summaries.sorted { ($0.modifiedAt ?? .distantPast) > ($1.modifiedAt ?? .distantPast) }
    }

    private static func decodedText(from data: Data) -> String? {
        if data.count >= 2, data[data.startIndex] == 0xFF, data[data.index(after: data.startIndex)] == 0xFE {
            var text = String(data: data, encoding: .utf16) ?? ""
            if text.hasPrefix("\u{FEFF}") { text.removeFirst() }
            return text
        }
        return String(data: data, encoding: .utf8)
    }

    private static func version(in text: String) -> String? {
        // The save serialises compactly ("Version":"x"); hand-written fixtures
        // may include spaces. Allow either.
        guard let regex = try? NSRegularExpression(pattern: "\\\"Version\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"") else {
            return nil
        }
        let range = NSRange(text.startIndex..., in: text)
        guard let match = regex.firstMatch(in: text, options: [], range: range),
              match.numberOfRanges > 1,
              let found = Range(match.range(at: 1), in: text) else { return nil }
        return String(text[found])
    }

    private static func countOccurrences(of needle: String, in text: String) -> Int {
        var count = 0
        var searchRange = text.startIndex..<text.endIndex
        while let found = text.range(of: needle, range: searchRange) {
            count += 1
            searchRange = found.upperBound..<text.endIndex
        }
        return count
    }
}

// MARK: - Import

public struct ImportReport {
    public var booksCreated = 0
    public var booksUpdated = 0
    public var skillsCreated = 0
    public var skillsUpdated = 0
    public var memoriesCreated = 0
    public var languagesAdded: [String] = []
    public var uncataloguedSkipped = 0
    public var warnings: [String] = []

    public var summary: String {
        var parts = ["\(booksCreated + booksUpdated) books (\(booksCreated) new, \(booksUpdated) updated)",
                     "\(skillsCreated + skillsUpdated) skills (\(skillsCreated) new, \(skillsUpdated) updated)",
                     "\(memoriesCreated) memories created"]
        if !languagesAdded.isEmpty {
            parts.append("languages added: " + languagesAdded.joined(separator: ", "))
        }
        if uncataloguedSkipped > 0 {
            parts.append("\(uncataloguedSkipped) uncatalogued texts skipped (identity unknown)")
        }
        if !warnings.isEmpty {
            parts.append("\(warnings.count) warning(s)")
        }
        return parts.joined(separator: " · ")
    }
}

public enum SaveImportError: Error, CustomStringConvertible {
    case unreadableSave(URL)
    case gameDataNotFound
    case noActivePlaythrough

    public var description: String {
        switch self {
        case .unreadableSave(let url): return "couldn't read save at \(url.path)"
        case .gameDataNotFound: return "Book of Hours game data not found at the standard Steam path"
        case .noActivePlaythrough: return "no playthrough to import into"
        }
    }
}

public final class SaveImporter {

    // MARK: Entry point

    /// Import one save into a playthrough. All writes happen in the caller's
    /// transaction (the database is main-thread confined per docs/GUI_PLAN.md).
    public static func run(saveURL: URL, db: SQLiteDatabase, playthroughID: Int64,
                           elementsDirectory: URL) throws -> ImportReport {
        guard let saveRoot = LenientJSON.object(from: try Data(contentsOf: saveURL)) as? [String: Any],
              let root = LenientJSON.dictionary(saveRoot["RootPopulationCommand"]) else {
            throw SaveImportError.unreadableSave(saveURL)
        }
        let index = try elementIndex(in: elementsDirectory)

        // Player state: element stacks, deduped by EntityId (the save keeps copies
        // of committed/uncommitted skill cards and moved-around books).
        var stacks: [String: [String: Any]] = [:]
        var uncatalogued = 0
        for sphere in LenientJSON.dictionaries(root["Spheres"]) {
            collectStacks(fromSphere: sphere, into: &stacks, uncatalogued: &uncatalogued)
        }

        // Our lookups.
        let principles = try PrincipleRepository(db: db).all()
        var principleIDByName: [String: Int64] = [:]
        var principleNameByID: [Int64: String] = [:]
        for principle in principles {
            principleIDByName[principle.name.lowercased()] = principle.id
            principleNameByID[principle.id] = principle.name
        }
        let languageRepo = LanguageRepository(db: db)
        var languages = try languageRepo.all()
        var languageIDByName: [String: Int64] = [:]
        for language in languages { languageIDByName[language.name.lowercased()] = language.id }

        let bookRepo = BookRepository(db: db, playthroughID: playthroughID)
        let memoryRepo = MemoryRepository(db: db, playthroughID: playthroughID)
        let skillRepo = SkillRepository(db: db, playthroughID: playthroughID)
        let journalRepo = JournalRepository(db: db, playthroughID: playthroughID)

        var report = ImportReport()
        var existingBooksByTitle: [String: Book] = [:]
        for book in try bookRepo.all() { existingBooksByTitle[book.title] = book }
        var existingMemoriesByName: [String: Memory] = [:]
        for memory in try memoryRepo.all() { existingMemoriesByName[memory.name] = memory }
        var existingSkillsByName: [String: Skill] = [:]
        for skill in try skillRepo.all() { existingSkillsByName[skill.name] = skill }

        // Pass 1: skills (books reference them via lessons).
        var skillIDByElementID: [String: Int64] = [:]
        for (entityID, stack) in stacks where entityID.hasPrefix("s.") {
            guard let def = index[entityID] else {
                report.warnings.append("skill \(entityID) has no game definition — skipped")
                continue
            }
            let level = 1 + (stackMutation("skill", in: stack) ?? 0)
            guard let name = LenientJSON.string(def["Label"]) else { continue }
            let isLanguage = LenientJSON.dictionary(def["aspects"])?["skill.language"] != nil
            let wisdom = stack.keys.first(where: { $0.hasPrefix("w.") && (stack[$0] as? Int) == -1 })?.dropFirst(2)
            let elementCode = stack.keys.first(where: { $0.hasPrefix("a.x") })?.dropFirst(3)

            if var existing = existingSkillsByName[name] {
                if existing.level == nil { existing.level = level }
                if existing.isLanguage == false && isLanguage { existing.isLanguage = true }
                if existing.wisdom == nil, let wisdom { existing.wisdom = String(wisdom) }
                if existing.element == nil, let code = elementCode,
                   let element = Self.elementName(forCode: String(code)) {
                    existing.element = element
                }
                try skillRepo.update(existing)
                report.skillsUpdated += 1
                skillIDByElementID[entityID] = existing.id
            } else {
                let draft = SkillDraft(
                    name: name,
                    isLanguage: isLanguage,
                    primaryPrincipleID: principleIDByName[Self.principleKey(withValue: 2, in: def).lowercased()],
                    secondaryPrincipleID: principleIDByName[Self.principleKey(withValue: 1, in: def).lowercased()],
                    level: level,
                    wisdom: wisdom.map(String.init),
                    element: elementCode.flatMap { Self.elementName(forCode: String($0)) },
                    notes: nil
                )
                let created = try skillRepo.insert(draft)
                report.skillsCreated += 1
                existingSkillsByName[name] = created
                skillIDByElementID[entityID] = created.id
            }
        }

        // Pass 2: books.
        for (entityID, stack) in stacks where entityID.hasPrefix("t.") {
            guard let def = index[entityID],
                  let title = LenientJSON.string(def["Label"]) else {
                report.warnings.append("book \(entityID) has no game definition — skipped")
                continue
            }
            let aspects = LenientJSON.dictionary(def["aspects"]) ?? [:]
            let mutations = stack

            // Requirement.
            var principleID: Int64?
            var difficulty: Int?
            if let key = aspects.keys.first(where: { $0.hasPrefix("mystery.") }) {
                principleID = principleIDByName[String(key.dropFirst("mystery.".count)).lowercased()]
                difficulty = LenientJSON.intValue(aspects[key])
            }
            // Fallback: the mastered mutation's value equals the difficulty.
            if difficulty == nil, let mastered = mutations.keys.first(where: { $0.hasPrefix("mastery.") }) {
                difficulty = LenientJSON.intValue(mutations[mastered])
            }

            // Language.
            var languageID: Int64?
            if let writtenKey = aspects.keys.first(where: { $0.hasPrefix("w.") }) {
                let name = Self.languageName(forAspectSuffix: String(writtenKey.dropFirst(2)),
                                             index: index)
                if let id = languageIDByName[name.lowercased()] {
                    languageID = id
                } else {
                    let added = try languageRepo.insert(name: name, native: false)
                    languages.append(added)
                    languageIDByName[name.lowercased()] = added.id
                    report.languagesAdded.append(name)
                    languageID = added.id
                }
            }

            // State.
            let mastered = mutations.keys.contains { $0.hasPrefix("mastery.") }
            var contamination: Contamination?
            if let key = mutations.keys.first(where: { $0.hasPrefix("contamination.") }) {
                contamination = Contamination(rawValue: String(key.dropFirst("contamination.".count)))
            }

            // Lessons.
            var lessonEntries: [BookLessonsEntry] = []
            let xtriggers = LenientJSON.dictionary(def["xtriggers"]) ?? [:]
            for (key, effects) in xtriggers where key.hasPrefix("mastering.") {
                for effect in LenientJSON.dictionaries(effects) {
                    guard let targetID = LenientJSON.string(effect["id"]) else { continue }
                    let skillElement = "s." + targetID.replacingOccurrences(
                        of: "^(x\\.|s\\.)", with: "", options: .regularExpression)
                    if let skillID = skillIDByElementID[skillElement] {
                        lessonEntries.append(BookLessonsEntry(
                            skillID: skillID,
                            amount: LenientJSON.intValue(effect["level"]) ?? 1))
                    } else if let def = index[skillElement],
                              let name = LenientJSON.string(def["Label"]),
                              let skill = existingSkillsByName[name] {
                        lessonEntries.append(BookLessonsEntry(
                            skillID: skill.id,
                            amount: LenientJSON.intValue(effect["level"]) ?? 1))
                    } else {
                        report.warnings.append("\(title): lesson for unknown skill \(skillElement) skipped")
                    }
                }
            }

            // Yielded memory (the memory every read of this book gives).
            var yieldedMemoryID: Int64?
            for (key, effects) in xtriggers where key.hasPrefix("reading.") {
                for effect in LenientJSON.dictionaries(effects) {
                    guard let memoryElement = LenientJSON.string(effect["id"]),
                          let memDef = index[memoryElement],
                          let memoryName = LenientJSON.string(memDef["Label"]) else { continue }
                    if let existing = existingMemoriesByName[memoryName] {
                        yieldedMemoryID = existing.id
                    } else {
                        let draft = Self.memoryDraft(element: memoryElement, definition: memDef,
                                                      principleIDByName: principleIDByName)
                        let created = try memoryRepo.insert(draft)
                        report.memoriesCreated += 1
                        existingMemoriesByName[memoryName] = created
                        yieldedMemoryID = created.id
                    }
                    break
                }
            }

            let lessonCount = lessonEntries.reduce(0) { $0 + $1.amount }
            let draft = BookDraft(
                title: title,
                bookKind: Self.bookKind(in: aspects),
                languageID: languageID,
                mysteryPrincipleID: principleID,
                difficulty: difficulty,
                readStatus: mastered ? .mastered : .catalogued,
                contamination: contamination,
                lessons: lessonCount > 0 ? lessonCount : nil,
                yieldedMemoryID: yieldedMemoryID,
                notes: nil
            )

            if var existing = existingBooksByTitle[title] {
                // Fill empty fields only — never stomp user-recorded data.
                if existing.mysteryPrincipleID == nil { existing.mysteryPrincipleID = draft.mysteryPrincipleID }
                if existing.difficulty == nil { existing.difficulty = draft.difficulty }
                if existing.languageID == nil { existing.languageID = draft.languageID }
                if existing.contamination == nil { existing.contamination = draft.contamination }
                if existing.lessons == nil { existing.lessons = draft.lessons }
                if existing.yieldedMemoryID == nil { existing.yieldedMemoryID = draft.yieldedMemoryID }
                if mastered && existing.readStatus != .mastered { existing.readStatus = .mastered }
                try bookRepo.update(existing)
                if !lessonEntries.isEmpty { try bookRepo.setLessons(existing.id, lessonEntries) }
                existingBooksByTitle[title] = existing
                report.booksUpdated += 1
            } else {
                let created = try bookRepo.insert(draft)
                if !lessonEntries.isEmpty { try bookRepo.setLessons(created.id, lessonEntries) }
                existingBooksByTitle[title] = created
                report.booksCreated += 1
            }
        }

        report.uncataloguedSkipped = uncatalogued

        _ = try journalRepo.insert(JournalDraft(
            entry: "Imported from \(saveURL.lastPathComponent): \(report.summary).",
            bookID: nil, memoryID: nil, skillID: nil))
        return report
    }

    // MARK: Structure walking

    private static func collectStacks(fromSphere sphere: [String: Any],
                                      into stacks: inout [String: [String: Any]],
                                      uncatalogued: inout Int) {
        for token in LenientJSON.dictionaries(sphere["Tokens"]) {
            let payload = LenientJSON.dictionary(token["Payload"]) ?? [:]
            let payloadType = LenientJSON.string(payload["$type"]) ?? ""
            if payloadType.hasPrefix("ElementStackCreationCommand"),
               let entityID = LenientJSON.string(payload["EntityId"]) {
                let mutations = LenientJSON.dictionary(payload["Mutations"]) ?? [:]
                if entityID.hasPrefix("uncatbook.") {
                    uncatalogued += 1
                } else if var existing = stacks[entityID] {
                    // Merge duplicate copies (committed/uncommitted skills, moved books).
                    for (key, value) in mutations where existing[key] == nil {
                        existing[key] = value
                    }
                    stacks[entityID] = existing
                } else {
                    var cleaned = mutations
                    cleaned.removeValue(forKey: "$type")
                    stacks[entityID] = cleaned
                }
            }
            for dominion in LenientJSON.dictionaries(payload["Dominions"]) {
                for nested in LenientJSON.dictionaries(dominion["Spheres"]) {
                    collectStacks(fromSphere: nested, into: &stacks, uncatalogued: &uncatalogued)
                }
            }
        }
    }

    private static func elementIndex(in directory: URL) throws -> [String: [String: Any]] {
        var index: [String: [String: Any]] = [:]
        let files = try FileManager.default.contentsOfDirectory(at: directory,
                                                                includingPropertiesForKeys: nil)
        for file in files where file.pathExtension.lowercased() == "json" {
            guard let data = try? Data(contentsOf: file),
                  let root = LenientJSON.object(from: data) as? [String: Any],
                  let elements = root["elements"] as? [[String: Any]] else { continue }
            for element in elements {
                if let id = LenientJSON.string(element["ID"]) ?? LenientJSON.string(element["id"]) {
                    index[id] = element
                }
            }
        }
        return index
    }

    // MARK: Mappings

    private static func stackMutation(_ key: String, in stack: [String: Any]) -> Int? {
        LenientJSON.intValue(stack[key])
    }

    /// The aspect key whose value is `value` — a level-1 skill's primary carries 2,
    /// its secondary 1 (docs/GAME_MECHANICS.md §Skills).
    private static func principleKey(withValue value: Int, in def: [String: Any]) -> String {
        let aspects = LenientJSON.dictionary(def["aspects"]) ?? [:]
        let candidates = aspects.keys.filter { key in
            guard let aspectValue = LenientJSON.intValue(aspects[key]), aspectValue == value
            else { return false }
            // Skip markers: skill, skill.language, wisdom options (w.*), evolves (e.*), boosts.
            return key == key.lowercased() && !key.hasPrefix("w.")
                && !key.hasPrefix("e.") && !key.hasPrefix("boost.")
                && key != "skill" && key != "skill.language"
        }
        return candidates.sorted().first ?? ""
    }

    private static let nativeLanguageNames: Set<String> =
        ["greek", "latin", "sanskrit", "aramaic", "phrygian"]

    private static func languageName(forAspectSuffix suffix: String,
                                     index: [String: [String: Any]]) -> String {
        if nativeLanguageNames.contains(suffix) {
            return suffix.prefix(1).uppercased() + suffix.dropFirst()
        }
        if let label = LenientJSON.string((index["s." + suffix] ?? [:])["Label"]) {
            return label
        }
        return suffix
    }

    private static let elementCodes: [String: String] = [
        "hea": "Health", "cho": "Chor", "sha": "Shapt", "met": "Mettle",
        "ere": "Ereb", "tri": "Trist", "fet": "Fet", "pho": "Phost", "wis": "Wist",
    ]

    private static func elementName(forCode code: String) -> String? {
        elementCodes[code]
    }

    private static func memoryDraft(element: String, definition: [String: Any],
                                    principleIDByName: [String: Int64]) -> MemoryDraft {
        let name = LenientJSON.string(definition["Label"]) ?? element
        let inherits = LenientJSON.string(definition["inherits"]) ?? ""
        let kind: MemoryKind = inherits.contains("numen") ? .numen
            : inherits.contains("weather") ? .weather : .memory
        let persistent = inherits.contains("persistent") || kind == .numen
        let aspects = LenientJSON.dictionary(definition["aspects"]) ?? [:]
        var drafts: [AspectDraft] = []
        for (key, value) in aspects {
            guard !key.hasPrefix("boost."),
                  let level = LenientJSON.intValue(value), level > 0,
                  let principleID = principleIDByName[key.lowercased()]
            else { continue }
            drafts.append(AspectDraft(principleID: principleID, level: level))
        }
        return MemoryDraft(name: name, kind: kind, persistent: persistent,
                           notes: nil, aspects: drafts)
    }

    /// Kind markers in tomes.json (verified against 281 installed tomes):
    /// `codex` is the plain bound-book format (256 of 281) — NOT a phonograph
    /// record; records carry `record.phonograph`. An earlier mapping turned
    /// every imported book into a `record` (migration 007 repairs that data).
    private static func bookKind(in aspects: [String: Any]) -> BookKind {
        if aspects["tablet"] != nil { return .book }  // tablets stay "book" for now (kind set is book/scroll/film/record)
        if aspects["film"] != nil { return .film }
        if aspects["record.phonograph"] != nil { return .record }
        if aspects["scroll"] != nil { return .scroll }
        return .book
    }
}