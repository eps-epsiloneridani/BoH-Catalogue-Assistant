# Roadmap & status

Phases are checkpoints, not prisons — but don't skip the Definition of Done. Update this
file's status markers at the end of every session (see AGENTS.md ground rules).

## Phase 0 — Plan, repo, schema ✅ (2026-10-05)

- [x] Game-mechanics research distilled into `docs/GAME_MECHANICS.md`
- [x] Schema v2 designed (`docs/DATABASE.md`), migrations authored (`db/migrations/001-003`)
- [x] GUI plan (`docs/GUI_PLAN.md`), decisions logged (`docs/DECISIONS.md`)
- [x] `scripts/migrate.sh` + `dump-sql.sh`; git repo initialized; legacy empty schema
      dropped and v2 **applied**; principles (13) and languages (15) seeded
- [x] AGENTS.md written as the durable entry point

## Phase 1 — App scaffold + database layer ✅ (2026-10-05)

Create `app/` exactly per `docs/GUI_PLAN.md`:
- [x] `Package.swift` (library `BoHLibrarianCore` + executable `BoHLibrarian`, Swift 5 mode,
      macOS 14+), `scripts/sync-migrations.sh`
- [x] `SQLiteDatabase` wrapper (open/close, `execute`, `query`, binding, errors) +
      `Migrator` (env → repo dir → bundle; sets `foreign_keys=ON`)
- [x] Models + repositories for Principles, Languages, Books, Memories, Skills, Journal
      (CRUD incl. junctions: MemoryAspects, MemorySources, BookLessons)
- [x] Tests on `:memory:`: migrations reach v3 and are idempotent; every repository CRUD;
      the two canonical Reading-Helper queries from `docs/DATABASE.md` return correct
      candidates on fixture data — **35 tests, all green**
- [x] App runs (`swift run`) showing a shell window with sidebar sections (smoke-tested
      against the real `Boh.db`; it opens the repo db from `app/` cwd via the `../Boh.db`
      fallback and finds schema v3 with nothing pending)

**DoD met:** `swift build` + `swift test` green from a clean checkout; `swift run` opens the
window. No functional UI yet.

## Phase 2 — Books screen ✅ (2026-10-05)

- [x] Books list (search, status filter, sort), detail, add/edit/delete
- [x] "Mark as read" sheet incl. quick-add of yielded memory; Journal entry written
- [x] `docs/MANUAL_TEST.md` started; checklist for this phase ticked — unit-tested
      items pre-ticked; interaction items await the first hands-on play session

Implementation notes: list filtering/sorting/search and journal composition live in
Core as pure functions (tested); UI state in `BooksStore`; the record-read flow is one
transaction. The section enum was renamed `AppSection` to un-shadow `SwiftUI.Section`.

## Phase 3 — Memories screen ✅ (2026-10-05)

- [x] Memory list/grid with aspect badges (Principles colors), persistent/kind markers
- [x] Aspect editor (add/remove principle+level), sources editor, book backlinks

Implementation notes: list implemented as list+detail split (same pattern as Books;
the grid variant can be a later toolbar toggle if wanted). List logic is a pure,
tested function (`MemoryFiltering`). Backlinks support manual link/unlink via
`Books.yielded_memory_id`. **Carried fix:** the Phase 2 routing edit had silently
never applied — Books/Memories screens were both wired up properly this phase
(see MANUAL_TEST note).

## Phase 3½ — Playthroughs + difficulty (user-requested detour) ✅ (2026-10-06)

