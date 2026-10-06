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

- **2026-10-06 — PARKED (Phases 0–5 + packaging) with user-requested additions shipped.**
  Build + 87 tests pass from a clean checkout; `Boh.db` at schema v7 with an empty
  "First playthrough" loaded; tree clean. All roadmap screens live (Books, Memories,
  Reading Helper, Skills, Journal) plus **playthroughs**, **difficulty**, **save
  import** (Manage Playthroughs → "Import from Save…"; see `docs/SAVE_IMPORT.md`),
  and a **packaged app**: `scripts/make-app.sh` → `dist/BoH Librarian.app`, smoke-
  tested Finder-style (bundled-migrations fallback proven). The packaged app keeps
  its own db in `~/Library/Application Support/BoH Librarian/` (D7) — the user's
  live copy is data-bearing (2 imported playthroughs + manual entries), migrated to
  v7 by the user's 2026-10-06 relaunch (verified in-db; MANUAL_TEST items ticked).
  Bug triage pass 1 (2026-10-06, same day): two user-reported issues fixed — the
  journal's new-entry button was dead on an empty journal (c4cbfae), and save import
  into a second playthrough hit a 001-era table-global `Skills.name` UNIQUE
  (769a1cd: migration 006 rebuilds Skills + Memories to per-playthrough unique
  names; repo db migrated to v6). Manual verification of both fixes is pending —
  see the unticked items in `docs/MANUAL_TEST.md`.

- **How to resume (in this order):**
  1. The user's hands-on pass surfaced the two bugs fixed above. Walk the
     currently-unticked items in `docs/MANUAL_TEST.md` together (the two fixes have
     dedicated verification items) and fix any papercuts found.
  2. Then the rest of Phase 6 per `docs/ROADMAP.md`: menu-bar quick journal,
     window-state persistence, JSON/CSV export.
  3. Later/optional ideas live at the bottom of the ROADMAP (save-import file picker,
     rooms tracker, recipes, visitors, opt-in wiki import, per-file playthroughs).

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
- 2026-10-06 (Phase 5): Skills + Journal screens (78 tests); ⌘⇧J quick-journal command.
- 2026-10-06 (parked): hand-off docs refreshed — as-built repo layout in AGENTS.md and
  GUI_PLAN.md, parked status with resume order recorded above.
- 2026-10-06 (final note): the user asked for OK/Cancel buttons on the playthrough
  manager sheet — recorded as a pending item in `docs/ROADMAP.md` §Pending items
  (they may do it themselves); not implemented.
- 2026-10-06 (post-park investigation): the user asked whether the db can be
  populated from their Steam save. **Answer: yes, fully feasible** — save and game
  data are plaintext JSON; decoded their live save end-to-end (42 books with read
  state/contamination, skills with levels, Tree commitments). Recorded in
  `docs/SAVE_IMPORT.md` + a second ROADMAP pending item; working read-only prototype
  at `scripts/import-save-preview.py`. Toolchain note for parsers of game files:
  mixed UTF-16/UTF-8 by BOM, lenient JSON (trailing commas, control chars) — see
  the prototype's `load()`.
