# GUI plan — BoH Librarian (native macOS app)

## Product vision

A quiet, fast, keyboard-friendly Mac window that sits next to the game and answers two
questions instantly: *"What did I discover?"* and *"What can I read this book with?"*
It replaces the spreadsheet BoH players keep (noting which book gives which memory) and
makes that data queryable. Single user, offline, no accounts, no sync.

## Stack (decided — see DECISIONS.md)

- **SwiftUI** (AppKit interop where needed), **Swift 5 language mode**, macOS **14+**.
- **Swift Package Manager only** — no `.xcodeproj`. Two targets: a library
  (`BoHLibrarianCore`: models, DB layer, repositories — so it's testable) and a thin
  executable (`BoHLibrarian`: `@main` + views glue).
- **System SQLite** via the `SQLite3` C module behind a small wrapper. **No external
  dependencies** (D2).

### `app/` layout (as built — Phases 1–5)

```
app/
  Package.swift
  Sources/BoHLibrarianCore/                  // the testable library
    SQLiteDatabase.swift / SQLiteValue.swift  // thin SQLite C-API wrapper
    Migrator.swift                            // migrations: env → repo dirs → bundle
    DatabaseLocation.swift                     // Boh.db path resolution (D7)
    Models.swift                              // all value types + value-set enums
    BookQuery.swift / MemoryQuery.swift / SkillQuery.swift   // pure filter/sort/search
    ReadingMath.swift                         // Reading Helper sentence composition
    Repositories/                             // Book, Memory, Skill, Journal, Playthrough,
                                               // LookupRepositories (Principles, Languages)
    Resources/Migrations/                     // synced copy of db/migrations/
  Sources/BoHLibrarian/                       // the SwiftUI app
    App.swift                                 // @main, window, Go menu + ⌘⇧J, activation fix
    AppState.swift                            // db bootstrap, playthrough lifecycle, store wiring
    RootView.swift                            // sidebar (switcher + sections) + detail routing
    Stores/                                   // Books, Memories, ReadingHelper, Skills, Journal
    Views/                                    // Books/, Memories/, ReadingHelper/, Skills/,
                                               // Journal/, Playthroughs/, Shared/ (badges, aspect editor)
  Tests/BoHLibrarianCoreTests/                // 101 tests on :memory: databases
```

Migration source of truth stays `db/migrations/`; `scripts/sync-migrations.sh` copies
it into the package resources, and `Migrator` prefers the repo directory when running
from source. The bundled copy guarantees a packaged app can migrate a fresh db (D5, D7).
Note: stores live in the executable target, so their glue logic is covered by the
`docs/MANUAL_TEST.md` checklists; the *behavior* they rely on (queries, filters, math,
journal wording) is pure code in Core and unit-tested.

### DB path resolution (D7)

`BOH_DB_PATH` → `./Boh.db` → `../Boh.db` (covers `swift run` from repo root or from `app/`)
→ `~/Library/Application Support/BoH Librarian/Boh.db` (packaged app; auto-created and
migrated if absent). Migrations resolve as `BOH_MIGRATIONS` → `./db/migrations` →
`../db/migrations` → bundled resources copy (`scripts/sync-migrations.sh` keeps the bundle
in sync with `db/migrations/`; a test fails if they drift). The chosen path is shown in
the app's status footer.

## Screens

### Sidebar (NavigationSplitView)

**Books · Memories · Skills · Journal · Reading Helper** — plus a small footer showing db
path and migration version. Shortcuts: ⌘1…⌘5, ⌘R helper, ⌘N new (section-aware),
⌘F focus search.

### Books

- **List**: columns Title, Status, Mystery (principle badge + level), Language, Set.
  Search-as-you-type; filter chips (Uncatalogued / Catalogued / Mastered / Contaminated);
  sort by title/mystery/series. Status shown with the game's colour language (e.g. green
  tick = mastered).
- **Detail**: everything about the book — mystery requirement, language (with a
  "known / NOT known" hint from `Skills.is_language`), contamination, location, yielded
  memory (clickable → memory detail), lessons granted, read history, notes, linked Journal
  entries.
- **Add/Edit sheet**: title, kind, set/volume, language picker, mystery principle+level,
  contamination, location. Deliberately quick: title + mystery is enough to be useful.
- **Mark as read** (sheet): what memory you used, what memory you gained (quick-add if not
  yet recorded: name + aspect rows), lessons learned → writes `read_status`, `times_read`,
  `first/last_read_at`, `yielded_memory_id`, and a Journal entry.

### Memories

- **Grid/list of cards**: name + aspect badges (colored principle circles with levels),
  persistent marker, kind tag. Filter by principle and minimum level; sort by level/name.
- **Detail**: aspect rows (add/edit/remove principle+level), sources (kind + detail rows),
  "which books yield this" backlinks, notes.

### Skills

- List: name, language flag, primary/secondary principle badges, level stepper (1–9),
  wisdom/element fields, notes. (Level 1 = 2 primary / 1 secondary — displayed computed.)

### Journal

- Reverse-chronological list with day headers; inline quick-add box (⌘⇧J focuses it).
- Each entry: free text, optional links (book/memory/skill shown as chips), in-game day
  field. Filter by linked entity or text search.

### Reading Helper (the payoff screen)

1. Pick a book (search by title; shows unread first).
2. Panel shows: mystery principle + level, language known?, contamination?, book kind
   (film/record → "needs projector/phonograph" hint).
3. Live lists: **memories that satisfy** (canonical query, best first — includes today's
   weather if recorded), **skills that contribute** (with computed contribution), plus a
   plain-English sentence: *"You need Rose 6 — best recorded source: Horizon-Sight (Rose 4)
   + Sky Stories lvl 3 (Rose 2)."*
4. **Log the read** → same write-path as the Books "mark as read" sheet.

### Later (Roadmap Phase 6+)

- Menu-bar quick journal (MenuBarExtra) for one-keystroke notes while playing.
- Always-on-top toggle; window remembers size/position.
- "What can I read today?" dashboard (unread books whose mystery ≤ recorded max per
  principle); export to JSON/CSV; optional opt-in wiki import (D6 boundary).
- Packaging: `scripts/make-app.sh` wraps the release binary into `BoH Librarian.app`
  (Info.plist with `LSMinimumSystemVersion`, bundle id `dev.bohlibrarian.app` — change to
  taste), ad-hoc codesigned, so it gets a Dock icon and ⌘Tab presence.

## Data flow & threading

Simple on purpose: the app is main-actor confined; the DB is opened once; queries are
synchronous (local SQLite, tiny data). Repositories return value structs; SwiftUI state
(`@Observable` store per screen) refreshes after each write. Undo via standard SwiftUI
edit behaviors; no background workers. If a future feature needs concurrency, that's the
moment to revisit (D2/D4).

## Testing

- `swift test` with **in-memory databases**: migration runner (fresh db reaches
  `user_version = 3`; idempotent re-run; 001 refuses non-empty legacy — logic-level), every
  repository (CRUD + junctions + the two canonical queries from DATABASE.md).
- Manual checklist kept in `docs/MANUAL_TEST.md` from Phase 2 onward (flows that need eyes:
  mark-read sheet, reading helper, filters).
- Definition of "works": `swift build` clean, `swift test` green, and the manual checklist
  ticked for the phase's screens.

## Phase mapping

See `docs/ROADMAP.md`. Phase 1 = package scaffold + DB layer + migrator + tests (no UI
beyond an empty window). UI screens land Books-first (Phase 2), because book↔memory
capture is the core loop; the Reading Helper follows once both entities exist (Phase 4).