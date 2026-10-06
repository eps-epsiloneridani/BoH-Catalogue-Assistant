# Manual test checklist

Run through the relevant phase's checklist after each UI change. Items backed by
unit tests are pre-ticked here; the rest need eyes and hands on the running app
(`cd app && swift run`). Add a phase section as each screen lands, and tick items
during your first play session with that build.

> **Phase 3 note (2026-10-05):** a Phase 2 editing slip meant the Books screen was
> never actually routed — the app showed the placeholder while `BooksScreen` sat
> unreachable in the binary. Fixed in Phase 3 (routing verified by grep + build);
> the Phase 2 hands-on items below now genuinely apply.

## Phase 2 — Books screen (2026-10-05)

### Verified automatically (unit tests — `swift test`, 48 green)
- [x] List filtering: search matches title/set/volume/location/notes, principle name,
      language name; case-insensitive; trimmed empty search shows everything
- [x] Filter chips logic: unread/uncatalogued/catalogued/mastered; contaminated shows
      only checked-present contamination (not "none", not unknown)
- [x] Sorts: title (localized), mystery (descending, unknown last), status
      (unread first), recently added
- [x] Journal text composition for mastering reads and re-reads (lessons only on
      mastering; empty optionals stay silent)
- [x] Repository writes used by the flows: `setYieldedMemory`, `setLessonsCount`,
      read counters, ON DELETE SET NULL for memory links
- [x] App launches against the real `Boh.db` and stays alive (smoke run)

### Needs hands — tick during your next session with the app
- [ ] Empty state: "No books recorded yet" with Add button, when db has no books
- [ ] Add Book (⌘N): sheet opens; Save disabled until title entered; mystery
      principle optional; level stepper only visible when a principle is chosen
- [ ] New book appears in list, auto-selected, detail shows everything entered
- [ ] Search field narrows the list as you type; filter and sort menus work
- [ ] Detail: mystery badge shows the principle's seeded colour
- [ ] Language "known" hint: native languages show a green check; an exotic
      language (e.g. Fucine) shows orange until you record learning it (Phase 5)
- [ ] Status segmented control updates instantly and survives app relaunch
- [x] Record read…: the "Mastering read" toggle defaults ON for every book —
      including mastered-from-form gap-fills (user request 2026-10-06); flip off
      deliberately for a pure re-read of an already-mastered book
- [ ] Record read with "New memory…": name + aspects create the memory, the book
      links to it, and the Journal entry reads
      `Mastered “…” … Memory gained: …`
- [ ] Record read twice on the same book: Times read goes 1 → 2
- [ ] Quick journal note from detail: press return, entry appears with day/time
- [ ] Notes editor: Save appears on change; Revert restores; note persists
- [ ] Edit… changes fields; read counters survive an edit
- [ ] Delete Book…: confirmation appears; book gone; its journal entries remain
      in the Journal table (link cleared) — verify via `sqlite3 Boh.db`
- [ ] Footer counts update after adds/deletes

## Phase 3 — Memories screen (2026-10-05)

### Verified automatically (unit tests — `swift test`, 58 green)
- [x] Search matches name, kind, notes, aspect text (e.g. "knock 4" finds Curious
      Hunch); trimmed empty search shows everything
- [x] Principle filter; with a minimum level (Rose ≥ 5 → only the Numen); minimum
      level alone uses the memory's highest aspect
- [x] Sorts: name; level (filtered principle first, highest aspect otherwise);
      kind (numina → weather → memories); recently added

### Needs hands — tick during your next session with the app
- [ ] Sidebar → Memories: list shows recorded memories with colored aspect badges;
      weather has a cloud icon, numina a star, persistent an orange ∞
- [x] Edit… sheet "How to obtain": Add source → type a detail, change the kind
      dropdown, press Save, reopen — rows persist with the chosen kind and detail
      (verified by the user on build e82e184 — the fix that moved the editors out
      of the grouped Form)
- [x] Memory detail pane displays READ-ONLY "How to obtain" and "Books that
      yield this" too (user request 2026-10-06, same pass as the aspects fix):
      no add-source dropdown, no "Link a book…" menu, no per-row remove buttons —
      editing lives in the Edit… sheet, which gained both editors
      (sources rows + yielding-book link sync on save)
- [x] Memory detail pane displays aspects READ-ONLY (as badges, no pickers/
      steppers/dropdowns — user report 2026-10-06: the pane embedded the editor);
      changes persist via the Edit… sheet instead
- [ ] Add Memory (⌘N): sheet with name/kind/persistent/aspects; Save disabled without
      a name; rows without a principle are skipped on save
- [ ] Principle + level menus filter the list live; sort menu changes the order
- [ ] Detail: aspect editor persists immediately — pick a principle, change a
      level, remove a row, add a row; relaunch and the aspects are still right
