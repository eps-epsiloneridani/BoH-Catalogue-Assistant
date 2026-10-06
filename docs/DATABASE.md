# Database plan (`Boh.db`)

Authoritative DDL: `db/migrations/*.sql` (applied in order; `scripts/migrate.sh`).
This file explains the design. Tool: SQLite. FKs enforced (`PRAGMA foreign_keys=ON`
on every connection — the app and scripts both do this). Timestamps: UTC TEXT.

## What the legacy schema got wrong (and why v2 replaced it)

The pre-project schema (`Books`, `Memories` — kept in git history) had:

1. **One column per principle** on `Memories` (11 columns). The game has **13 principles**
   — it was missing **Lantern and Nectar**; and the set already changed once (Numa added
   Rose and Moon). Column-per-principle means an ALTER for every game update. (D3)
2. **Single-aspect assumptions**: `Books.Aspect`/`Difficulty` was fine, but nothing modelled
   languages, contamination, book types, lessons, or skills.
3. **No many-to-many**: aspects on memories (1–4 per card), sources per memory (many),
   lessons per book (many) all need junction tables.

The db was **empty** when migration 001 dropped the legacy tables (D8).

## Schema v2

### Principles — lookup, seeded (13)

| column | notes |
|---|---|
| `name` | exact in-game name, UNIQUE |
| `sort_order` | stable display order |
| `color` | hex badge color in the UI (editable seed defaults). Rendered as badge fill/border; badge **text color is derived per appearance mode** to hold WCAG AA 4.5:1 (the raw tints fail both modes for several hues) — `ColorMath.readableTextHex`, D12 |
| `notes` | one-line gloss |

### Languages — lookup, seeded (15)

| column | notes |
|---|---|
| `name` | exact in-game name, UNIQUE |
| `native` | 1 = known from game start (Latin, Greek, Sanskrit, Aramaic, Phrygian — verify in play) |

### Memories

| column | notes |
|---|---|
| `name`, `kind` | kind ∈ `memory` \| `weather` \| `numen` (documented set; UNIQUE (name, kind, **playthrough_id**) — a Weather and a Memory may share a name, e.g. "Storm"; the same name is allowed across playthroughs) |
| `persistent` | 1 = survives dawn (still wiped by Numa) |
| `notes` | free text |

- **`MemoryAspects`** (junction): `memory_id × principle_id → level` (level ≥ 1).
  A memory with no rows = aspects not yet recorded.
- **`MemorySources`** (junction): how it can be obtained. `kind` ∈ `re-read book` \|
  `first read` \| `weather` \| `talk` \| `consider` \| `consume` \| `craft` \| `gather` \|
  `numa` \| `other`; `detail` free text (e.g. `Talk with the Rector (17%)`). UNIQ over all
  three columns.

### Books

| column | notes |
|---|---|
| `title` | not UNIQUE — duplicate copies are allowed |
| `set_name`, `volume` | series ("The Locksmith's Dream") / edition ("vol. 2") |
| `book_kind` | `book` \| `scroll` \| `film` \| `record` |
| `language_id` | FK → Languages (NULL if none/unknown) |
| `mystery_principle_id`, `difficulty` | the reading challenge — the wiki's "Mastery Difficulty", recordable before mastering |
| `read_status` | `uncatalogued` → `catalogued` → `mastered` |
| `contamination` | `none` \| `curse` \| `theoplasmic` \| `infestation` \| `corruption` \| plus the game's own contamination names as save-imports carry them (`winkwell`, `witchworms` — more may follow as saves surface them; see docs/SAVE_IMPORT.md) |
| `location` | free text: room / shelf / "Oriflamme's" |
| `times_read`, `first_read_at`, `last_read_at` | read history |
| `lessons` | total Lessons granted on first read (1–3) |
| `yielded_memory_id` | FK → Memories: the memory this book always gives. Import and record-read stamp it even on unread books; **display** of "books that yield this" backlinks is mastered-only (UI spoiler policy — the player sees a book's yield once they've earned it) |

- **`BookLessons`** (junction): which skills' lessons the book teaches (`book_id × skill_id`,
  `amount` for ×2/×3). Optional detail beyond `Books.lessons`.

### Skills

