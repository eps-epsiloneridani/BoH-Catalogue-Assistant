# BoH Librarian — hands-on test checklist

What this file is: the checks that need the **running app** (Unit tests only cover Core).
- Run the automated side: `cd app && swift test` — 101 tests green (as of 2026-10-06).
- The outstanding hands-on items are grouped by screen/flow below; tick as you go through
  them in the app.
- Work that's already been cleared by hand is a one-line record under **Cleared** at the
  bottom (the old per-session checklists were consolidated there on 2026-10-06).
- Anything surprising goes to the Scratch pad.

Last full hands-on pass: 2026-10-06 (Books screen, Memories screen, Reading Helper core
flows, imported-kind repair, earned-only lists).

## Outstanding hands-on — by screen

### Playthroughs & the manage sheet (Phase 3½)

- [X] Sidebar shows the playthrough switcher (person icon + the active run's name)
- [x] New Playthrough…: create a second run; Books/Memories screens are empty; footer
      name + counts changed; the previous run's rows are NOT visible
- [X] Switch back via the menu: everything recorded in the first run is intact
- [x] Manage…: rows left-justified (plain layout, not a grouped Form); rename sticks
      three ways — Return, blur (click elsewhere/another row), and type-then-click-OK —
      including renaming the **active** run, with the sidebar switcher label following;
      renaming to blank restores the stored name
- [x] Manage…: Load switches runs; delete refuses the active/only run; deleting a spare
      run works after confirmation
- [x] Book form: "Difficulty known" toggle + stepper record **without** a mystery
      principle; list shows "difficulty N" when the principle isn't recorded; sort by
      Difficulty works

### Books (the form-master addition)

- [x] Add a book with Read status = Mastered directly — from the form AND via the
      detail's status picker: the row shows the green mastered tick, the journal
      gains a "Mastered …" entry, Times read is 1; saving that still-mastered book
      again does NOT double-count

### Reading Helper (Phase 4 + the filter/sort feature)

- [x] Helper toolbar gains Filter / Mystery / Sort menus: Filter = All, Unread,
      Uncatalogued, Catalogued, Mastered, Contaminated; Mystery = "Any mystery" + the 13
      principles (books with no recorded mystery drop out while a filter is active);
      Sort = Status (unread-first, the default), Title, Difficulty (hardest first),
      **Easiest first** (ascending, unknown last), Recently added
- [x] Spoiler posture spot-check on an unmastered imported book: candidates only list
      memories you've earned; the "Always yields" panel appears only for mastered
      books; the Books screen's detail for that book shows "revealed by mastering the
      book" instead of the yield name
- [ ] Record read on an unmastered imported book — the gained redesign (user
      request 2026-10-06): "Memory gained" lists ONLY earned memories by name; the
      book’s imported-but-unearned yield appears as a placeholder — "Unrevealed
      memory — this read earns it" — selectable without revealing the name;
      selecting + recording earns the memory (Memories list, real journal name,
      pane updates instantly). The journal CHIPS and the journal edit sheet’s
      memory picker show the same placeholder for unearned links; a non-mastering
      gain masks the journal text as "unrevealed memory" — no leak anywhere.

### Skills & Journal (Phase 5)

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
- [ ] Empty journal (fresh run or new playthrough): the quick-capture box shows above
      the "Nothing in the journal yet" card; "Note today's finding" focuses the box;
      ⌘⇧J from another section lands there focused; the first note flips the screen
      to the timeline
- [ ] Day headers: entries group when the in-game day changes; entries without a day
      group under "No in-game day noted"
- [ ] Link chips appear on entries created via record-read (book + memory chips); the
      edit sheet can add/change/remove all three links and edit text/day
- [ ] Journal search narrows the list; delete removes an entry immediately (no
      confirmation — notes are trivially re-typed)
- [ ] Footer skills/journal counts update live

### Save import (+ the manage-sheet integration)

- [ ] Manage Playthroughs sheet: "Import from Save…" button and a Cancel/OK bar at the
      bottom (both just close; changes apply immediately — including renames committed
      on blur/OK)
- [ ] Import sheet lists AUTOSAVE.json with its modified date, game version, and
      book/skill counts
- [ ] Import into a new playthrough: name defaults sensibly, import completes in a few
      seconds, summary alert shows counts, and the app switches to the imported
      playthrough with populated Books/Memories/Skills/Journal screens
- [ ] Spot-check against the game: a mastered book shows difficulty + principle badge +
      yielded memory; book kinds read correctly (bound books are plain "Book" — no sort
      of "Record" badge — since the codex/record mapping fix; the phonograph records and
      scrolls in your library still badge as Record/Scroll); a contaminated book shows
      its contamination; skills show levels; the Journal records the import
- [ ] Import into the *current* playthrough: no duplicates, existing notes untouched
- [ ] With the game folder absent (another Mac): friendly empty state mentioning the
      parked file picker
- [ ] Location stamping (2026-10-06): re-import the autosave into a playthrough with
      unrecorded locations — Books rows show "Library — shelf D.3"-style subtitles from
      the save's sphere chain (room > shelf/slot; desks/scroll-slots humanized); your own
      recorded locations are never overwritten; in-transit items read "portage1 — …"/
      "purchases.europe — …"
- [ ] **Import into "Playthrough 2" (the reported regression):** import the autosave
      into a second run — it completes with its own copy of books/skills/memories
      (per-playthrough name uniqueness is in force since migration 006; the db is at
      schema v7), and "First playthrough" keeps its records untouched

### Packaged app

- [ ] Double-click `dist/BoH Librarian.app` from Finder — window opens, no terminal
- [ ] Footer shows the Application Support db path (not the repo's `../Boh.db`)
- [ ] Library starts empty; Manage Playthroughs → Import from Save… pulls in your
      AUTOSAVE — the full end-to-end flow with real data
- [ ] Quit via ⌘Q; relaunch — everything you imported/recorded persists
- [ ] Keep in Dock (right-click → Options) and/or drag to /Applications if you want it
      permanent (the db lives in Application Support either way)

Note: the packaged app uses its **own** database, separate from the repo's `Boh.db`
(D7 — the repo db stays git-versioned for `swift run` development). To test the
packaged app against the repo db: `BOH_DB_PATH=Boh.db open "dist/BoH Librarian.app"`
from the repo root — but the cleaner path is the Application Support one.

### Accessibility (the a11y pass)

- [ ] VoiceOver (⌘F5) walk: Books screen rows announce as a single stop; tab/VO-walk
      the record-read sheet end to end; Playthroughs manager sheet
- [ ] Memories screen: a memory's backlink and source rows reachable, and the Edit…
      sheet's editors announce ("Remove source", "Unlink …")
- [ ] Appearance: Dark Mode — principle badges stay readable in both modes; Increase
      Contrast on — still readable
- [ ] Dynamic Type at the largest accessibility size: Books rows + the BookFormView
      sheet remain usable (wrapping, not clipping)
- [ ] Accessibility Inspector (Xcode) on the Books screen: no unlabeled elements

## Verified automatically (the unit suite, 101 green — a map, not a chore list)

- **Books**: filtering/sorting/search vocabulary (Title, Difficulty, Status, Easiest
  first, Recently added; status + mystery filters incl. dropping unrecorded mysteries);
  form-master-counts-as-a-read rule (never double-counts); record-read journal text;
  per-book caches (journal entries newest-first, lesson names via the Skills join).
- **Memories**: earned-only visibility (list, Helper candidates, mastered-only
  backlinks — flips both directions at mastery); source/link editor mechanics
  (`setSources` replace-all + dedupe, `allYielding`, `setYieldingBooks` two-way sync);
  per-memory caches (sources + yields; playthrough-scoped, cross-playthrough isolated).
- **Record-read sheet**: quick-add reuses same-(name,kind) imported yields
  (`insertOrReuse`); "Memory used" is earned-only while "gained" sees everything.
- **Import**: lenient JSON (UTF-16/BOM, control chars, trailing commas); kind mapping
  (`record.phonograph` → record; codex/tablet → book; scroll; film); fill-empty upsert;
  scanner (RootPopulationCommand-only, size-capped); real-game integration.
- **Migrations**: 001–007 apply idempotently; 005 scoping; 006 rebuild preserves ids
  and scopes uniques; 007 repairs imported kinds by import fingerprint.
- **Security hardening**: save scanner skips oversized files.
- **Accessibility (automated)**: badge text meets WCAG AA 4.5:1 in BOTH appearance
  modes for all 13 seeded tints (`ColorMath`); rows/badges announce as single crafted
  elements; icon-only controls labelled, decorations hidden.

## Cleared by hand (record)

- **2026-10-06 — Phase 2, Books screen core**: add/find/search/filter/sort, read
  counters (1→2 verified on the live db), record-read incl. "New memory…" (quick-add
  reuse cited: memory "Persistent"), notes editor, edit preserves counters, delete
  leaves journal text, footer counts.
- **2026-10-06 — Phase 3, Memories screen core**: list/badges/icons, add-memory sheet,
  filter/sort menus, read-only detail panes (aspects, sources, yielding books) with
  Edit…-sheet editors persisting across relaunch, memory deletion, footer counts,
  edit-sheet displays updating immediately (live caches).
- **2026-10-06 — Phase 4, Reading Helper core**: picker/selection, requirement panel,
  earned candidates, preselected record-read, near-misses + reach line, skill
  contributions (+L+1 / +L), mastered re-read panel, books-screen agreement.
- **2026-10-06 — Imported-kind repair**: relaunch migrate to v7 verified in-db (82
  flips, scrolls + genuine records intact); badge spot-checks on Books screen.
- **2026-10-06 — Earned-only lists**: Memories list shortened to earned/hand-made;
  quick-add reuse verified; edit-sheet sources verified (build e82e184).
- **2026-10-06 — Manage sheet + toggles**: "Mastering read" defaults ON (incl.
  mastered-from-form gap-fill); edit-sheet save→pane displays update immediately.

## Scratch pad

Anything noticed while testing (oddities, papercuts, ideas):
<!-- e.g. 2026-10-05: record-read sheet should probably remember the last used game day… -->