- 2026-10-06 (save import SHIPPED, user-requested): both pending items closed.
  Core: `SaveImport.swift` (lenient JSON, scanner, `SaveImporter` with fill-empty
  upsert; `winkwell`/`witchworms` contamination cases; 84 tests incl. real-game
  import into an in-memory db). UI: "Import from Save…" in Manage Playthroughs
  (standard-path save list w/ counts+version, destination = new or current playthrough,
  summary alert) + OK/Cancel buttons on that sheet. File-picker extension parked in
  ROADMAP §Later. Also fixed: MANUAL_TEST phase order (Phase 3 had been stranded
  at the file's end since its insertion).
- 2026-10-06 (packaging, user-requested): `scripts/make-app.sh` → release build →
  `dist/BoH Librarian.app` (bundle id, git-stamped version, ad-hoc codesign).
  Smoke-tested via `open`: first launch creates the app's own db in
  `~/Library/Application Support/BoH Librarian/` at schema v5 **from the bundled
  migrations** — that fallback's first real-world proof. `dist/` gitignored;
  the smoke test's db was removed so the first double-click is a pristine run.
- 2026-10-06 (user-reported bug #1, journal): the journal's new-entry button did
  nothing on an empty journal — hit in the user's first minutes on the packaged
  app (pristine db, before save import had landed an entry). Root cause: the
  quick-capture field rendered only in the non-empty branch, so the empty state's
  "Note today's finding" focused a field that wasn't there; ⌘⇧J died the same way
  and its requestFocus flag stuck true, poisoning later jumps. Fixed: quick-capture
  renders in the empty branch; reload + focus-request handling on the body's
  onAppear (also makes first entries written by record-read show up on arrival with
  an empty cached journal); dropped that button's default-action shortcut (Return
  is the field's onSubmit; a same-tick double fire risks a double insert). 84 tests
  green; dist rebuilt for hand verification.
- 2026-10-06 (user-reported bug #2, import): importing the autosave into a second
  playthrough failed — `UNIQUE constraint failed: Skills.name`. Root cause: 001-era
  table-global uniques (`Skills.name`, `Memories (name, kind)`) survived 005's
  playthrough scoping. Migration 006 rebuilds both tables to per-playthrough
  uniqueness — data-preserving, proven on a copy of the user's live Application Support
  db (rows/ids/timestamps/FK integrity verified; the previously-failing insert now
  succeeds; same-playthrough duplicates still rejected). Repo `Boh.db` migrated to
  v6; 86 tests green (86 = 84 + import-into-second-playthrough + v5→v6 rebuild).
- 2026-10-06 (user-reported bug #3, imported kinds): every imported book showed an
  "Record" badge — 41 of 42 books per imported playthrough carried `book_kind =
  'record'`, and it looked to the user like a cascade update fired by manual entry
  (their last four entries happened to be Books). Db forensics settled it: all
  affected rows kept `updated_at = created_at` (the import stamps 12:58:45 and
  13:19:42), no triggers/views exist, and an hour-earlier playthrough imported the
  same mis-stamps — the rows were **born** as 'record' during the save import:
  `SaveImporter.bookKind(in:)` mapped the tomes.json aspect `codex` to `.record`.
  In the game data `codex` is the plain bound-book format (256/281 tomes); vinyl
  records carry `record.phonograph`. The SAVE_IMPORT.md investigation note that
  said "records appear as codex-aspected Books" was the origin of the misread —
  corrected in the doc. Fix: mapping keys off `record.phonograph` (films `film`,
  scrolls `scroll`, codex/tablet → book); migration 007 flips only 'record' rows
  with the two real import fingerprints (verified first on a copy of the live
  Application Support db: 82 flips; scrolls Tantras intact; the user's two genuine
  phonograph records untouched; integrity/FK clean; user's db applies it on next
  launch); dist rebuilt with the synced migration. 87 tests green (87 = 86 +
  mapping-coverage fixture + 007 data-repair proof).
- 2026-10-06 (security review, user request): full pass over the published repo's
  attack surfaces. Verified clean: uniform parameterized SQL (only two interpolations,
  both constant/Int), SQLITE_TRANSIENT binding, no network/process-exec code, no
  dependencies, script hygiene (pipefail, quoting, -init /dev/null), lenient-JSON
  state machine, pseudonymous authors/emails in all history, no player data in the
  *repo* db. Fixups shipped: save-scanner size cap (64 MB — a stray huge .json in the
  save dir can no longer freeze the manager sheet; tested both ways), 700/600 perms
  on the app's Application Support dir + db, and D11 records the deliberate posture
  (no sandbox/hardened runtime, ad-hoc signing, env-var dev knobs) with revisit
  triggers. **History issue surfaced: the pre-anonymize commits carry the user’s given
  name (AGENTS/ROADMAP/SAVE_IMPORT.md, test comments) and a hardcoded /Users/…
  home path (import-save-preview.py) — HEAD is clean but git history is not;
  history rewrite scheduled in the same session.**
- 2026-10-06 (007 verified + memory backlinks mastered-only, user request): the
  user's relaunched app applied 007 (live db at v7; 82 mis-stamped rows now 'book',
  both scrolls and the two genuine phonograph records intact; MANUAL_TEST items
  ticked). Policy refinement of the spoiler posture (D6's spirit): "Books that yield
  this" on a memory now displays only books the player has *mastered* — import and
  record-read still stamp `yielded_memory_id` on unread books (the data keeps the
  link, backlinks just don't reveal a yield the player hasn't earned). The link
  becomes visible at mastery (both ways tested). "Link a book…" still offers all
  books; note for the user: a manual link to a not-yet-mastered book stays hidden
  until they master it.
- 2026-10-06 (anonymization + cold-start consolidation, user-requested): all docs,
  logs, code comments, test comments and the save-preview script scrubbed of
  personal name/home-path references ("the user" throughout; hardcoded /Users/…
  path replaced with expanduser). New ground rule records the convention; commit
  identities already pseudonymous (username + noreply email).

## Repository layout

```
AGENTS.md            <- you are here (entry point)
README.md            <- short human-facing overview
Boh.db               <- THE DATA (SQLite, schema v7). Committed to git on purpose (D1).
db/migrations/       <- numbered SQL migrations (canonical schema source; 001–007 applied)
docs/
  DATABASE.md        <- schema design (through v6), value sets, canonical queries, future tables
  GUI_PLAN.md        <- app architecture, as-built layout, screens, stack decisions, build/run/test
  GAME_MECHANICS.md  <- distilled Book of Hours facts that drive the schema + sources
  ROADMAP.md         <- phases, definitions of done, current status
  DECISIONS.md       <- short log of key decisions (D1, D2, …) with rationale
  MANUAL_TEST.md     <- per-phase hands-on checklists for the running app
  SAVE_IMPORT.md     <- feasibility + design for importing from a BoH save (confirmed)
scripts/
  migrate.sh         <- apply pending migrations to Boh.db (--status to inspect)
  dump-sql.sh        <- write a text .sql snapshot of the db into snapshots/
  sync-migrations.sh <- copy db/migrations into the app bundle resources (after editing them)
app/                 <- the Swift package (see docs/GUI_PLAN.md for the as-built map)
  Sources/BoHLibrarianCore/    <- testable core: SQLite wrapper, Migrator, models,
                                 repositories, pure query/math helpers, bundled migrations
  Sources/BoHLibrarian/        <- SwiftUI app: AppState (playthroughs, store wiring),
                                 RootView (routing), Stores/ (per-screen state), Views/
  Tests/BoHLibrarianCoreTests/ <- 87 tests on :memory: databases
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
| Run the app (dev) | `cd app && swift run` |
| Package a double-clickable .app | `scripts/make-app.sh` → `dist/BoH Librarian.app` |
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
- **Anonymity (repo is published):** docs, logs and code comments refer to "the user"
  — no personal names, no home-directory paths with usernames, no personal emails.
  Commits keep the pseudonymous noreply identity (see `git config user.name/email`).

## Document index

1. `docs/GAME_MECHANICS.md` — how the game actually works (why the schema looks like this).
2. `docs/DATABASE.md` — the data model.
3. `docs/GUI_PLAN.md` — the app.
4. `docs/ROADMAP.md` — where we are and what's next.
5. `docs/DECISIONS.md` — why things are the way they are.
6. `docs/MANUAL_TEST.md` — what to check by hand after UI work.