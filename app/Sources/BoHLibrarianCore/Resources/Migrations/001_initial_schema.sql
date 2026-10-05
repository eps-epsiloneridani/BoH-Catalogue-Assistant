-- 001_initial_schema.sql — BoH Librarian schema v2
--
-- Replaces the legacy two-table schema (Books, Memories) that existed before this
-- project had a plan. Both legacy tables were verified EMPTY on 2026-10-05 before this
-- migration was authored, and scripts/migrate.sh re-checks (refusing to apply this file
-- if either table contains rows). The legacy DDL lives in git history and in
-- docs/DATABASE.md. Design rationale: docs/DATABASE.md, docs/DECISIONS.md (D3, D8).
--
-- Conventions: snake_case; TEXT timestamps in UTC via datetime('now'); enums stored as
-- TEXT with value sets documented in docs/DATABASE.md (GUI constrains entry; CHECKs kept
-- minimal so value sets can grow without table rebuilds).

PRAGMA foreign_keys = ON;

BEGIN;

DROP TABLE IF EXISTS Books;      -- legacy table (was empty)
DROP TABLE IF EXISTS Memories;   -- legacy table (was empty)

----------------------------------------------------------------------- lookup

CREATE TABLE Principles (
  id          INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
  name        TEXT    NOT NULL UNIQUE,            -- exact in-game name
  sort_order  INTEGER NOT NULL UNIQUE,
  color       TEXT,                               -- hex, UI badge color (editable)
  notes       TEXT,
  created_at  TEXT    NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE Languages (
  id         INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
  name       TEXT    NOT NULL UNIQUE,             -- exact in-game name
  native     INTEGER NOT NULL DEFAULT 0,          -- 1 = known from game start
  notes      TEXT,
  created_at TEXT    NOT NULL DEFAULT (datetime('now'))
);

----------------------------------------------------------------------- memories

CREATE TABLE Memories (
  id         INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
  name       TEXT    NOT NULL,
  kind       TEXT    NOT NULL DEFAULT 'memory',   -- memory | weather | numen
  persistent INTEGER NOT NULL DEFAULT 0,          -- survives dawn (not Numa)
  notes      TEXT,
  created_at TEXT    NOT NULL DEFAULT (datetime('now')),
  updated_at TEXT    NOT NULL DEFAULT (datetime('now')),
  UNIQUE (name, kind)                             -- a Weather and a Memory may share a name
);

CREATE TABLE MemoryAspects (
  memory_id    INTEGER NOT NULL REFERENCES Memories(id)   ON DELETE CASCADE,
  principle_id INTEGER NOT NULL REFERENCES Principles(id) ON DELETE RESTRICT,
  level        INTEGER NOT NULL CHECK (level > 0),
  PRIMARY KEY (memory_id, principle_id)
);

CREATE TABLE MemorySources (
  memory_id INTEGER NOT NULL REFERENCES Memories(id) ON DELETE CASCADE,
  kind      TEXT    NOT NULL,  -- re-read book | first read | weather | talk | consider |
                               -- consume | craft | gather | numa | other
  detail    TEXT,              -- free text, e.g. 'Talk with the Rector (17%)'
  PRIMARY KEY (memory_id, kind, detail)
);

----------------------------------------------------------------------- skills

CREATE TABLE Skills (
  id                     INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
  name                   TEXT    NOT NULL UNIQUE,          -- exact in-game name
  is_language            INTEGER NOT NULL DEFAULT 0,       -- exotic languages are skills
  primary_principle_id   INTEGER REFERENCES Principles(id) ON DELETE SET NULL,
  secondary_principle_id INTEGER REFERENCES Principles(id) ON DELETE SET NULL,
  level                  INTEGER CHECK (level IS NULL OR (level BETWEEN 1 AND 9)),
  wisdom                 TEXT,    -- Tree of Wisdoms branch once committed
  element                TEXT,    -- Element of the Soul gained by that commitment
  notes                  TEXT,
  created_at             TEXT    NOT NULL DEFAULT (datetime('now')),
  updated_at             TEXT    NOT NULL DEFAULT (datetime('now'))
);

----------------------------------------------------------------------- books

CREATE TABLE Books (
  id                   INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
  title                TEXT    NOT NULL,                   -- not unique: copies allowed
  set_name             TEXT,    -- series, e.g. 'The Locksmith's Dream'
  volume               TEXT,    -- 'vol. 2', 'Second Edition', ...
  book_kind            TEXT    NOT NULL DEFAULT 'book',    -- book | scroll | film | record
  language_id          INTEGER REFERENCES Languages(id)   ON DELETE SET NULL,
  mystery_principle_id INTEGER REFERENCES Principles(id) ON DELETE SET NULL,
  mystery_level        INTEGER CHECK (mystery_level IS NULL OR mystery_level > 0),
  read_status          TEXT    NOT NULL DEFAULT 'uncatalogued', -- uncatalogued|catalogued|mastered
  contamination        TEXT,    -- none | curse | theoplasmic | infestation | corruption
  location             TEXT,    -- room / shelf / source
  times_read           INTEGER NOT NULL DEFAULT 0,
  first_read_at        TEXT,
  last_read_at         TEXT,
  lessons              INTEGER, -- Lessons granted on first read (1-3)
  yielded_memory_id    INTEGER REFERENCES Memories(id) ON DELETE SET NULL,
  notes                TEXT,
  created_at           TEXT    NOT NULL DEFAULT (datetime('now')),
  updated_at           TEXT    NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE BookLessons (
  book_id  INTEGER NOT NULL REFERENCES Books(id)  ON DELETE CASCADE,
  skill_id INTEGER NOT NULL REFERENCES Skills(id) ON DELETE CASCADE,
  amount   INTEGER NOT NULL DEFAULT 1,           -- 'x2', 'x3'
  PRIMARY KEY (book_id, skill_id)
);

----------------------------------------------------------------------- journal

CREATE TABLE Journal (
  id        INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
  logged_at TEXT    NOT NULL DEFAULT (datetime('now')),
  game_day  TEXT,            -- free text, e.g. 'Year 2, Autumn, day 3'
  entry     TEXT    NOT NULL,
  book_id   INTEGER REFERENCES Books(id)   ON DELETE SET NULL,
  memory_id INTEGER REFERENCES Memories(id) ON DELETE SET NULL,
  skill_id  INTEGER REFERENCES Skills(id)  ON DELETE SET NULL
);

----------------------------------------------------------------------- indexes

CREATE INDEX idx_memory_aspects_principle ON MemoryAspects (principle_id, level);
CREATE INDEX idx_books_read_status        ON Books (read_status);
CREATE INDEX idx_books_mystery           ON Books (mystery_principle_id, mystery_level);
CREATE INDEX idx_journal_logged_at       ON Journal (logged_at);
CREATE INDEX idx_memory_sources_memory    ON MemorySources (memory_id);
CREATE INDEX idx_book_lessons_skill       ON BookLessons (skill_id);

PRAGMA user_version = 1;
COMMIT;