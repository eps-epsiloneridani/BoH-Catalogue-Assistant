# Populating from a Book of Hours save — IMPLEMENTED (2026-10-06)

**Status: shipped.** Entry point: the Manage Playthroughs sheet → "Import from Save…",
which lists save games from the standard install path and imports into a new or the
current playthrough. Core implementation: `app/Sources/BoHLibrarianCore/SaveImport.swift`
(`SaveScanner`, `SaveImporter`, lenient-JSON parser); UI:
`app/Sources/BoHLibrarian/Views/Playthroughs/ImportFromSaveSheet.swift`. Tests:
`SaveImportTests` — including one that imports the *real* installed game's live save
into an in-memory db (skipped cleanly when the game isn't installed).

The python prototype `scripts/import-save-preview.py` remains as a handy read-only
preview for the terminal, but the app importer supersedes it for real use.

Everything below is the original investigation — kept as the reference for the
formats and the design rules the implementation follows.

## Where the data lives

| What | Path (this Mac) | Format |
|---|---|---|
| Active save | `~/Library/Application Support/Weather Factory/Book of Hours/AUTOSAVE.json` | plaintext JSON, ~7.5 MB |
| Stale/manual saves | `save.json`, `restart.json` (beta-era here) | same |
| Steam Cloud mirror | `~/Library/Application Support/Steam/userdata/<id>/1028310/remote/` | only `achievements.json` here — the real save is the file above |
| Game definitions | `…/Steam/steamapps/common/Book of Hours/OSX.app/Contents/Resources/Data/StreamingAssets/bhcontent/core/elements/*.json` | 2,035 element definitions across ~60 files |
| Modding reference | `StreamingAssets/MODDING_README.txt` | official: mods are the same JSON — the format is meant to be read |

## Save structure (Secret Histories engine)

`RootPopulationCommand.Spheres` → `Tokens` → `Payload`, recursive (rooms contain
spheres contain tokens). Element stacks (`ElementStackCreationCommand`) carry:

- `EntityId` — the element ID: `t.<book>`, `s.<skill>`, `uncatbook.<tier>`, `hours.*`…
- **Location**: the sphere chain — each sphere's `GoverningSphereSpec.Id` names the room
  (`Library`, `purchases.europe`, `portage1…`) and the nested shelf/slot sphere
  (`ShelfSpaceSphereD.3` = shelf D slot 3, `ScrollSlot.2`, `ShelfSpaceDeskMid`).
  Films/records ride the same structure. Import stamps `Books.location` as a
  humanized "Room — slot" label, fill-empty only.
- **`Defunct` tokens**: stale copies of moved items — skipped entirely (mutations
  and location never merge from a defunct copy).
- `Mutations` (String→Int) — the player state we need
- `Illuminations` (String→String) — dynamic text (TLG notes etc.)

Decoded state (verified against this save):

- **Books mastered**: `mastery.<principle>` mutation; its value equals the book's
  difficulty. Absent = catalogued but unread.
- **Contaminated books**: `contamination.<name>` (this save has `winkwell`,
  `witchworms` — our `Contamination` enum's four categories are coarser than the
  game's names; map to nearest category or widen the set at import time).
- **Skills**: `skill: N` mutation = level N+1. `wisdom.committed` + `w.<wisdom>` +
  `a.x<element>` = Tree of Wisdoms commitment and the Element gained
  (x→prefix code: xfet=Fet, xwis=Wist, xtri=Trist, xcho=Chor, xmet=Mettle,
  xere=Ereb, xhea=Health…). Note: the save holds duplicate skill stacks
  (committed and uncommitted copies) — dedupe by EntityId.
- **Uncatalogued books**: `uncatbook.<tier>` stacks (baronial/curia/nocturnal/solar/
  dawn). The save does *not* know which book each becomes — `DealersTable` draws at
  catalogue time. An importer should skip them (or record counts only).
- **Memories**: transient by design — the save only holds today's leftovers. Don't
  import; derive instead (below).

## Game-definition structure

`elements/*.json` files, dialect quirks that bit us (all handled in the prototype):

- **Encoding**: mixed — some UTF-16 with BOM, some UTF-8. Decode by BOM check;
  UTF-8 bytes "successfully" decode as UTF-16 garbage otherwise.
- **Lenient JSON**: raw control characters in strings (`strict=False`) and
  trailing commas before `]`/`}` (strip with regex). The game's parser tolerates both.
- Keys: `ID` in tomes.json, `id` in skills.json — check both.

Book definition (`tomes.json`, 281 tomes incl. records/films/tablets):

- `Label` — title
- `aspects.mystery.<principle>` — principle + **difficulty** in one
- `aspects.w.<language>` — written-in language (absent = one of the five native
  languages; the native five aren't skill elements — match by name)
- `aspects.r.<skill>` + `xtriggers.mastering.<principle>` (effect `level` = lesson count)
- `xtriggers.reading.<principle>` → `id` — **the memory every read yields**
- `aspects.period.<tier>` — deck it comes from; `manifestationtype` Book/Tablet/…
  (note: tablets exist in-game — our `BookKind` may need a `tablet` case).
  Kind markers in the 281 installed tomes (verified 2026-10-06): aspected
  `codex` = plain bound-book format (256/281); `scroll` = 8; actual records
  carry `record.phonograph` (8, all `manifestationtype: Book`). CORRECTION of the
  original investigation: an earlier note claimed "records appear as
  `codex`-aspected Books" — that misreading put `codex → .record` into the
  importer and stamped 41 of 42 imported books per playthrough as 'record'
  (repaired by migration 007; mapping fixed in the same commit).

Skill definition (`skills.json`, 73 = 63 skills + 10 exotic languages): `Label`,
base `aspects` (level 1 = primary 2 / secondary 1 — matches `SkillMath`),
`w.<wisdom>` ×2 = the two Tree options. Language skills: `s.<language>`.

Memory definitions are spread across files (`aspecteditems.json` for numina,
`music.json`, etc.): `Label` + `aspects` = principle levels for `MemoryAspects`
(ignore `boost.*` keys — those are crafting helpers). Numina carry `numen.*` ids;
regular book memories `mem.*`; some have family ids (`music.cheerful`).

## Import design (when picked up)

1. Parse the save + build the element index (prototype does both, ~150 lines).
2. Ask which playthrough receives the import (or offer to create one named after
   the Librarian). Never overwrite: match existing books by title within the
   playthrough; create missing; only *upgrade* fields that are empty (don't stomp
   user-recorded notes/locations).
3. Write per book: title, `mysteryPrincipleID`+`difficulty`, language (match our
   seeded Languages by name), `readStatus` (mastered if `mastery.*`), contamination
   (nearest category), lessons (`BookLessons` after creating the skill if needed),
   `yieldedMemoryID` (create the memory with its aspects + `numen`/`persistent`
   kind as appropriate).
4. Write per skill: name, level, isLanguage, wisdom, element. Languages known =
   exotic `s.<lang>` stacks present.
5. Skip: uncatbooks (or counts only), transient memories, DealersTable contents
   (spoilers — it would reveal which books remain in every deck).
6. Drop one Journal entry recording the import (date, save file, counts) and
   `git add Boh.db` before/after per the ground rules.

**Spoilers (D6):** importing your *own* save imports only what you've already
found — it does not violate the no-spoiler-seed policy. That policy still forbids
importing definitions for books *not* present in the save.

## Known unknowns for the implementer

- House of Light additions: `DLC_HOL_*.json` element files exist alongside core
  (plus `further/` and `edition/` folders) — index them too, but verify their save
  state shape matches.
- The `hours.*` stacks (the librarian's in-game "Book of Hours") and
  `journal.generic.start` — skip them; they're story scaffolding, not library books.
- `soph` aspect on tomes appears to mirror difficulty (both 10 on The Sun's
  Design) — use `mystery.*`, not `soph`.
- Language *level* requirements (if any beyond "known") — verify in play
  (already on the GAME_MECHANICS open-questions list).