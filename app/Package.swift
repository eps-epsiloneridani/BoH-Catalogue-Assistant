// swift-tools-version: 6.1
// BoH Librarian — native macOS app for recording Book of Hours findings.
// Layout and rationale: docs/GUI_PLAN.md. Decisions D2/D4 in docs/DECISIONS.md.

import PackageDescription

let package = Package(
    name: "BoHLibrarian",
    platforms: [.macOS(.v14)],
    targets: [
        // The library: models, SQLite layer, repositories, migrator. Everything
        // testable without a GUI.
        .target(
            name: "BoHLibrarianCore",
            path: "Sources/BoHLibrarianCore",
            resources: [.copy("Resources/Migrations")],
            swiftSettings: [.swiftLanguageMode(.v5)]
        ),
        // The app: a thin executable holding the SwiftUI views and app state.
        .executableTarget(
            name: "BoHLibrarian",
            dependencies: ["BoHLibrarianCore"],
            path: "Sources/BoHLibrarian",
            swiftSettings: [.swiftLanguageMode(.v5)]
        ),
        .testTarget(
            name: "BoHLibrarianCoreTests",
            dependencies: ["BoHLibrarianCore"],
            path: "Tests/BoHLibrarianCoreTests",
            swiftSettings: [.swiftLanguageMode(.v5)]
        ),
    ]
)