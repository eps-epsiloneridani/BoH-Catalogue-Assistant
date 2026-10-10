# Linux (Qt) Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A native Linux desktop app ("BoH Librarian", Qt Widgets) with identical functionality to the existing SwiftUI macOS app, sharing one schema (`db/migrations/`) and one docs set in this repo.

**Architecture:** Port `BoHLibrarianCore` to C++ essentially 1:1 (SQLite wrapper, Migrator, models, queries, repositories, save import) and carry the 111-test suite over as the porting gate — the tests are the contract, not a re-derivation. Rebuild the UI in Qt Widgets on the SwiftUI screen structure (sidebar + detail panes + modal dialogs for forms/sheets). A single reload choke point in the app controller structurally eliminates the empty-state/stale-snapshot bug class that produced 6 macOS fixes.

**Tech Stack:** C++20, Qt 6.2+ (Core/Gui/Widgets only), CMake ≥ 3.21, system `libsqlite3` (≥ 3.35) via pkg-config, Qt Test for the suite. **No other dependencies; offline build (no FetchContent/CPM).**

**Spec:** `docs/GUI_PLAN.md` (screens), `docs/DATABASE.md` (schema), `docs/DECISIONS.md` (posture D2/D5/D6/D11/D12), `docs/SAVE_IMPORT.md` (import design). This plan argues from those; executors read both.

## Key choices (decided here — the user delegated language and look)

