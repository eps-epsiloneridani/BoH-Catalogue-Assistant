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
  Build + 101 tests pass from a clean checkout; `Boh.db` at schema v7 with an empty
  "First playthrough" loaded; tree clean. All roadmap screens live (Books, Memories,
  Reading Helper, Skills, Journal) plus **playthroughs**, **difficulty**, **save
  import** (Manage Playthroughs → "Import from Save…"; see `docs/SAVE_IMPORT.md`),
  and a **packaged app**: `scripts/make-app.sh` → `dist/BoH Librarian.app`, smoke-
  tested Finder-style (bundled-migrations fallback proven). The packaged app keeps
  its own db in `~/Library/Application Support/BoH Librarian/` (D7) — the user's
  live copy is data-bearing (2 imported playthroughs + manual entries), migrated to
  v7 by the user's 2026-10-06 relaunch (verified in-db; MANUAL_TEST items ticked).
  Bug triage pass 1 (2026-10-06, same day): two user-reported issues fixed — the
  journal's new-entry button was dead on an empty journal (ab29a31), and save import
  into a second playthrough hit a 001-era table-global `Skills.name` UNIQUE
  (7423cfe: migration 006 rebuilds Skills + Memories to per-playthrough unique
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
  history rewritten 2026-10-06 (filter-branch, 1:1 across all 36 commits; doc
  hash references remapped; backup bundle + refs/original purged after).**

- 2026-10-06 (accessibility pass, user request): assessment + fixes. Assessment:
  semantic fonts/motion-free/native controls = solid; zero a11y modifiers anywhere;
  computed WCAG contrast — **all 13 principle badges fail AA in ≥1 mode** (tint-as-text;
  measured, e.g. Lantern 1.21:1 light, Edge 1.43:1 dark). Fix: Core `ColorMath`
  (luminance/contrast/blend + `readableTextHex` deriving per-mode badge text; 5 tests
  incl. all-tints-both-modes ≥4.5 enforcement); badges derive text (fill/border keep
  the tint identity), whole book rows + badges announce as one crafted element; every
  icon-only control labelled / decorations hidden (28 sites catalogued: journal
  edit/delete, aspect remove, unlink, source remove/add, playthrough delete/active/
  menu, save-selection, globe, status icons, language-known). D12 records the posture
  (AA target; labels required on icon controls; revisit triggers). VoiceOver/Dynamic
  Type hands-on checklist added to MANUAL_TEST; docs updated. 92 tests green.

- 2026-10-06 (earned-only memory list, user request): the save import creates a
  memory (with aspects) for every imported book's yield — so the Memories list
  showed names/traits the player hadn't earned. Now `MemoryRepository.allKnown()`
  drives the list (and the footer count through the store): visible = no yield
  links (hand-created) OR ≥1 mastered yielding book; `all()` stays all-inclusive
  for record-read pickers and import dedupe. Visibility flips at mastery (tested
  all branches). **Deliberately NOT changed (flagged): ReadingHelper still lists
  unearned memories as aspect-candidates and reveals 'Always yields' names for
  unmastered imported books — same leak class, different screen, awaiting
  direction.** 93 tests green.

- 2026-10-06 (rewrite published): the scrubbed history went to origin via GitHub
  Desktop's force push (fetch first → Repository → Force push; Desktop pushes with
  force-with-lease internally; verified locally main == origin/main). The user
  accepts GitHub-side retention of unreachable old commits — no further action on
  the name/home-path exposure; ordinary pushes from here on.

- 2026-10-06 (earned-only reading helper, user request): same earned-visibility
  predicate now shared (`MemoryRepository.earnedVisibility`) and applied to the
  Helper's aspect candidates; Books detail's Yields panel gated to mastered books
  ("revealed by mastering the book") — correction: part 2's flag overstated the
  Helper leak (its 'Always yields' panel was already mastered-gated); the real
  unconditional reveal was Books detail. Record-read sheet split: "Memory used"
  earned-only, "memory gained (existing)" keeps the full table (selecting an
  imported yield is the earning act — also avoids a UNIQUE-crash path where the
  hidden memory couldn't be picked as existing and a re-add would collide with
  006's per-playthrough unique names). 94 tests green.

- 2026-10-06 (quick-add memory collision, user checklist report): "creating a new
  memory from a read doesn't populate the memory table" on Imported from AUTOSAVE.
  Db forensics: the earning mechanism worked (the successful case was on 'manual
  debug': memory 'Persistent' linked to mastered book 97, visible). On AUTOSAVE the
  quick-add's typed name collided with the import's hidden (unearned) memories —
  the insert hit 006's UNIQUE (name, kind, playthrough_id), createMemory returned
  nil, and the sheet's early return aborted the ENTIRE read (trail: no post-14:05
  journal entries there). Fix: MemoryRepository.insertOrReuse — same (name, kind,
  case-insensitive) is the same game entity: reuse, and the read earns it; a
  different kind inserts. 95 tests green.

- 2026-10-06 (memory detail pane is display-only, user manual-test report): the
  pane embedded the full inline AspectEditor (pickers/steppers/remove rows) that
  looked editable without a save; its per-change persistence path existed but the
  affordance was wrong for a record view. The detail pane now renders aspects as
  PrincipleBadges (read-only, AA text + combined VO semantics for free); editing is
  the Edit… sheet's job (MemoryFormView's AspectEditor + save-path verified);
  the dead inline-editor state + MemoriesStore.setAspects removed. 96 tests green.