| column | notes |
|---|---|
| `name` | UNIQUE per playthrough; exact in-game name |
| `is_language` | 1 for the 10 exotic languages (native languages don't need rows) |
| `primary_principle_id`, `secondary_principle_id` | level-1 skill = 2 primary / 1 secondary |
| `level` | current in-game level (1–9); NULL while unknown |
| `wisdom`, `element` | Tree of Wisdoms commitment (free text until a Wisdoms table is warranted) |

### Journal

Free-form findings log — the heart of "record your findings".
`logged_at` (auto), `game_day` free text ("Year 2, Autumn, day 3"), `entry` (required),
optional links `book_id` / `memory_id` / `skill_id` (ON DELETE SET NULL — the text survives).

### Indexes

`MemoryAspects(principle_id, level)` — the Reading Helper hot path;
`Books(read_status)`, `Books(mystery_principle_id, difficulty)`,
`Journal(logged_at)`, `MemorySources(memory_id)`, `BookLessons(skill_id)`.

## Canonical queries (kept here so the app and CLI agree)

**Reading Helper — memories that can open a book** (mystery principle `:pid`, level `:lvl`):

```sql
SELECT m.id, m.name, m.kind, ma.level
FROM Memories m
JOIN MemoryAspects ma ON ma.memory_id = m.id
WHERE ma.principle_id = :pid AND ma.level >= :lvl
ORDER BY ma.level DESC, m.name;
```

**Reading Helper — skill contribution** (a level-L skill contributes L+1 primary / L secondary):

```sql
SELECT s.name, s.level,
       CASE WHEN s.primary_principle_id = :pid THEN s.level + 1 ELSE s.level END AS contributes
FROM Skills s
WHERE s.is_language = 0 AND s.level IS NOT NULL
  AND (s.primary_principle_id = :pid OR s.secondary_principle_id = :pid)
ORDER BY contributes DESC;
```

**Best recorded memory per principle** (dashboard candidate):

```sql
SELECT p.name AS principle, MAX(ma.level) AS best
FROM MemoryAspects ma JOIN Principles p ON p.id = ma.principle_id
GROUP BY p.id ORDER BY p.sort_order;
```

## Migrations

- `db/migrations/NNN_description.sql`, numeric order, forward-only, each a complete
  transaction ending in its own `PRAGMA user_version = NNN`.
- `scripts/migrate.sh` applies pending files (refuses migration 001 if the legacy tables
  still contain rows — belt and braces). `--status` lists applied/pending.
- The app applies the same files on launch: `BOH_MIGRATIONS` → `./db/migrations` →
  `../db/migrations` → bundled resources copy (kept in sync via `scripts/sync-migrations.sh`).
- Seeds: `002` principles (13, with UI colors), `003` languages (15). **Nothing else is
  ever seeded** (D6).

### What migrations 004–007 changed

- **004 — difficulty:** `Books.mystery_level` renamed to `Books.difficulty` — the term the
  game community and wiki use ("Mastery Difficulty"). Deliberately **recordable
  independently of the mystery principle**: the player knows the number to beat from
  catalogue time even when the principle isn't noted.
- **005 — playthroughs:** BoH is run-based; findings don't carry over between Librarians.
  Adds:
  - `Playthroughs (id, name, created_at, notes)` — one row per saved game.
  - `Meta (key, value)` — `active_playthrough` holds the loaded playthrough's id.
  - Nullable `playthrough_id` FK columns (ON DELETE CASCADE) on `Books`, `Memories`,
    `Skills`, `Journal`, backfilled to a default playthrough. Nullability is a SQLite
    ADD COLUMN limitation: **the application enforces scoping** — every repository is
    constructed with a playthrough id; all list queries filter on it; all inserts stamp it.
    Update/delete by id are unscoped (ids always originate from scoped queries).
  - Principles/Languages are game structure — unscoped.
- **006 — per-playthrough unique names:** 005 scoped the entity *tables* but left
  001's table-global `UNIQUE` constraints — `Skills.name` and `Memories (name, kind)`
  — forbidding the same entity in two saved games (importing a save into a second
  playthrough failed with `UNIQUE constraint failed: Skills.name`). Rebuilds both
  tables (canonical ALTER-rebuild, ids preserved — FK references survive) and moves
  uniqueness to `UNIQUE (name, playthrough_id)` / `UNIQUE (name, kind, playthrough_id)`.
  Names remain unique **within** one playthrough. `Books.title` stays non-unique
  (copies); Principles/Languages keep global uniques (unscoped lookups).
  - Deleting a playthrough cascades its findings; the UI confirms and refuses to delete
    the active or only playthrough.
- **007 — imported book kinds repaired (data repair, one-off):** the save importer
  mapped the tomes.json aspect `codex` to `book_kind = 'record'` — but `codex` is the
  plain bound-book format in the game's data; phonograph records carry aspect
  `record.phonograph`. Both imports of 2026-10-06 stamped 41 of 42 books per
  playthrough as 'record'. The migration flips those rows back to 'book', keyed on
  the import fingerprints (`created_at` in ('2026-10-06 12:58:45', '2026-10-06
  13:19:42')) so manual entries — including genuinely-phonograph records — are left
  untouched; only `book_kind` (+ `updated_at`) change. The importer itself is fixed
  in the same commit, so no future import recreates the damage.

### Future schema notes

If `Meta` grows more keys (e.g. app settings), keep it typed-by-convention (values are
TEXT) and document each key here. See docs/DECISIONS.md D9/D10 for rationale.

## Future tables (add via new migrations when a need crystallises)

| Probable table | Trigger |
|---|---|
| `Rooms` (name, area, unlocked, requirement principle+level, notes) | player starts tracking room unlocks |
| `Workstations` + `Recipes` (inputs, result, challenge tier) | crafting note-taking begins |
| `Visitors` / `Incidents` | dialogue & incident outcomes need recording |
| `Wisdoms` lookup | Tree of Wisdoms progress becomes worth tracking |
| `Elements` (of the soul; level +/++/+++) | soul-evolution tracking |

Rule of thumb: don't create a table until the player has recorded the thing twice in
Journal free text and it hurts.

## Hygiene

- Default rollback journal (no WAL): transient `Boh.db-journal` is gitignored; a clean close
  leaves only `Boh.db` to commit.
- `scripts/dump-sql.sh` writes a human-readable dump to `snapshots/` (gitignored) for
  eyeballing diffs; the binary db itself *is* the backup story via git (D1).