- [ ] Sources: add (kind menu + detail text) and remove persist across relaunch
- [ ] “Books that yield this”: a book recorded via “Record read…” shows up as a
      backlink here; Link a book… adds one manually; unlink removes it
- [ ] Deleting a memory: confirmation; backlinked books lose the link (visible on
      the Books screen detail as “not recorded”); journal entries keep their text
- [ ] Footer memory count updates live as memories are added/deleted

## Phase 3½ — Playthroughs + difficulty (2026-10-06)

### Verified automatically (unit tests — `swift test`, 62 green)
- [x] Migration 004/005: difficulty column renamed; Playthroughs + Meta tables; one
      default playthrough seeded and marked active; idempotent re-runs
- [x] Scoping: books/memories/skills/journal inserted via one playthrough's repos are
      invisible to another's; cross-playthrough gets fail closed
- [x] Cascade: deleting a playthrough removes its books, memories, skills, journal and
      junction rows; other runs untouched
- [x] Meta round-trip: setActiveID/activeID survive a fresh connection

### Needs hands — tick during your next session with the app
- [ ] Sidebar shows the playthrough switcher (person icon + "First playthrough")
- [ ] New Playthrough…: create a second run; Books/Memories screens are empty; footer
      name + counts changed; "First playthrough" rows are NOT visible
- [ ] Switch back via the menu: everything recorded in run 1 is intact
- [ ] Manage…: rename a run (return key); Load switches; delete refuses the active/only
      run; deleting a spare run works after confirmation
- [ ] Book form: "Difficulty known" toggle + stepper recordable without a mystery
      principle; list shows "difficulty N" when the principle isn't recorded; sort by
      Difficulty works

## Phase 4 — Reading Helper (2026-10-06)

### Verified automatically (unit tests — `swift test`, 71 green)
- [x] Requirement sentence: "You need Rose 6."; graceful fallbacks when the principle
      or difficulty isn't recorded; canonical candidate/contribution queries already
      covered in RepositoryTests
- [x] Reach line: "Best recorded: memory 4 + skill 3 = 7 — enough, before souls, inks
      and tools"; "…N short…" when under; "Nothing recorded yet reaches for it."; nil
      without a difficulty

### Needs hands — tick during your next session with the app
- [ ] Helper opens with the first unread book selected; picker lists unread first, then
      mastered; search narrows by title/set/location
- [ ] Requirement panel: badge + "You need Rose 6."; language row shows the
      known/not-learned hint; contamination line appears for cursed books; films/records
      get the projector/phonograph hint
- [ ] Record a memory with a matching aspect on the Memories screen, then pick the book
      in the helper — it appears under "Memories that satisfy it", best first
- [ ] Tap a satisfying memory: the record-read sheet opens with that memory preselected
      as "Memory used"
- [ ] With no satisfying memory: near-misses show under "Closest recorded memories"
      with "not enough alone", and the reach line says how far short you are
- [ ] Skills section: a level-L skill shows +L+1 (primary) or +L (secondary)
- [ ] Mastered books show the re-read panel with the yielded memory instead of candidates
- [ ] Recording from the helper: book status/counters and Journal update; the Books
      screen agrees afterwards

## Phase 5 — Skills & Journal screens (2026-10-06)

### Verified automatically (unit tests — `swift test`, 78 green)
- [x] Skill list: search over name/wisdom/element/notes; principle filter matches
      primary OR secondary; language filter separates languages from skills
- [x] Sorts: name; level descending with unlevelled skills last; recently added
- [x] Contribution math: level 1 → 2 primary / 1 secondary; level 9 → 10 / 9

### Needs hands — tick during your next session with the app
- [ ] Skills screen opens empty-state; Add Skill records name + principles + level 1;
      the row shows contribution badges (e.g. Sky 2 · Rose 1) and "level 1"
- [ ] Level stepper in detail: +/− persists immediately and updates the badges;
      runs 1–9 only
- [ ] Language toggle in the form: languages get the globe icon and appear under the
      "Languages" kind filter — and the Books screen's language-known hint turns green
      for a language recorded this way
- [ ] Principle filter + sort menus work; search matches wisdom/notes
- [ ] Wisdom/Element fields: type + return (or Save) persists; shows in the detail
      header captions
- [ ] Journal: ⌘⇧J (or the Go menu) jumps to the journal with quick-add focused; a
      note typed + return lands at the top under the current in-game day
- [ ] Empty journal (fresh run or new playthrough): the quick-capture box shows
      above the "Nothing in the journal yet" card; "Note today's finding" focuses
      the box; ⌘⇧J from another section lands there focused; the first note flips
      the screen to the timeline (fixed: the new-entry button used to do nothing
      on an empty journal)
- [ ] Day headers: entries group when the in-game day changes; entries without a day
      group under "No in-game day noted"
