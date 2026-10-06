# AGENTS.md — BoH Librarian

Entry point for AI agents (and humans) working in this repo. **Read this file first**, then
`docs/ROADMAP.md` for current status and next steps. Keep this file up to date — any session
that changes the plan, schema, or code must update the docs and the Status snapshot below.

## What this project is

A **native macOS GUI app** for a single user to **record findings while playing
*Book of Hours*** (Weather Factory, 2023 — the occult-librarian game set in Hush House).
The data store is the SQLite database at the repo root: **`Boh.db`**.

Two goals, in priority order:

1. **Fast capture of discoveries** — books found and read, memories obtained, skills learned.
   This replaces the spreadsheet players normally keep ("note down the memory you got from
   each book").
2. **The Reading Helper** — pick a book, see its mystery (principle + level), language and
   contamination; the app lists which recorded memories and skills could satisfy the reading
   requirement; one click logs the read and the memory gained.

## Ground rules (non-negotiables)

- **`Boh.db` is user data.** Never make a destructive change without a git commit immediately
  before. Migrations are **forward-only** and must preserve data.
- **Docs first.** Schema or plan changes update `docs/` in the same commit.
- **No external package dependencies.** Build must work offline. SQLite is accessed through
  the system `SQLite3` module via a small hand-written wrapper (see `docs/DECISIONS.md` D2).
- **No spoilers in seeds.** Only structural reference data is pre-seeded (principles,
  languages). Books, skills and memories are recorded by the player as they find them (D6).
- **End of session:** update `docs/ROADMAP.md` status, update the Status snapshot here,
  commit everything (`Boh.db` included), and summarize state for the next session.

## Status snapshot (update every session)

- **2026-10-06 — Phases 0–4 complete.** The Reading Helper is live: pick the book in
  hand → see the requirement (difficulty, language, contamination, equipment), the
  plain-English reach line, satisfying/near-miss memories and skill contributions → tap
  a memory to record the read with it preselected. 71 tests green. **Next: Phase 5** —
  Skills & Journal screens per `docs/ROADMAP.md`.

### Session log
- 2026-10-05 (planning): docs, migrations, seeds, git init.
- 2026-10-05 (Phase 1): scaffold, SQLite layer, migrator, models, repositories, app shell;
  35 tests.
- 2026-10-05 (Phase 2): Books screen + record-read flow; filtering/journal logic in Core
  (48 tests); `docs/MANUAL_TEST.md` started.
- 2026-10-05 (Phase 3): Memories screen (58 tests); shared `AspectEditor`; **fixed**: the
  Phase 2 routing edit had never applied, so Books screen was unreachable until now —
  routing now grep-verified after every wiring change.
- 2026-10-06 (Phase 3½, user-requested): migrations 004 (difficulty) + 005 (playthroughs);
  scoped repositories; playthrough switcher/New/Manage UI (62 tests). The verify-wiring
  grep caught a second silently-dropped paired edit (sidebar switcher) before it could
  ship unreachable — the convention works.
- 2026-10-06 (Phase 4): Reading Helper (71 tests). Toolchain note: on this SDK
  `capitalized` is a property — `capitalized()` doesn't compile (bitten once, fixed).
- No playthrough data recorded in `Boh.db` yet.

## Repository layout

```
AGENTS.md            <- you are here (entry point)
README.md            <- short human-facing overview
Boh.db               <- THE DATA (SQLite). Committed to git on purpose (D1).
db/migrations/       <- numbered SQL migrations (canonical schema source)
docs/
  DATABASE.md        <- schema v2 design, value sets, canonical queries, future tables
  GUI_PLAN.md        <- app architecture, screens, stack decisions, build/run/test
  GAME_MECHANICS.md  <- distilled Book of Hours facts that drive the schema + sources
  ROADMAP.md         <- phases, definitions of done, current status
  DECISIONS.md       <- short log of key decisions (D1, D2, ...) with rationale
  MANUAL_TEST.md     <- per-phase hands-on checklists for the running app
scripts/
  migrate.sh         <- apply pending migrations to Boh.db (--status to inspect)
  dump-sql.sh        <- write a text .sql snapshot of the db into snapshots/
app/                 <- Swift package (created in Phase 1; see docs/GUI_PLAN.md)
```

## Everyday commands

| Task | Command |
|---|---|
| Apply pending migrations | `scripts/migrate.sh` |
| Migration status | `scripts/migrate.sh --status` |
| Inspect the database | `sqlite3 Boh.db` (`.tables`, `.schema Books`, `PRAGMA user_version;`) |
| Text snapshot of the db | `scripts/dump-sql.sh` (output gitignored; for eyeballing diffs) |
| Sync bundled migrations (after editing `db/migrations/`) | `scripts/sync-migrations.sh` |
| Build the app | `cd app && swift build` |
| Run the app | `cd app && swift run` |
| Run tests | `cd app && swift test` |
| Commit play-session data | `git add Boh.db && git commit -m "data: <what you recorded>"` |

## Conventions

- **Migrations:** `db/migrations/NNN_short_name.sql`, applied in numeric order. Each file is
  a complete transaction and sets its own `PRAGMA user_version`. Never edit an applied
  migration; write a new one. The app runs migrations on launch from the same files
  (resolution order: `BOH_MIGRATIONS` env var → `./db/migrations` → bundled copy).
- **Naming:** SQL uses `snake_case`; Swift models use the same concepts in `PascalCase`.
  Game terms keep their exact in-game spelling ("Killasimi", not "Killasami").
- **Timestamps:** TEXT, UTC, via `datetime('now')`. `updated_at` is maintained by the app,
  not triggers.
- **Enums:** stored as TEXT, documented value sets in `docs/DATABASE.md`; the GUI constrains
  entry (SQLite CHECKs deliberately minimal so value sets can grow without table rebuilds).
- **Commits:** small, described imperatively. Always include doc updates with the change they
  describe. Commit `Boh.db` freely — that's the versioning story for playthrough data.
- **Verify wiring, not just builds:** after routing a screen or connecting a flow, grep the
  call site and smoke-run — Phase 2 shipped a fully-tested but *unreachable* screen for
  exactly one session because an edit call failed silently and the build still passed.
- **Game knowledge:** `docs/GAME_MECHANICS.md` is the distilled reference (with sources and
  open questions). If play reveals a mechanic differently than documented, fix the doc first,
  then adjust schema/UI to match.

## Document index

1. `docs/GAME_MECHANICS.md` — how the game actually works (why the schema looks like this).
2. `docs/DATABASE.md` — the data model.
3. `docs/GUI_PLAN.md` — the app.
4. `docs/ROADMAP.md` — where we are and what's next.
5. `docs/DECISIONS.md` — why things are the way they are.
6. `docs/MANUAL_TEST.md` — what to check by hand after UI work.