- 2026-10-06 (helper filter/sort, user feature): the Reading Helper picker adopts
  the Books screen's tested vocabulary wholesale — BookQueryOptions gains a mystery-
  principle filter (books without a recorded principle excluded while filtered) and
  BookSort gains "Easiest first" (ascending difficulty, unknown last; the Books menu
  offers it too). Helper toolbar: Filter (read status) / Mystery / Sort menus; the
  helper's displayed list is now BookFiltering.apply(options…) — search widens to
  notes/mystery-name/language (Books semantics); unread-first remains the default
  via BookSort.status; dead statusRank removed. 101 tests green (+2: mystery filter,
  easiest-first order).
- 2026-10-06 (doc-bump footgun, twice bitten): README was silently emptied
  again — the inline `open(path, "w").write(open(path).read()...)` pattern
  truncates on the write-open before the read evaluates, and commits as empty
  (twice now: 474de5b-era fixed at 05ad56f, re-broken by 1f590dd-era bumps;
  count strings also drifted via assert-less replaces). RULE GOING FORWARD:
  doc-bump scripts always read-then-write in two steps with hardcoded asserts
  on the exact current strings before any write; never inline write(read()).
- 2026-10-06 (manual-test Phase 4 cleared, user request): all Reading Helper
  hands-on items ticked in the user's pass; wording synced with this session's
  as-built truth (earned-only candidates, earned-only "Memory used" picker,
  AA badge text, labelled hint icons).
- 2026-10-06 ("can't rename the active playthrough"?, user question): answer —
  deleting the active/only run is the only deliberate gate (mounted stores);
  renaming the active run was never gated and persists identically. The observed
  non-stick was the click-OK path: focus stays on the TextField, so blur-commit
  never fired before dismissal. Fixed with a third commit path — rows commit on
  disappear (sheet close/unmount), unchanged names no-op. 99 tests green.
- 2026-10-06 (manage sheet: names left + renames stick, user bug): two issues on
  ManagePlaythroughsSheet — rows were centered by the grouped Form's column, and
  renames only committed via onSubmit (Return): typing then clicking OK/elsewhere
  never called renamePlaythrough (which itself persists + refreshes fine — the app
  layer was innocent). Same grouped-Form lesson again: the sheet rebuilt as a plain
  left-justified layout (ScrollView; import button + footers as text rows; the Form
  gone entirely); PlaythroughRow commits on blur via @FocusState.onChange AND on
  Return; blank edits restore the stored name (double-commit guarded by the
  unchanged check). 99 tests green.