- [ ] Link chips appear on entries created via record-read (book + memory chips); the
      edit sheet can add/change/remove all three links and edit text/day
- [ ] Search narrows the journal; delete removes an entry immediately (no confirmation
      — notes are trivially re-typed)
- [ ] Footer skills/journal counts update live

## Save import + manager OK/Cancel (2026-10-06)

### Verified automatically (unit tests — `swift test`, 86 green)
- [x] Lenient JSON parser: trailing commas, raw control characters, UTF-16 with BOM,
      BOM-less UTF-8
- [x] Importer: books with difficulty/principle/language/contamination/read status,
      lessons junction (skill + count), yielded memories with aspects and
      persistence/numen flags; skills with level (skill:N mutation → N+1), wisdom +
      element commitments; `skill.language` marks languages; native languages match
      by name; uncatbooks and transient memories skipped; one Journal entry written
- [x] Fill-empty upsert: user-recorded notes/difficulty preserved, read state only
      upgrades, re-import doesn't duplicate
- [x] Scanner: only files with a `RootPopulationCommand` count as saves (achievements
      and config excluded); game version read leniently (compact or spaced JSON)
- [x] Real-game integration: the live AUTOSAVE imports into an in-memory db with
      non-empty human titles (skips cleanly if the game is uninstalled)
- [x] Second-playthrough import with identical entity names — the reported
      `UNIQUE constraint failed: Skills.name` (001-era table-global uniques; fixed by
      migration 006 rebuilding Skills/Memories to per-playthrough uniqueness); plus a
      data-bearing-v5 → v6 rebuild test (rows/ids preserved, duplicates still blocked
      within one playthrough)

### Needs hands — tick during your next session with the app
- [ ] Manage Playthroughs sheet: shows "Import from Save…" button and an OK/Cancel
      row at the bottom (both just close; changes apply immediately)
- [ ] Import sheet lists AUTOSAVE.json with its modified date, game version, and
      book/skill counts
- [ ] Import into a new playthrough: name defaults sensibly, import completes in a
      few seconds, summary alert shows counts, and the app switches to the imported
      playthrough with populated Books/Memories/Skills/Journal screens
- [ ] Spot-check against the game: a mastered book shows difficulty + principle badge
      + yielded memory; a contaminated one shows its contamination; skills show levels;
      the Journal entry records the import
- [ ] Import into the *current* playthrough: no duplicates, existing notes untouched
- [ ] With the game folder absent (if you ever test on another Mac): friendly
      empty state mentioning the parked file picker
- [ ] **Import into Playthrough 2 (the reported failure):** relaunch the app once
      so the existing db migrates v5 → v6 (footer shows schema v6), then import the
      autosave into “Playthrough 2” — it completes with its own copy of the
      books/skills/memories, and “First playthrough” keeps its records untouched.

## Imported book kinds repaired (2026-10-06)

### Verified automatically (unit tests — `swift test`, 87 green)
- [x] Mapping: fixture tome with `codex` imports as `.book`; `record.phonograph`
      as `.record`; `scroll` as `.scroll` (the old mapping turned every codex —
      i.e. nearly all books — into 'record')
- [x] Migration 007 on a copy of the live db: 82 of 84 mis-stamped rows flip
      `record → book`; both Tantras (scrolls) intact; manual entries — including
      two genuine phonograph records — untouched; `integrity_check` ok, FK check ok
- [x] Migration 007 unit proof on a data-bearing v6 db (fingerprints + controls)
- [x] Re-import is a no-op on kinds: the fill-empty upsert never writes `book_kind`

### Needs hands — tick on your next launch
- [x] Relaunch the app (the packaged one: rebuild `dist` from this commit first) —
      the app migrates its Application Support db v6 → v7 on launch (verified in-db:
      v7, 82 books / 2 scrolls / 0 stale 'record' rows)
- [x] Books screen: the books you imported (Journal of Thomas Dewulf, De Ratio
      Quercuum, …) no longer show "Record" badges; the two phonograph records
      (A Tower Rises, An Investigation of A Foundered Country) still do; the scrolls
      (both Tantras) still show "Scroll"
- [x] Switch playthroughs — both imported playthroughs read cleanly, lists intact

## Packaged app (2026-10-06)

### Verified automatically (the packaging smoke test)
- [x] `scripts/make-app.sh` builds release + wraps `dist/BoH Librarian.app`
      (bundle id, version 0.1.0 + git hash, ad-hoc signature)
- [x] Launched via `open` (Finder semantics): stays alive, gets Dock/⌘Tab presence
- [x] First launch creates `~/Library/Application Support/BoH Librarian/Boh.db` and
      migrates it to schema v5 **from the bundled migrations** with seeds
      (13 principles, "First playthrough" active)

