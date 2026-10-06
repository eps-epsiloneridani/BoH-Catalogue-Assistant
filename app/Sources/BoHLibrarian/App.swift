import SwiftUI

@main
struct BoHLibrarianApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @State private var appState = AppState()

    var body: some Scene {
        WindowGroup {
            RootView()
                .environment(appState)
                .frame(minWidth: 960, minHeight: 600)
        }
        .windowToolbarStyle(.unified)
        .commands {
            CommandMenu("Go") {
                ForEach(AppSection.allCases) { section in
                    Button("Go to \(section.title)") {
                        appState.section = section
                    }
                    .keyboardShortcut(KeyEquivalent(Character(String(section.rawValue))))
                }
                Divider()
                Button("New Journal Note") {
                    appState.section = .journal
                    appState.journalStore?.requestFocus = true
                }
                .keyboardShortcut("j", modifiers: [.command, .shift])
                .help("Jump to the journal and start a quick note (⌘⇧J)")
            }
        }
    }
}

/// `swift run` launches the process without a Dock presence; claim a proper
/// regular app activation so the window behaves like a native Mac app.
/// (A packaged .app bundle — Phase 6 — gets this for free.)
final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
    }
}