- 2026-10-06 (manual-test Phases 2 & 3 cleared, user request): the Books and
  Memories "needs hands" checklists ticked in one pass per the user's hands-on
  session; three items' wording updated to as-built truth at the same time
  (difficulty recorded via toggle+stepper without a principle per D10; the detail
  pane's aspects/sources/links are read-only with editing in Edit…). Counters
  cited from the live db (book 97 times_read = 2; memory "Persistent" via
  quick-add reuse).
- 2026-10-06 (book pane applied the same live-cache fix, on request): the
  latent quick-note staleness fixed — BookDetailView's four auxiliary snapshots
  (yield name, journal entries, lesson names, language-known) now read live from
  BooksStore dicts refilled on every reload; new Core queries:
  JournalRepository.entriesByBook (newest-first per book) and
  BookRepository.lessonSkillAmountsByBook (names resolved via the Skills join,
  scoped by the Books join). Only notes stay pane-local (seeded per book id).
  99 tests green.
- 2026-10-06 (stale display after save, user bug): edits made in the memory
  edit sheet didn't show in the detail pane until reselecting the record — the
  pane cached sources/backlinks in @State, refreshed only by .task(id:
  memory.id), which never re-fires for an already-selected record (the book
  pane's auxKey anticipated exactly this — multi-field refetch key). Fix: the
  pane reads store-backed live caches — Core MemoryRepository.allSourcesByMemory
  + yieldingByMemory (one grouped query each, playthrough-scoped, mastered-only
  yields for display; +1 test incl. strict cross-playthrough scoping) — and the
  store refills both dicts on every reload, so the pane (and a reopened form's
  params) reflect saves the moment the sheet closes. The pane's only remaining
  editable cache is notes, which is its own explicit Save-note flow. Flagged but
  not changed: BookDetailView's quick-note path may leave ITS journal section
  stale the same way (auxKey unchanged by journal inserts) — same fix shape if
  reproduced. 98 tests green.
- 2026-10-06 (verified): source/link editors outside the Form work — the user
  confirms entering details + kinds + saving + reopening on build e82e184;
  MANUAL_TEST item ticked. Project lesson recorded for future UI: on this macOS
  build, TextFields must stay out of ForEach-in-grouped-Form contexts — plain
  VStack editor blocks + index bindings are the working pattern.
- 2026-10-06 (still dead after owning its row — ForEach+TextField ruled out
  wholesale): the user's db settles the mechanism questions: memory 33 carries
  Forge 2/Grail 2 created via the quick-add's AspectEditor — ForEach($array)
  pickers/steppers write through in grouped Forms; the old detail pane's add-row
  wrote sources incl. details — plain VStack + TextField works; three source-rows
  with details exist from early play. The only never-working combination is
  TextField inside ForEach (any arrangement — own-row, direct binding). Fix: the
  source/link editors moved OUT of the Form into a plain VStack below it
  (the old pane's working context), driven by index-based data ForEach with
  subscript bindings; Cancel/Save bar follows. 97 tests green.
- 2026-10-06 (detail field still dead after the binding fix, follow-up): with the
  hand-rolled binding gone the kind picker persisted (direct bindings write
  through) but the detail TextField still took no input — the remaining suspect
  is the grouped-Form row itself: TextFields sharing one row with a menu-style
  Picker stop accepting input on macOS. Restructured each source as two plain
  Form rows (kind + remove; detail field owning its own row — the one shape the
  app's other Form TextFields use successfully). MANUAL_TEST re-verify stays open.
- 2026-10-06 (source rows wouldn't take input, user report follow-up): on the edit
  sheet, the "How to obtain" detail TextField reverted every keystroke and the kind
  picker's changes didn't persist — the row's detail bound through a hand-rolled
  Binding(get:set:) built from the ForEach projected binding, which reverts under
  macOS grouped Forms. Fix: SourceRow carries a plain detailText (nil converted at
  load/save edges) and every row control binds directly through the collection
  binding — the same pattern as the (proven) AspectEditor steppers in the same
  Form. 97 tests green.
- 2026-10-06 (sources + yielding-links editing move into the edit sheet, user
  request): same pane pass — "How to obtain" and "Books that yield this" were still
  editing surfaces (add-source kind dropdown + detail field; "Link a book…" menu;
  per-row remove/unlink buttons). The detail pane now displays both read-only;
  editing moved to the Edit… form (MemoryFormView edit-mode sections): source rows
  (kind picker + detail + remove, stable local row ids) and yielding-book links,
  persisted on Save through new Core mechanics — MemoryRepository.setSources
  (replace-all, PK-dedupe), allYielding (any-status links vs the mastered-only
  display variant) and setYieldingBooks (two-way link sync; ids bound not
  interpolated; empty list clears). Store wrappers addSource/removeSource/linkBook/
  unlinkBook became dead and were removed. 97 tests green.
- 2026-10-06 (record-read toggle defaults ON, user request): after the form-master
  change the gap-fill flow exposed a rough default — MarkAsReadSheet seeded the
  "Mastering read" toggle OFF for already-mastered books (it was ON only when the
  book wasn't mastered yet). Now it defaults ON for every book: opening record-read
  on a mastered-from-form book presents as the mastering read (lessons picker
  visible; journal line "Mastered…"); a pure re-read is a deliberate flip-off.
  UI-only change; no Core logic moved.
- 2026-10-06 (form master counts as a read, user request): creating/editing a book
  with read status mastered wrote only the status text — no counter, no journal,
  yield unearned. Correct model (a player can catalogue a low-level mystery and
  beat it on the spot): the form master IS a read. Core BookReadTransitions
  .countsAsRead (tested: add-as-mastered, edit transition, mastered→mastered never
  double-counts); BooksStore add / update(original:with:) / setReadStatus wrap save
  + recordRead + a "Mastered…" journal entry in one transaction; both UI paths pass
  the current game day like the record-read sheet does. An imported yield link is
  earned automatically by the mastered book. Skills/lessons capture in the form
  deliberately parked (the record-read sheet remains their home). 96 tests green.


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
  Tests/BoHLibrarianCoreTests/ <- 101 tests on :memory: databases
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