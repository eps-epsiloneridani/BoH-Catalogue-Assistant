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

## Phase 6 — Polish (next)

- [ ] `make-app.sh` packaging, menu-bar quick journal, window state persistence
- [ ] Export JSON/CSV; "readable today" dashboard if desired

## Pending items — user-reported (do before or with Phase 6)

- [ ] **OK/Cancel buttons on the playthrough manager sheet.** The Manage Playthroughs
      sheet (`app/Sources/BoHLibrarian/Views/Playthroughs/PlaythroughViews.swift` →
      `ManagePlaythroughsSheet`) currently has no dismiss buttons of its own — only the
      sheet's window chrome. Add a trailing footer Section with Cancel + OK, matching
      the pattern in `BookFormView`/`SkillFormView`/`JournalEditSheet`. Note: renames
      commit immediately (on return-key submit) and delete/load act at once, so both
      buttons simply dismiss — they're affordances, not save states. (Reported 2026-10-06
      by the user while parking the project; he may pick this up himself.)
- [ ] **Import from a Book of Hours save — FEASIBLE, awaiting green light.** the user asked
      (2026-10-06) whether the db can be populated from his Steam save. Investigated
      and proven: the save (`~/Library/Application Support/Weather Factory/Book of
      Hours/AUTOSAVE.json`) is plaintext JSON holding books (read state, contamination),
      skills (levels, Tree commitments) — and the game's own element files
      (`…/StreamingAssets/bhcontent/core/elements/*.json`) hold titles, mystery+
      difficulty, languages, lessons and each book's yielded memory. Full findings +
      import design: `docs/SAVE_IMPORT.md`; working read-only prototype:
      `scripts/import-save-preview.py` (verified against the user's live save — 42 books,
      27 skill stacks decoded). Implementation: extend the prototype into an importer
      per the SAVE_IMPORT design; never overwrite user data; commit Boh.db before/after.

## Later / optional (only on request)

Rooms tracker, crafting/recipes tracker, visitors/incidents, full wiki import (spoilers!),
multi-playthrough support (second db file).