Requested before Phase 4: books need to record difficulty at catalogue time, and the
app needs playthrough save/load/new (BoH is run-based — findings don't carry over).
- [x] Migration 004: `Books.mystery_level` → `difficulty` (the game community's term;
      wiki book tables: "Mastery Difficulty")
- [x] Difficulty recordable **independently of the mystery principle** (form toggle +
      stepper always available; list/detail show it without a principle)
- [x] Migration 005: `Playthroughs` + `Meta['active_playthrough']` + `playthrough_id`
      scoping on Books/Memories/Skills/Journal, cascade delete
- [x] All repositories scoped to the active playthrough; `PlaythroughRepository` + tests
      (CRUD, Meta round-trip, cross-playthrough isolation, cascade)
- [x] UI: sidebar playthrough switcher (load), New Playthrough sheet (new game — old run
      saved as-is), Manage sheet (rename/load/delete with confirmation); footer shows the
      active run
- [x] 62 tests green; `Boh.db` migrated to v5 (default playthrough seeded + active)

## Phase 4 — Reading Helper ✅ (2026-10-06)

- [x] Book picker → requirement panel (mystery, language-known hint, contamination, kind)
- [x] Live candidate lists (memories ≥ level; skills with computed contribution)
- [x] One-flow "log the read" (shared with Phase 2's sheet — tapping a candidate memory
      prefills it)

Implementation notes: `ReadingHelperStore` composes the canonical queries;
`ReadingMath` (Core, tested) writes the plain-English requirement/reach sentences —
"You need Rose 6. Best recorded: memory 4 + skill 3 = 7 — enough, before souls, inks and
 tools." Near-misses (top 3 below difficulty) show when nothing satisfies. Mastered
books get a yield-focused re-read panel. Souls/inks/tools are explicitly out of scope
until an Elements/Tools table lands.

## Phase 5 — Skills & Journal screens ✅ (2026-10-06)

- [x] Skills CRUD with level stepper (computed 2/1 display), wisdom/element fields
- [x] Journal timeline, quick-add, entity links/filters

Implementation notes: `SkillsStore` + `SkillFormView`/`SkillDetailView` (level stepper
persists immediately; badges show the card's own numbers — level L → L+1 primary /
L secondary, tested as `SkillMath`); list queries are pure/tested (`SkillFiltering`).
`JournalStore` builds day-headed rows (groups whenever the in-game day changes),
quick capture uses the session's current in-game day, entries show link chips
(book/memory/skill), edit sheet supports text + day + all three links, delete is
immediate. ⌘⇧J jumps to the journal and focuses quick-add.

## Phase 6 — Polish (in progress)

- [x] **`make-app.sh` packaging** — ✅ 2026-10-06. Release build →
      `dist/BoH Librarian.app` (Info.plist with bundle id + git-stamped version,
      ad-hoc codesigned). Launched Finder-style in the smoke test: the packaged app
      creates its own db at `~/Library/Application Support/BoH Librarian/Boh.db`
      per D7 and migrates it from the **bundled** migrations — that fallback's first
      real-world proof. Keep in Dock via right-click → Options if wanted.
- [ ] menu-bar quick journal, window state persistence
- [ ] Export JSON/CSV; "readable today" dashboard if desired

## Pending items — user-reported

- [ ] **Package the resource bundle into the .app (portability papercut).**
      Discovered while shipping the kind-repair: `make-app.sh` copies only the bare
      executable, so the packaged app's `Bundle.module` (bohLibrarianCore migrations)
      resolves via the hardcoded SwiftPM *build-directory* fallback — it works only on
      this Mac while `app/.build` exists. A truly portable copy (another Mac, or
      .build cleaned) would fail to find migrations on launch. Fix: copy
      `BoHLibrarian_BoHLibrarianCore.bundle` into the .app's `Contents/Resources`
      and re-sign; verify the smoke test finds migrations with `.build` moved aside.
- [x] **OK/Cancel buttons on the playthrough manager sheet.** ✅ (2026-10-06)
      Done in the save-import UI commit — the manager sheet gained a trailing
      Cancel + OK section (both dismiss; changes apply immediately, as designed).
- [x] **Import from a Book of Hours save.** ✅ (2026-10-06) Implemented end-to-end:
      `SaveImporter` (Core) + "Import from Save…" in the Manage Playthroughs sheet,
      which lists save games from the standard install path (`~/Library/Application
      Support/Weather Factory/Book of Hours`) with book/skill counts and game version,
      then imports into a new or the current playthrough. 84 tests green incl. a
      real-game import test against the live save. Full documentation:
      `docs/SAVE_IMPORT.md`.
- [x] **Journal new-entry was a dead end on an empty journal.** ✅ (2026-10-06)
      The empty state's "Note today's finding" targeted a quick-add field that
      only rendered once entries existed (⌘⇧J hit the same wall — its requestFocus
      flag was consumed unhandled and stuck true, disabling later jumps). The
      quick-capture box now renders in the empty branch too; reload + focus-request
      handling sits on the body's onAppear so it covers both branches and entries
      written by record-read still appear on arrival.
- [x] **Save import into a second playthrough failed:** `UNIQUE constraint failed:
      Skills.name`. ✅ (2026-10-06) 001-era table-global `UNIQUE` on `Skills.name`
      (and on `Memories (name, kind)`) survived 005's playthrough scoping, so a
      second playthrough couldn't hold the same entity names — importing the
      autosave into "Playthrough 2" died on the first skill. `006_per_playthrough_unique_names`
      rebuilds both tables (data-preserving; verified against the live db: rows, ids,
      timestamps and FK integrity intact) and moves uniqueness to
      (name, playthrough_id) / (name, kind, playthrough_id); duplicates within one
      playthrough are still blocked. Regression tests: import into a second
      playthrough with identical names + a data-bearing v5→v6 rebuild.
      86 tests green.
- [x] **Imported books were stamped 'record':** ✅ (2026-10-06) the save importer's
      kind map read the tomes.json aspect `codex` as "phonograph record"; `codex`
      is in fact the plain bound-book format (256/281 tomes), so 41 of 42 books per
      imported playthrough entered as 'record' — it *looked* like a cascade changing
      the kind of other entries after manual entry, but the journal/updated_at trail
      shows every affected row kept `updated_at = created_at` (the import stamp):
      the rows were born mis-stamped, nothing rewrote them. Fix: the mapping now keys
      off `record.phonograph` (films `film`, scrolls `scroll`, codex/tablet → book),
      and migration 007 repairs the data — flips only 'record' rows carrying the two
      real import fingerprints (`created_at` 12:58:45 / 13:19:42), leaving manual
      entries (both genuine phonograph records) and scrolls untouched. Verified on a
      copy of the live Application Support db: 82 flips, scroll/manual rows intact,
      integrity + FK checks clean. Tests: fixture mapping (codex/book, record,
      scroll), 007 unit proof, live-import kind regression. 87 tests green.
      **Verified by the user** (relaunch applied 007; MANUAL_TEST items ticked).
- [x] **Memory backlinks mastered-only (spoiler posture).** ✅ (2026-10-06, user
      request) "Books that yield this" on a memory displayed every book carrying a
      `yielded_memory_id` link — including recorded-but-unread books whose yields the
      import had already stamped. Now the display covers only books the player has
      mastered; the data link is untouched and the backlink appears at mastery
      (test drives both directions). "Link a book…" unchanged. 87 tests green.
- [x] **Accessibility pass.** ✅ (2026-10-06) Assessment found native controls solid
      (semantic fonts, no motion, nothing color-only) but zero a11y modifiers anywhere
      and — measured — **all 13 principle badges failing WCAG AA in at least one mode**
      (tint-as-text). Fixed: `ColorMath` in Core (WCAG luminance/contrast/blend +
      `readableTextHex`; derived per-mode badge text, fill/border keep the seeded tint;
      5 tests incl. all-tints-both-modes ≥ 4.5:1), badges/rows announce as single
      crafted elements, every icon-only control labelled or hidden (28 sites). D12
      records the posture; MANUAL_TEST gains a VoiceOver/Dynamic-Type checklist.
      92 tests green.
- [x] **Earned-only memory list (spoiler posture, part 2).** ✅ (2026-10-06, user
      request) the import seeds a memory per imported book's yield, so the
      Memories list revealed names/traits of unearned memories.
      `MemoryRepository.allKnown()` filters the list (and footer count) to
      hand-created memories and those with a mastered yielding book; the table
      itself keeps everything (pickers/dedupe need it). 93 tests green.
- [x] **Earned-only reading helper (spoiler posture, part 3).** ✅ (2026-10-06)
      Same earned-visibility predicate (shared `MemoryRepository.earnedVisibility`)
      now drives the Helper's aspect candidates — unearned memories no longer show
      as usable "could satisfy" candidates. The Helper's 'Always yields' panel was
      already mastered-gated (correction: part 2's flag overstated it — the real
      unconditional reveal was the Books detail's Yields panel, now gated: unmastered
      books show "revealed by mastering the book"). Record-read sheet: "Memory used"
      picker is earned-only, but "memory gained (existing)" keeps the full table —
      selecting a not-yet-earned imported yield is exactly the act that earns it.
      94 tests green.
- [x] **Quick-add memory collided with hidden imported yields.** ✅ (2026-10-06,
      user checklist report) the record-read sheet's "New memory…" insert hit 006's
      per-playthrough UNIQUE (name, kind) whenever the name already existed as an
      imported (unearned, hidden) yield — createMemory returned nil and the sheet
      aborted the *entire* read (no journal entry proves it on the trail).
      MemoryRepository.insertOrReuse treats the same (name, kind, case-sensitive-
      insensitive) as the same game entity: reuse earns it via the read. Tests
      cover reuse + kind-differentiated inserts. 95 tests green.

## Later / optional (only on request)

- **File picker for save import.** The importer reads the standard save path only;
  choosing an arbitrary save file (e.g. from another machine or a Steam Cloud
  restore) needs an NSOpenPanel step in `ImportFromSaveSheet` (acknowledged and
  parked at the user's request, 2026-10-06 — the sheet's footer says so in-app).

Rooms tracker, crafting/recipes tracker, visitors/incidents, full wiki import (spoilers!),
multi-playthrough support (second db file).