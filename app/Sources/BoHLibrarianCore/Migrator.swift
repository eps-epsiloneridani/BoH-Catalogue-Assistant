import Foundation

public extension Bundle {
    /// The resource bundle SPM generates for BoHLibrarianCore
    /// (holds the bundled copy of db/migrations — docs/DECISIONS.md D5).
    static var bohLibrarianCore: Bundle { .module }
}

/// One numbered schema migration, loaded from `db/migrations/NNN_name.sql`.
public struct Migration: Identifiable, Equatable {
    public let number: Int
    public let name: String
    public let sql: String

    public init(number: Int, name: String, sql: String) {
        self.number = number
        self.name = name
        self.sql = sql
    }

    public var id: Int { number }
}

public enum MigratorError: Error, CustomStringConvertible {
    case noMigrationsFound(location: String)
    case badFileName(String)
    case unreadableMigration(String, underlying: String)
    case sequenceGap(expected: Int, found: Int)
    case legacyDataPresent(rows: Int)
    case applyFailed(Migration, message: String)
    case versionNotAdvanced(Migration, actual: Int)

    public var description: String {
        switch self {
        case .noMigrationsFound(let location):
            return "no migrations found at \(location)"
        case .badFileName(let file):
            return "migration file names must look like NNN_description.sql — '\(file)'"
        case .unreadableMigration(let file, let underlying):
            return "cannot read migration '\(file)': \(underlying)"
        case .sequenceGap(let expected, let found):
            return "migration numbers must be contiguous: expected #\(expected), found #\(found)"
        case .legacyDataPresent(let rows):
            return "refusing to apply migration 001: the legacy Books/Memories tables contain "
                + "\(rows) row(s). Export that data first, then apply (see docs/DATABASE.md)"
        case .applyFailed(let migration, let message):
            return "migration #\(migration.number) '\(migration.name)' failed: \(message)"
        case .versionNotAdvanced(let migration, let actual):
            return "migration #\(migration.number) '\(migration.name)' finished but user_version is still \(actual)"
        }
    }
}

/// Applies pending migrations to a database. Forward-only: applied migrations are
/// recorded via `PRAGMA user_version` and never re-run (docs/DATABASE.md §Migrations).
public final class Migrator {

    public let migrations: [Migration]

    /// Migrations must be numbered contiguously from 1.
    public init(migrations: [Migration]) throws {
        guard !migrations.isEmpty else {
            throw MigratorError.noMigrationsFound(location: "the provided list")
        }
        let sorted = migrations.sorted { $0.number < $1.number }
        var expected = 1
        for migration in sorted {
            guard migration.number == expected else {
                throw MigratorError.sequenceGap(expected: expected, found: migration.number)
            }
            expected += 1
        }
        self.migrations = sorted
    }

    public func pending(on db: SQLiteDatabase) -> [Migration] {
        let version = db.userVersion
        return migrations.filter { $0.number > version }
    }

    public func apply(to db: SQLiteDatabase) throws {
        for migration in pending(on: db) {
            // 001 drops the pre-project legacy tables; refuse if they still hold data (D8).
            if migration.number == 1 { try legacySafetyCheck(on: db) }
            do {
                try db.executeScript(migration.sql)
            } catch {
                throw MigratorError.applyFailed(migration, message: "\(error)")
            }
            let after = db.userVersion
            if after < migration.number {
                throw MigratorError.versionNotAdvanced(migration, actual: after)
            }
        }
    }

    private func legacySafetyCheck(on db: SQLiteDatabase) throws {
        var rows = 0
        for name in ["Books", "Memories"] {
            guard (try? db.tableExists(name)) == true else { continue }
            rows += (try? db.scalarInt("SELECT COUNT(*) FROM \(name);")) ?? 0
        }
        if rows > 0 { throw MigratorError.legacyDataPresent(rows: rows) }
    }

    // MARK: - Loading

    /// Parse migration files from a directory, sorted by number.
    public static func load(fromDirectory directory: URL) throws -> [Migration] {
        let urls = try FileManager.default.contentsOfDirectory(
            at: directory, includingPropertiesForKeys: nil
        ).filter { $0.pathExtension.lowercased() == "sql" }
        guard !urls.isEmpty else {
            throw MigratorError.noMigrationsFound(location: directory.path)
        }
        return try parse(urls: urls)
    }

    /// Migrations bundled with BoHLibrarianCore (fallback when the repo isn't present).
    public static func bundled() throws -> [Migration] {
        let urls = Bundle.bohLibrarianCore.urls(
            forResourcesWithExtension: "sql", subdirectory: "Migrations"
        ) ?? []
        guard !urls.isEmpty else {
            throw MigratorError.noMigrationsFound(location: "the app bundle's Resources/Migrations")
        }
        return try parse(urls: urls)
    }

    /// Resolution order (D5, D7): `BOH_MIGRATIONS` → `./db/migrations` →
    /// `../db/migrations` → bundled copy.
    public static func resolve() throws -> Migrator {
        if let envPath = ProcessInfo.processInfo.environment["BOH_MIGRATIONS"] {
            return try Migrator(migrations: load(fromDirectory: URL(fileURLWithPath: envPath)))
        }
        let current = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
        for parent in [".", ".."] {
            let directory = URL(
                fileURLWithPath: parent == "."
                    ? "db/migrations"
                    : "../db/migrations",
                relativeTo: parent == "." ? current : current.deletingLastPathComponent()
            )
            if FileManager.default.fileExists(atPath: directory.path),
               let list = try? load(fromDirectory: directory), !list.isEmpty {
                return try Migrator(migrations: list)
            }
        }
        return try Migrator(migrations: bundled())
    }

    private static func parse(urls: [URL]) throws -> [Migration] {
        var migrations: [Migration] = []
        for url in urls {
            let base = url.deletingPathExtension().lastPathComponent
            guard let underscore = base.firstIndex(of: "_"),
                  let number = Int(base[base.startIndex..<underscore]) else {
                throw MigratorError.badFileName(url.lastPathComponent)
            }
            guard let sql = try? String(contentsOf: url, encoding: .utf8) else {
                throw MigratorError.unreadableMigration(url.lastPathComponent, underlying: "not UTF-8 text")
            }
            let name = String(base[base.index(after: underscore)...])
            migrations.append(Migration(number: number, name: name, sql: sql))
        }
        return migrations.sorted { $0.number < $1.number }
    }
}