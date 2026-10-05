import Foundation

/// Resolves where Boh.db lives (docs/DECISIONS.md D7):
/// `BOH_DB_PATH` → `./Boh.db` → `../Boh.db` → `~/Library/Application Support/BoH Librarian/Boh.db`.
///
/// The first two cover running from the repo root or from `app/` via `swift run`;
/// the last one is for a packaged app. A fresh database at the resolved path is fine —
/// the Migrator will create the full schema in it.
public enum DatabaseLocation {

    public static func resolvePath() throws -> String {
        if let envPath = ProcessInfo.processInfo.environment["BOH_DB_PATH"] {
            return envPath
        }
        let fileManager = FileManager.default
        for candidate in ["Boh.db", "../Boh.db"] {
            if fileManager.fileExists(atPath: candidate) {
                return candidate
            }
        }
        let support = try fileManager.url(
            for: .applicationSupportDirectory, in: .userDomainMask,
            appropriateFor: nil, create: true
        )
        let directory = support.appendingPathComponent("BoH Librarian", isDirectory: true)
        try fileManager.createDirectory(at: directory, withIntermediateDirectories: true)
        return directory.appendingPathComponent("Boh.db").path
    }
}