### Needs hands — your first double-click run
- [ ] Double-click `dist/BoH Librarian.app` from Finder — window opens, no terminal
- [ ] Footer shows the Application Support db path (not the repo's `../Boh.db`)
- [ ] Library starts empty; Manage Playthroughs → Import from Save… pulls in your
      AUTOSAVE — the full end-to-end flow with real data
- [ ] Quit via ⌘Q; relaunch — everything you imported/recorded persists
- [ ] Keep in Dock (right-click → Options) if you like; drag to /Applications if you
      want it permanent (it's self-contained — the db lives in Application Support)

Note: the packaged app uses its **own** database, separate from the repo's `Boh.db`
(that's D7 — the repo db stays git-versioned for `swift run` development). If you
want the packaged app on the repo db instead, launch it once from the repo root:
`BOH_DB_PATH=Boh.db open "dist/BoH Librarian.app"` — but the cleaner test path is
letting it live in Application Support.

## Scratch pad

Use this space for anything noticed while testing (oddities, papercuts, ideas):
<!-- e.g. 2026-10-05: record-read sheet should probably remember the last used game day… -->

## Memory backlinks mastered-only (2026-10-06, user request)

### Verified automatically (unit tests — `swift test`, 87 green)
- [x] `MemoryRepository.booksYielding` returns only `mastered` books; a
      recorded-but-unread book silently gains backlink visibility at mastery
      (link survives, reappears — tested both ways)

### Needs hands
- [ ] After the next dist rebuild + /Applications swap: a memory you gained from a
      book — its Books-that-yield-this list shows that book; a memory linked to an
      unread book (e.g. via import) does not list it

## Accessibility (2026-10-06)

### Verified automatically (unit tests — `swift test`, 92 green)
- [x] Contrast: `ColorMathTests` asserts **all 13 seeded principle tints read
      ≥ 4.5:1 over their badge backdrop in BOTH appearance modes** (raw tints
      fail 13/13 in some mode); already-readable tints are left untouched
      (Edge/Scale/…, computed against macOS window-background approximations)
- [x] Icon-only controls carry accessibility labels (28 `Image(systemName:)`
      sites catalogued: buttons labeled, decorations hidden, state icons labeled)
- [x] Book rows and principle badges announce as one element with a crafted
      label (`title, Record, set, volume, location, Sky 4, Vak, mastered`)

### Needs hands — tick during your next session with the app
- [ ] VoiceOver (⌘F5) walk: Books screen — rows announce as a single stop;
      tab/VO-walk the record-read sheet end to end; Playthroughs manager sheet
- [ ] Memories screen: a memory's backlinks and source rows reachable and
      every button announces ("Remove source", "Unlink this book")
- [ ] Appearance: switch system to Dark Mode — principle badges stay readable
      in both modes; Increase Contrast setting on — still readable
- [ ] Dynamic Type at the largest a11y size: Books screen rows + BookFormView
      sheet remain usable (wrapping, not clipping)
- [ ] Accessibility Inspector (Xcode) on the Books screen: no unlabeled elements

## Earned-only memory list (2026-10-06, user request)

### Verified automatically (unit tests — `swift test`, 93 green)
- [x] `MemoryRepository.allKnown`: hand-created memories visible; a mastered
      yielding link earns visibility even when other yielding books are
      unmastered; all-unmastered yields hidden; mastering flips them in; the
      underlying table keeps every row (import/lookup paths untouched)
- [x] Footer memory count follows the list (reads the store)

### Needs hands
- [ ] Add a book with Read status = Mastered directly (from the form, and via the
      detail's status picker): the row shows the green mastered tick, the journal
      gains a "Mastered …" entry, and Times read is 1 — not a bare status stamp;
      editing that still-mastered book and saving again does NOT increment it
- [ ] Reading Helper on an unmastered imported book: candidate list only shows
      memories you've earned; its "Always yields" panel never runs for unmastered
      books (already gated); Books detail for that book shows "revealed by
      mastering the book" instead of the yield name
- [ ] Record read on an unmastered imported book: "Memory used" offers only earned
      memories, but "Memory gained → existing" can still find the book's imported
      yield — selecting it masters the link and the memory appears in the list
- [x] Open Memories on "Imported from AUTOSAVE": the list is far shorter than
      60+ created memories — only earned/hand-made ones show; master one of the
      unearned-yield books via Record read and watch its memory appear
- [x] Record-read quick-add on "Imported from AUTOSAVE": typing a memory name that
      already exists as an imported (hidden) yield REUSES it (user report
      2026-10-06: the old quick-add crashed on 006's per-playthrough uniqueness and
      aborted the whole read; covered by insertOrReuse + tests)
- [ ] A hidden memory's yield still shows via record-read on that book (the
      book detail/"Always yields" flow) — record-read is how the player learns it