1. **C++20 + Qt 6 Widgets.** Qt's native path; zero binding-layer risk. QML rejected (forms-heavy desktop app; Widgets map 1:1 to the SwiftUI List/Form/Sheet shapes and give native a11y for free). Rust Qt bindings rejected (immature; violates the offline/no-deps spirit of D2).
2. **Lives in this repo, top-level `qt/`.** One schema source, one docs set; the macOS app keeps building untouched. `qt/` (not `linux/`) because nothing in the code is Linux-locked.
3. **Migrations embed straight from `db/migrations/` via a `.qrc`** at build time — the `sync-migrations.sh` drift class is retired for the Qt side (one source, no copy).
4. **Port the SQLite wrapper, don't switch to QSqlQuery.** The wrapper is 286 audited lines; repositories' SQL is tested against it.
5. **Save import ships a folder picker on day one** (the mac app's parked file-picker item): Linux has no single standard Steam layout — the app probes candidates, then always offers `QFileDialog`.

## File structure (as it will be built)

```
qt/
  CMakeLists.txt              # core lib + app exe + 10 test exes; qrc for migrations
  src/core/                   # mirrors BoHLibrarianCore file-for-file (.h/.cpp pairs)
    SQLiteDatabase.{h,cpp}    SQLiteValue.{h,cpp}   Migrator.{h,cpp}
    DatabaseLocation.{h,cpp}  Models.{h,cpp}        migrations.qrc
    BookQuery.{h,cpp}  MemoryQuery.{h,cpp}  SkillQuery.{h,cpp}
    ReadingMath.{h,cpp}  ColorMath.{h,cpp}
    SaveImport.{h,cpp}        # incl. BoHPaths (Linux candidates) + lenient JSON pipeline
    Repositories/             # Book, Memory, Skill, Journal, Playthrough, Lookup
  src/app/                    # mirrors BoHLibrarian
    main.cpp                  AppController.{h,cpp}   # db bootstrap + reload choke point
    Stores/                   # 5 QObjects mirroring Books/Memories/ReadingHelper/Skills/Journal stores
    Views/                    # MainWindow, Sidebar, Books/, Memories/, ReadingHelper/,
                              # Skills/, Journal/, Playthroughs/, Shared/ (BadgeDelegate…)
  tests/                      # one Qt Test exe per ported Swift test file (10)
  packaging/                  # boh-librarian.desktop, icon, make-appimage.sh
```

Shared with the existing repo (unchanged): `db/migrations/` (single schema source), `docs/`, `scripts/assets/` (icon source SVG).

## Global Constraints

- Qt ≥ 6.2, C++20 (GCC 11+ / Clang 14+), CMake ≥ 3.21, SQLite ≥ 3.35, pkg-config.
- Dependencies: Qt + system sqlite3 only. No network, no fetch-at-build, ever (D2 spirit).
- Schema: `db/migrations/*.sql` is the single source of truth. Never edit an applied migration; forward-only; a db NEWER than the app's migrations must fail loudly at launch.
- SQL must be parameterized uniformly (no string interpolation of values — the audited invariant).
- SQLite access: main thread only (same as the Swift app). No async db layer.
- Data dir perms 700 / db perms 600 (D11 posture, mirrored with `QFile::setPermissions`).
- Spoiler posture: earned-only memory visibility (`earnedVisibility`), mastered-only backlinks, unrevealed-yield placeholders — port exactly; these behaviors are tested in RepositoryTests and must not be "simplified".
- A11y: WCAG AA badge text via ported `ColorMath` (tests enforce 13 tints × both modes ≥ 4.5); every icon-only control gets `setAccessibleName` (D12).
- Anonymity: docs/comments say "the user"; no personal names/paths/emails.
- App data on Linux: `$XDG_DATA_HOME/BoH Librarian/` (default `~/.local/share/BoH Librarian/`); db at `Boh.db` inside it (D7 analog).

## Review Focus

(Implied by the spec but not covered by carried-over tests; each gets a pinning test in the named task.)

1. **Non-ASCII titles in case-insensitive matching** — Swift's `lowercased()` is Unicode-aware; C++ `std::tolower` is not. All folding must go through `QString` (pin: `insertOrReuse` reuses "Néée"/"néée" as one entity; search finds accented titles) — Task 6.
2. **Strict `QJsonDocument` vs the game's lenient JSON** — UTF-16 BOMs, trailing commas, control chars inside strings. The sanitizer pipeline must be string-aware (the macOS remediation-4 bug class). Pin: string containing `", ]"` survives; UTF-16BE file parses — Task 7.
3. **Steam installs outside default paths** — additional libraries, Debian-layout `~/.steam/debian-installation`, Flatpak-Steam. Discovery must scan every `libraryfolders.vdf` entry, not hardcode `~/.steam/steam`. Pin: discovery against a synthetic tree fixture — Task 8.
4. **Old distro SQLite** — floor 3.35. Pin: CI runs the whole suite on ubuntu-22.04 (ships 3.37) — Task 1's CI job.
5. **Duplicate JSON keys in a save file** — behavior must be pinned as last-wins (both Swift `JSONSerialization` and `QJsonDocument` do last-wins; pin so a future parser swap can't change it silently) — Task 7.

---

### Task 1: Project scaffold + CI

**Files:**
- Create: `qt/CMakeLists.txt`, `qt/src/app/main.cpp`, `qt/tests/test_smoke.cpp`
- Modify: `.github/workflows/ci.yml`

Deliverable: an empty main window builds and `ctest` is green, locally and on CI.

- [ ] Write `qt/CMakeLists.txt`: C++20, `find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)`, `find_package(SQLite3 REQUIRED)`, an empty `bohcore` static lib placeholder, `boh-librarian` exe (main.cpp: `QApplication` + empty `QMainWindow` shown), `enable_testing()`.
- [ ] Write `qt/tests/test_smoke.cpp`: Qt Test with one trivially-true test; `add_test` in CMake.
- [ ] Build + run: `cmake -S qt -B qt/build && cmake --build qt/build && ctest --test-dir qt/build`. All green.
- [ ] CI: add a `linux-test` job — ubuntu-22.04 (older glibc/SQLite → compat floor), `apt-get install qt6-base-dev libsqlite3-dev`, build + ctest. Keep the existing macOS job untouched.
- [ ] Commit.

### Task 2: SQLite wrapper (`SQLiteDatabase`, `SQLiteValue`)

**Files:**
- Create: `qt/src/core/SQLiteDatabase.{h,cpp}`, `qt/src/core/SQLiteValue.{h,cpp}`, `qt/tests/test_SQLiteDatabase.cpp`
- Test source of truth: `app/Tests/BoHLibrarianCoreTests/SQLiteDatabaseTests.swift` (128 lines)

Port 1:1 semantics: RAII handles replace `defer` cleanups; `sqlite3_open_v2`, `SQLITE_TRANSIENT` binding, parameterized `bind`/`step`/`column` as in the audited Swift wrapper. In-memory and file dbs, exec, prepare, last_insert_rowid, error type carrying `sqlite3_errmsg`.

- [ ] Write `test_SQLiteDatabase.cpp` porting every Swift test case 1:1 (same names, same assertions).
- [ ] Run: fails (no implementation).
- [ ] Implement `SQLiteDatabase` + `SQLiteValue`. Constructor opens; destructor closes; no copy, move allowed.
- [ ] Run: all wrapper tests green.
- [ ] Commit.

### Task 3: Migrator + embedded migrations

**Files:**
- Create: `qt/src/core/Migrator.{h,cpp}`, `qt/src/core/migrations.qrc`, `qt/tests/test_Migrator.cpp`
- Reference: `app/Sources/BoHLibrarianCore/Migrator.swift`, `app/Tests/.../MigratorTests.swift` (337 lines)

Resolution order (same precedence as Swift): `BOH_MIGRATIONS` env → `./db/migrations` → `../db/migrations` → `:/migrations` Qt-resource copy of `db/migrations` (qrc prefixes the real files — no sync script). Behaviors that MUST survive: each migration is one transaction; `PRAGMA user_version` tracking; refuse to apply to a db newer than the app's max version (loud error); failed apply rolls back cleanly and leaves the connection usable; a drift test asserting the qrc copy equals the repo directory.

- [ ] Write `migrations.qrc` listing `../../db/migrations/*.sql` verbatim; CMake `qt_add_resources` on `bohcore`.
- [ ] Write `test_Migrator.cpp` porting all MigratorTests cases (resolve precedence, seeds present at v9, newer-db refusal, failure-rollback, drift).
- [ ] Run: fails.
- [ ] Implement `Migrator` (read resource via `QFile(":/migrations/…")`).
- [ ] Run: green — a `:memory:` db reaches schema v9 with principles + languages seeded.
- [ ] Commit.

### Task 4: Models, value sets, ReadingMath, ColorMath

**Files:**
- Create: `qt/src/core/Models.{h,cpp}`, `qt/src/core/ReadingMath.{h,cpp}`, `qt/src/core/ColorMath.{h,cpp}`, `qt/tests/test_ReadingMath.cpp`, `qt/tests/test_ColorMath.cpp`
- Reference: `Models.swift`, `ReadingMath.swift`, `ColorMath.swift` + their tests

Mechanical port: enums as `enum class` + `toString/parse` pairs (value sets in `docs/DATABASE.md`); structs with `QString` fields; `std::optional` for optionals. ColorMath: pure math — keep function names; the AA-enforcement test (all 13 tints × light/dark ≥ 4.5) must pass identically.

- [ ] Write `test_ColorMath.cpp` (incl. the ≥4.5 all-tints-both-modes enforcement test).
- [ ] Write `test_ReadingMath.cpp` (helper sentence composition cases).
- [ ] Run: fail. Implement the three files. Run: green. Commit.

### Task 5: Queries (Book/Memory/Skill)

**Files:**
- Create: `qt/src/core/BookQuery.{h,cpp}`, `MemoryQuery.{h,cpp}`, `SkillQuery.{h,cpp}`, `qt/tests/test_BookQuery.cpp`, `test_MemoryQuery.cpp`, `test_SkillQuery.cpp`
- Reference: the three Swift query files + tests (179/111/94 lines)

Pure filter/sort/search logic — port SQL-building and in-memory ranking exactly. **Case folding: `QString::toLower()` everywhere Swift used `lowercased()`** (never `std::tolower`).

- [ ] Port the three test files 1:1.
- [ ] Run: fail. Implement. Run: green. Commit.

### Task 6: Repositories

**Files:**
- Create: `qt/src/core/Repositories/` (Book, Memory, Skill, Journal, Playthrough, Lookup `.h/.cpp`), `qt/tests/test_Repositories.cpp`, `qt/tests/test_PlaythroughRepository.cpp`
- Reference: `app/Sources/BoHLibrarianCore/Repositories/*` + `RepositoryTests.swift` (550) + `PlaythroughRepositoryTests.swift` (101)

Everything must survive the port: write scoping (`WHERE playthrough_id = ? AND id = ?` — fail-closed), `insertOrReuse` (trimmed, case-insensitive, Unicode-aware via QString), `earnedVisibility` predicate, `setSources`/`setYieldingBooks` two-way sync, live-cache grouped queries (`allSourcesByMemory`, `yieldingByMemory`, `entriesByBook`, `lessonSkillAmountsByBook`).

- [ ] Port both test files 1:1, **plus one new pin**: `insertOrReuse` treats `"Cōnfected"` and `"cōnfected"` as the same memory (Review Focus #1).
- [ ] Run: fail. Implement all six repositories. Run: green (all 111-equivalent + 1). Commit.

### Task 7: Save import part 1 — lenient JSON pipeline + scanner

**Files:**
- Create: `qt/src/core/SaveImport.{h,cpp}` (scanner + JSON pipeline half), `qt/tests/test_SaveImport.cpp` (grows through Tasks 7–8)
- Reference: `SaveImport.swift` lines 1–200 (pipeline, `BoHPaths`, scanner), the corresponding `SaveImportTests` cases

Pipeline port: read file (64 MB cap — skip larger, same constant) → sniff BOM, decode UTF-16LE/BE to UTF-8 via `QStringDecoder` → strip control chars → **string-aware** trailing-comma stripper (the state machine, not a regex) → `QJsonDocument::fromJson`. Scanner: list `.json` files newest-first with summaries.

- [ ] Port tests: BOM decode (UTF-16LE + BE), trailing-comma survival, **string containing `", ]"` survives** (remediation-4 class), size-cap skip, unknown contamination warning, **duplicate-key last-wins pin** (Review Focus #2/#5).
- [ ] Run: fail. Implement. Run: green. Commit.

### Task 8: Save import part 2 — importer + Linux path discovery

**Files:**
- Create: rest of `qt/src/core/SaveImport.{h,cpp}` (`SaveImporter`, `ImportReport`, `BoHPaths`), `qt/tests/test_SavePathDiscovery.cpp`
- Reference: `SaveImport.swift` lines 200–704 + rest of the Swift tests; `docs/SAVE_IMPORT.md`

Importer semantics to preserve exactly: fill-empty upsert; `record.phonograph`/`film`/`scroll` kind mapping; contamination `winkwell`/`witchworms`; location stamping with humanized labels + defunct-token guard; unearned auction-lot skip (`unearnedSkipped`); per-playthrough name reuse on collision. New Linux logic in `BoHPaths`:

- [ ] Implement `saveDirectoryCandidates()`: env `BOH_SAVE_DIR` → every Steam library from `libraryfolders.vdf` (parse all libraries; check `~/.steam/steam`, `~/.local/share/Steam`, `~/.steam/debian-installation`, Flatpak `~/.var/app/com.valvesoftware.Steam/...`) → each library's `steamapps/compatdata/1028310/pfx/drive_c/users/steamuser/AppData/LocalLow/Weather Factory/Book of Hours`.
- [ ] Implement `gameElementsDirectoryCandidates()`: env `BOH_GAME_ELEMENTS` → each library's `steamapps/common/Book of Hours/Book of Hours_Data/StreamingAssets/bhcontent/core/elements` (Windows layout under Proton; note the Mac layout differs — this is expected). **Verify against the user's real Proton prefix during acceptance; adjust if Unity put saves elsewhere.** App id 1028310 comes from `docs/SAVE_IMPORT.md`'s recorded userdata path; also scan `appmanifest_*.acf` names as a fallback if probing fails.
- [ ] Port the remaining SaveImportTests (full import fixture, second-playthrough import, kind mapping, 009-class unearned skip, real-game test that skips cleanly when absent).
- [ ] Write `test_SavePathDiscovery.cpp` against a synthetic directory tree fixture (fake libraryfolders.vdf + compatdata) — Review Focus #3.
- [ ] Run: green. Commit.

### Task 9: App shell — window, sidebar, bootstrap, choke point

**Files:**
- Create: `qt/src/app/main.cpp` (extend), `qt/src/app/AppController.{h,cpp}`, `qt/src/app/Views/MainWindow.{h,cpp}`, `Views/Sidebar.{h,cpp}`
- Reference: `App.swift`, `AppState.swift`, `RootView.swift`

`AppController` owns the db + all stores; single `reloadAll()` called after **every** write and on window activation — the structural fix for the empty-state bug class. Bootstrap: `DatabaseLocation` Linux port (`BOH_DB_PATH` → `./Boh.db`/`../Boh.db` → `$XDG_DATA_HOME/BoH Librarian/Boh.db`, mkdir 700, db 600), migrate on launch, status footer shows resolved path + schema version. Sidebar: playthrough switcher combo + 5 sections (`QListWidget` + `QStackedWidget`), shortcuts Ctrl+1…5, Ctrl+R (helper), Ctrl+N (section-aware), Ctrl+F (search focus), Ctrl+Shift+J (quick journal). Switching playthroughs remounts stores (confirmation on delete comes in Task 16).

- [ ] Implement shell; every section initially shows an empty-state placeholder.
- [ ] Manual check: launch with no db → created at XDG path, v9, footer correct; switching sections never loses reload.
- [ ] Commit.

### Task 10: Stores port

**Files:**
- Create: `qt/src/app/Stores/` (Books, Memories, ReadingHelper, Skills, Journal `.h/.cpp`)
- Reference: the five Swift stores

QObject classes holding the same caches as the Swift stores; `reload()` refills **all** dicts (sources/backlinks/entries/lesson-names — the stale-snapshot fix, applied by construction). Signals: `changed()` consumed by views. No per-view local snapshots anywhere except explicit note editors (Task 11/13 keep the guarded re-seed behavior).

- [ ] Implement five stores; compile into the app.
- [ ] Commit.

### Task 11: Books screen

**Files:**
- Create: `qt/src/app/Views/Books/BooksScreen.{h,cpp}`, `BookDetailView.{h,cpp}`, `BookFormView.{h,cpp}`, `qt/src/app/Views/Shared/BadgeDelegate.{h,cpp}`
- Reference: `Books/` Swift files + `docs/GUI_PLAN.md` §Books

`QListView` + delegate rows (title, status, principle badge w/ AA text from ColorMath, language, set); search-as-you-type; filter/sort menus (incl. "Easiest first"); detail pane with yield backlink (clickable → Memories section, earned/mastered-gated), journal entries, lesson names, language-known hint, notes (explicit save-note flow with guarded re-seed). Add/Edit dialog = `QFormLayout` (title + mystery is enough, per the plan doc).

- [ ] Build screen wired to BooksStore.
- [ ] Manual: add a book; search/filter/sort; badge contrast sane in light and dark.
- [ ] Commit.

### Task 12: Record-read flow

**Files:**
- Create: `qt/src/app/Views/Books/RecordReadDialog.{h,cpp}`
- Reference: `MarkAsReadSheet.swift`, `BookReadTransitions` in `BookQuery.swift`

Behaviors that must land exactly: mastering toggle **defaults ON**; "Memory used" lists earned-only; "Memory gained" lists earned-only with the book's unearned imported yield as a **placeholder row** ("Unrevealed memory — this read earns it"); quick-add new memory goes through `insertOrReuse`; form-as-master counts as a read (add/edit with mastered status → counter + "Mastered…" journal entry, never double-counting).

- [ ] Port `BookReadTransitions` (already in Task 5's query file — verify the mastered→mastered no-double-count test exists; add if the Swift suite names it only in UI).
- [ ] Build dialog; wire to BooksStore add/update/setReadStatus paths.
- [ ] Manual: read an unread book with quick-add memory name that collides with an imported hidden memory → reused, read recorded, journal written.
- [ ] Commit.

### Task 13: Memories screen

**Files:**
- Create: `qt/src/app/Views/Memories/` (MemoriesScreen, MemoryDetailView, MemoryFormView), `Views/Shared/AspectEditor.{h,cpp}`
- Reference: `Memories/` Swift files

List = earned-visible only (`allKnown` predicate). Detail pane **display-only** (principle badges, sources, mastered-only yielding-book backlinks — all from store caches). Edit dialog: aspects editor, sources (kind + detail), yielding links — persisted via `setSources`/`setYieldingBooks` on Save. Layout lesson: editors live in plain `QVBoxLayout` blocks, not inside `QFormLayout` rows (the Qt-native equivalent of the macOS grouped-Form lesson; direct widget get/set means the binding-revert class cannot exist).

- [ ] Build screen + dialog.
- [ ] Manual: create → edit aspects/sources/links → save → detail reflects instantly (store-backed caches).
- [ ] Commit.

### Task 14: Reading Helper

**Files:**
- Create: `qt/src/app/Views/ReadingHelper/ReadingHelperScreen.{h,cpp}`
- Reference: `ReadingHelperScreen.swift`, `ReadingMath.swift`

Book picker with filter/mystery/sort menus (`BookFiltering.apply` semantics); earned-only aspect candidates; composed requirement sentence; "memory used" earned-only with placeholder; mastering flow writes the read + journal; reload hook at body level equivalent (controller-level `reloadAll` covers it); "Always yields" backlink clickable.

- [ ] Build screen; wire to ReadingHelperStore.
- [ ] Manual: pick book → candidates filter by principle/level → record mastery → journal + counter update without restart.
- [ ] Commit.

### Task 15: Skills + Journal screens

**Files:**
- Create: `qt/src/app/Views/Skills/` (SkillsScreen, SkillDetailView, SkillFormView), `Views/Journal/` (JournalScreen, JournalEditSheet)
- Reference: the Swift files

Skills: list, detail with Tree of Wisdoms commitment editor (**dirty-flag via onChange-equivalents; Save/Revert reachable** — the macOS remediation-1 bug must not recur). Journal: chronological list, chips navigate to book/skill/memory (masked unrevealed chip carries no action), quick-capture field present **in the empty state too**, Ctrl+Shift+J focuses it, edit dialog's memory picker = earned-only + placeholder for the entry's current link.

- [ ] Build both screens.
- [ ] Manual: journal new-entry on empty journal works; ⌘⇧J→Ctrl+Shift+J works from any section; commitment edit → Save persists.
- [ ] Commit.

### Task 16: Playthroughs manager + import sheet

**Files:**
- Create: `qt/src/app/Views/Playthroughs/` (PlaythroughsDialog, ImportFromSaveDialog)
- Reference: the Swift files

Manager: left-justified rows (the Form-centering lesson is moot in Qt but keep the shape), rename commits on Return **and** on focus-out **and** on dialog close; blank restores stored name; delete gated for the active run with confirmation. Import sheet: scan list (counts + version), destination = new or current playthrough, "Choose folder…" button (day-one picker, Review Focus #3 fallback), summary report incl. `unearnedSkipped`, OK/Cancel.

- [ ] Build both dialogs.
- [ ] Manual: import the user's autosave into a fresh playthrough → 47-era row counts, no 'record' mis-stamps, locations humanized.
- [ ] Commit.

### Task 17: Accessibility + polish pass

**Files:**
- Modify: view files from Tasks 9–16 as needed

- [ ] Every icon-only control: `setAccessibleName` + tooltip (audit list: journal edit/delete, aspect remove, unlink, source remove/add, playthrough delete/activate/menu, save-selection rows, globe, status icons, language-known).
- [ ] Composite rows/badges announce as one element (`QAccessible` events in the delegate).
- [ ] Keyboard-only walk: every flow reachable (tab order, shortcuts, dialogs).
- [ ] Dark/light theme: badges derive AA text via ColorMath (already enforced by tests) — verify visually in both.
- [ ] Commit.

### Task 18: Packaging + docs + CI release

**Files:**
- Create: `qt/packaging/boh-librarian.desktop`, `qt/packaging/make-appimage.sh`, icon files from `scripts/assets/` SVG
- Modify: `.github/workflows/ci.yml`, `README.md`, `docs/GUI_PLAN.md`, `docs/DECISIONS.md`, `docs/ROADMAP.md`, `docs/MANUAL_TEST.md`

- [ ] `.desktop` (Name/Exec/Icon/Categories=Game;Utility) + regenerate icon (the committed SVG → hicolor PNGs; the icns is mac-only).
- [ ] `make-appimage.sh`: linuxdeploy + Qt plugin bundling; runs offline after one-time tool download (cache in CI).
- [ ] CI: tag builds → AppImage artifact attached to the release alongside the mac zip.
- [ ] Docs: D13 (Qt port stack + why Widgets/no QML), D14 (Linux paths + Steam discovery posture, revisit triggers); GUI_PLAN gains an as-built `qt/` section; MANUAL_TEST gains a "Linux" section (the existing checklists are the acceptance script); ROADMAP status.
- [ ] Smoke: AppImage on a clean `~` (fresh db at XDG path) — the double-click-first-run test.
- [ ] Commit.

### Task 19: User data migration (mac → Linux)

**Files:**
- Create: short section in `docs/SAVE_IMPORT.md` or README ("Moving your library")

- [ ] Doc: copy the macOS `Boh.db` to `~/.local/share/BoH Librarian/Boh.db` on the Linux box; launch; the app applies any pending migrations forward-only and shows the version in the footer.
- [ ] Verify with the user's real db copy: version 9, playthrough + row counts match, integrity check passes (`PRAGMA integrity_check`).
- [ ] Commit.

## Effort estimate (honest)

| Slice | Tasks | Experienced Qt dev | Sessions w/ agent pairing |
|---|---|---|---|
| Core port (tests-first) | 2–8 | 2–3 days | 4–6 short sessions |
| App shell + stores | 9–10 | 1 day | 1–2 |
| Screens | 11–16 | 3–4 days | 4–6 |
| A11y + packaging + docs | 17–18 | 1–2 days | 2 |
| Data migration + acceptance | 19 | half day | 1 |

Total: roughly **8–10 working days solo**, or **2–3 calendar weeks of casual pairing**.

## Explicitly out of scope (flagged, not decided)

- Touching the macOS app or its CI — both keep working untouched.
- Flatpak packaging (sandboxing fights the save-import's arbitrary-folder reads, mirroring D11) — revisit if distribution widens.
- Windows build — `qt/` intentionally contains nothing Linux-locked, but no Windows task exists here.
- The `scripts/import-save-preview.py` prototype's known comma-stripper bug — read-only cosmetic, waived on macOS, irrelevant to the C++ port.
