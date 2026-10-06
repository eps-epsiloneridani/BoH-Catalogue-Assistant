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