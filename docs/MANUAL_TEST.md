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
- [ ] Record read…: sheet on an uncatalogued book defaults to "Mastering read",
      on a mastered book says "recording a re-read"
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
- [ ] Day headers: entries group when the in-game day changes; entries without a day
      group under "No in-game day noted"
- [ ] Link chips appear on entries created via record-read (book + memory chips); the
      edit sheet can add/change/remove all three links and edit text/day
- [ ] Search narrows the journal; delete removes an entry immediately (no confirmation
      — notes are trivially re-typed)
- [ ] Footer skills/journal counts update live

## Scratch pad

Use this space for anything noticed while testing (oddities, papercuts, ideas):
<!-- e.g. 2026-10-05: record-read sheet should probably remember the last used game day… -->

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