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

## Phase 1 — App scaffold + database layer (next)

Create `app/` exactly per `docs/GUI_PLAN.md`:
- [ ] `Package.swift` (library `BoHLibrarianCore` + executable `BoHLibrarian`, Swift 5 mode,
      macOS 14+), `scripts/sync-migrations.sh`
- [ ] `SQLiteDatabase` wrapper (open/close, `execute`, `query`, binding, errors) +
      `Migrator` (env → repo dir → bundle; sets `foreign_keys=ON`)
- [ ] Models + repositories for Principles, Languages, Books, Memories, Skills, Journal
      (CRUD incl. junctions: MemoryAspects, MemorySources, BookLessons)
- [ ] Tests on `:memory:`: migrations reach v3 and are idempotent; every repository CRUD;
      the two canonical Reading-Helper queries from `docs/DATABASE.md` return correct
      candidates on fixture data
- [ ] App runs (`swift run`) showing an empty shell window with sidebar sections

**DoD:** `swift build` + `swift test` green from a clean checkout; `swift run` opens the
window; no functional UI yet.

## Phase 2 — Books screen

- [ ] Books list (search, status filter, sort), detail, add/edit/delete
- [ ] "Mark as read" sheet incl. quick-add of yielded memory; Journal entry written
- [ ] `docs/MANUAL_TEST.md` started; checklist for this phase ticked

## Phase 3 — Memories screen

- [ ] Memory list/grid with aspect badges (Principles colors), persistent/kind markers
- [ ] Aspect editor (add/remove principle+level), sources editor, book backlinks

## Phase 4 — Reading Helper

- [ ] Book picker → requirement panel (mystery, language-known hint, contamination, kind)
- [ ] Live candidate lists (memories ≥ level; skills with computed contribution)
- [ ] One-flow "log the read" (shared with Phase 2's sheet)

## Phase 5 — Skills & Journal screens

- [ ] Skills CRUD with level stepper (computed 2/1 display), wisdom/element fields
- [ ] Journal timeline, quick-add, entity links/filters

## Phase 6 — Polish

- [ ] `make-app.sh` packaging, menu-bar quick journal, window state persistence
- [ ] Export JSON/CSV; "readable today" dashboard if desired

## Later / optional (only on request)

Rooms tracker, crafting/recipes tracker, visitors/incidents, full wiki import (spoilers!),
multi-playthrough support (second db file).