-- 006_per_playthrough_unique_names.sql
-- Entity tables became playthrough-scoped in 005, but their 001-era UNIQUE
-- constraints stayed table-global:
--
--   * Skills.name            UNIQUE            (001)
--   * Memories (name, kind)  UNIQUE (name,kind) (001)
--
-- With two playthroughs, both forbid the same game entity in a second saved
-- game: importing the autosave into "Playthrough 2" failed with
-- `UNIQUE constraint failed: Skills.name` for exactly this reason (the same
-- names are recorded in "First playthrough"). In-game names stay unique *within*
-- one playthrough — that constraint moves to (name, playthrough_id).
--
-- Books.title was never unique (copies) and Principles/Languages are unscoped
-- game-structure lookups — both keep their constraints as-is.
--
-- Canonical table rebuild (sqlite.org/lang_altertable.html §8): the rowids (and
-- thus every FK reference from Books/Memories/Journal/BookLessons/MemoryAspects/
-- MemorySources) are preserved by copying ids verbatim. PRAGMA foreign_keys is a
-- no-op inside a transaction, so it is set before this file's own BEGIN —
-- sqlite3_exec (app) and migrate.sh both run the file statement-by-statement.

PRAGMA foreign_keys = OFF;

BEGIN;

-- Skills --------------------------------------------------------------------
CREATE TABLE Skills_rebuilt (
  id                     INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
  name                   TEXT    NOT NULL,         -- exact in-game name; unique per playthrough
  is_language            INTEGER NOT NULL DEFAULT 0,       -- exotic languages are skills
  primary_principle_id   INTEGER REFERENCES Principles(id) ON DELETE SET NULL,
  secondary_principle_id INTEGER REFERENCES Principles(id) ON DELETE SET NULL,
  level                  INTEGER CHECK (level IS NULL OR (level BETWEEN 1 AND 9)),
  wisdom                 TEXT,    -- Tree of Wisdoms branch once committed
  element                TEXT,    -- Element of the Soul gained by that commitment
  notes                  TEXT,
  created_at             TEXT    NOT NULL DEFAULT (datetime('now')),
  updated_at             TEXT    NOT NULL DEFAULT (datetime('now')),
  playthrough_id         INTEGER REFERENCES Playthroughs(id) ON DELETE CASCADE
);

INSERT INTO Skills_rebuilt
  (id, name, is_language, primary_principle_id, secondary_principle_id,
   level, wisdom, element, notes, created_at, updated_at, playthrough_id)
SELECT id, name, is_language, primary_principle_id, secondary_principle_id,
       level, wisdom, element, notes, created_at, updated_at, playthrough_id
FROM Skills;

DROP TABLE Skills;
ALTER TABLE Skills_rebuilt RENAME TO Skills;

CREATE INDEX idx_skills_playthrough ON Skills (playthrough_id);
CREATE UNIQUE INDEX idx_skills_name_playthrough ON Skills (name, playthrough_id);

-- Memories ------------------------------------------------------------------
CREATE TABLE Memories_rebuilt (
  id         INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
  name       TEXT    NOT NULL,
  kind       TEXT    NOT NULL DEFAULT 'memory',   -- memory | weather | numen
  persistent INTEGER NOT NULL DEFAULT 0,          -- survives dawn (not Numa)
  notes      TEXT,
  created_at TEXT    NOT NULL DEFAULT (datetime('now')),
  updated_at TEXT    NOT NULL DEFAULT (datetime('now')),
  playthrough_id INTEGER REFERENCES Playthroughs(id) ON DELETE CASCADE
);

INSERT INTO Memories_rebuilt
  (id, name, kind, persistent, notes, created_at, updated_at, playthrough_id)
SELECT id, name, kind, persistent, notes, created_at, updated_at, playthrough_id
FROM Memories;

DROP TABLE Memories;
ALTER TABLE Memories_rebuilt RENAME TO Memories;

CREATE INDEX idx_memories_playthrough ON Memories (playthrough_id);
CREATE UNIQUE INDEX idx_memories_name_kind_playthrough ON Memories (name, kind, playthrough_id);

PRAGMA user_version = 6;

COMMIT;

PRAGMA foreign_keys = ON;